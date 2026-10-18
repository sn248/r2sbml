/**
 * @file    convertReactions.cpp
 * @brief   Loads an SBML File and converts reactions to ODEs
 * @author  Sarah Keating
 * @author  Frank T. Bergmann
 * @author  Satyaprakash Nayak
 * https://stackoverflow.com/questions/41938120/how-can-i-translate-an-sbml-formula-to-program-code
 * https://groups.google.com/g/sbml-discuss/c/inS4Lzp3Ri8/m/XEL-aeiAAgAJ
 * <!--------------------------------------------------------------------------
 * This sample program is distributed under a different license than the rest
 * of libSBML.  This program uses the open-source MIT license, as follows:
 *
 * Copyright (c) 2013-2016 by the California Institute of Technology
 * (California, USA), the European Bioinformatics Institute (EMBL-EBI, UK)
 * and the University of Heidelberg (Germany), with support from the National
 * Institutes of Health (USA) under grant R01GM070923.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Neither the name of the California Institute of Technology (Caltech), nor
 * of the European Bioinformatics Institute (EMBL-EBI), nor of the University
 * of Heidelberg, nor the names of any contributors, may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * ------------------------------------------------------------------------ -->
 */

#include <Rcpp.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <chrono>
#include <ctime>
#include <sbml/SBMLTypes.h>
#include <sbml/conversion/ConversionProperties.h>
#include <sbml/SBMLTransforms.h>

#include "formulaToInfix.h"

using namespace std;
LIBSBML_CPP_NAMESPACE_USE

int writeFileR(SBMLDocument*, std::string);
int writeFileMrgsolve(SBMLDocument*, std::string);
int writeFileNlmixr2(SBMLDocument*, std::string);
int writeFileMatlab(SBMLDocument*, std::string);
int writeFileJulia(SBMLDocument*, std::string);
int writeFileUbiquity(SBMLDocument*, std::string);

// Starting value for a species, in the units the generated ODE integrates.
//
// A species carries initialAmount or initialConcentration, not both, and the
// unset one reads back as NaN.  Asking only for the amount therefore printed
// the literal `nan` for every concentration-based model -- which is not a
// number in R, C++ or rxode2 -- so take whichever attribute is actually set.
//
// Taking it unchanged is still wrong, though.  Unless hasOnlySubstanceUnits is
// set, an SBML species symbol denotes a *concentration*, and replaceReactions
// duly divides the rate rule by the compartment volume.  So the state being
// integrated is a concentration and an amount-valued initial condition has to
// be divided by that volume to match -- on sbmlsimple.xml (amount 5e-21 in a
// 1e-14 compartment) that is a factor of 1e14.  The reverse conversion applies
// to a hasOnlySubstanceUnits species given a concentration.
//
// Nothing about this is visible in the generated code: the model simply starts
// in the wrong place and integrates smoothly away from it.
static double initialCompartmentSize(const Model* model, const Compartment* c);

static double speciesInitialValue(const Model* model, const Species* species)
{
   const Compartment* c = model->getCompartment(species->getCompartment());

   // The volume at t = 0, which an assignment rule may set to something other
   // than the declared size.  An unsized zero-dimensional compartment has no
   // volume to convert through, and a size of 0 would only turn the conversion
   // into an inf or a NaN.  A sized 0-D one converts through its size, as its
   // reaction terms do (see exposeCompartmentSizes()).
   double volume = 1.0;
   if (c != NULL && (c->getSpatialDimensions() > 0 || c->isSetSize()))
   {
     const double size = initialCompartmentSize(model, c);
     if (std::isfinite(size) && size != 0.0) volume = size;
   }

   const bool wantsAmount = species->getHasOnlySubstanceUnits();

   if (species->isSetInitialConcentration())
   {
     const double conc = species->getInitialConcentration();
     return wantsAmount ? conc * volume : conc;
   }
   if (species->isSetInitialAmount())
   {
     const double amount = species->getInitialAmount();
     return wantsAmount ? amount : amount / volume;
   }
   return 0.0;
}

// Index of the rate rule governing species @p speciesId, or -1 when the
// species has none.  After the replaceReactions conversion every reaction has
// become a rate rule, so a species without one is held constant -- a boundary
// condition, or a species taking part in no reaction.
static int rateRuleFor(Model* model, const std::string& id)
{
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     const Rule* r = model->getRule(i);
     if (r->isRate() && r->getVariable() == id) return (int)i;
   }
   return -1;
}

// True when @p speciesId is the variable of an assignment rule.  Such a
// species is not integrated: its value follows from the rule at every time
// point, so it is emitted as a local inside the RHS rather than given a slot
// in the state vector.  Giving it one would leave the solver carrying a stale
// copy that never updates.
static bool hasAssignmentRule(Model* model, const std::string& speciesId)
{
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     const Rule* r = model->getRule(i);
     if (r->isAssignment() && r->getVariable() == speciesId) return true;
   }
   return false;
}

// Indices of the species that form the state vector, in model order.
//
// Every writer uses this to decide what it is integrating.  Walking the rules
// instead gets two cases wrong: a species with no rate rule never gets a
// derivative, and an algebraic rule has no variable at all, so it yields a
// derivative with an empty name.
static std::vector<int> integratedSpecies(Model* model)
{
   std::vector<int> states;
   for (unsigned int i = 0; i < model->getNumSpecies(); i++)
   {
     if (!hasAssignmentRule(model, model->getSpecies(i)->getIdAttribute()))
     {
       states.push_back((int)i);
     }
   }
   return states;
}

// --- compartments whose volume is not constant ----------------------------
//
// replaceReactions leaves two things out when a compartment varies. Its own
// rate rule never reaches the writers, so the volume would be emitted as a
// fixed number; and the species rate rules it produces carry only the reaction
// term. Since a species symbol is [S] = n/V,
//
//     d[S]/dt = (dn/dt)/V  -  [S] * (dV/dt)/V
//
// and libSBML supplies just the first half. Both the volume and the dilution
// have to be put back here, or the model integrates at a frozen volume and
// silently drifts away from the right answer.

// The size of a compartment.  A 0-dimensional compartment has none to give,
// and nothing reads it: the SBML Test Suite's 0-D cases put only
// hasOnlySubstanceUnits species inside one and never name it in a formula.
// So it is 1, as speciesInitialValue() already assumes.  Any other unset size
// stays NaN, for warnUndefinedValues() to report.
static double compartmentValue(const Compartment* c)
{
   if (c->isSetSize() || c->isSetVolume()) return c->getVolume();
   if (c->isSetSpatialDimensions() && c->getSpatialDimensionsAsDouble() == 0) return 1.0;
   return std::numeric_limits<double>::quiet_NaN();
}

// --- values an assignment rule gives at t = 0 ---------------------------------
//
// A compartment an assignment rule sets has that rule's value, not its
// declared size, and species initial amounts have to be converted through it.
// SBML Test Suite case 00143 declares size 5 under the rule `compartment = 1`,
// and every species started a factor of 5 too low.

// Values of the parameters and compartments at t = 0, including those the
// assignment rules set.  A rate-rule variable starts at its declared value
// (SBML Test Suite 00311-00318, whose volume rule reads one).  Species are left out on purpose: their values are in
// whichever units the file declares, not the units being integrated, so a rule
// that reads one is left unevaluated rather than evaluated wrongly.
//
// SBMLTransforms::evaluateASTNode() returns NaN for any name missing from the
// map, so "not evaluable" needs no separate test.
static std::map<std::string, double> valuesAtStart(const Model* model)
{
   std::map<std::string, double> known;
   for (unsigned int i = 0; i < model->getNumParameters(); i++)
   {
     const Parameter* p = model->getParameter(i);
     if (std::isnan(p->getValue()) || model->getAssignmentRule(p->getIdAttribute()) != NULL) continue;
     known[p->getIdAttribute()] = p->getValue();
   }
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     const Compartment* c = model->getCompartment(i);
     const double v = compartmentValue(c);
     if (std::isnan(v) || model->getAssignmentRule(c->getIdAttribute()) != NULL) continue;
     known[c->getIdAttribute()] = v;
   }

   // Assignment rules may read one another; SBML forbids cycles but not any
   // particular order, so sweep until a pass adds nothing.
   bool progress = true;
   while (progress)
   {
     progress = false;
     for (unsigned int i = 0; i < model->getNumRules(); i++)
     {
       const Rule* r = model->getRule(i);
       if (!r->isAssignment() || known.count(r->getVariable())) continue;
       if (model->getParameter(r->getVariable()) == NULL &&
           model->getCompartment(r->getVariable()) == NULL) continue;
       const double v = SBMLTransforms::evaluateASTNode(r->getMath(), known, model);
       if (std::isfinite(v))
       {
         known[r->getVariable()] = v;
         progress = true;
       }
     }
   }
   return known;
}

// Size of @p c at t = 0: its assignment rule's value when it has one and the
// rule can be evaluated, the declared size otherwise.  An initial assignment
// needs nothing here, since expandInitialAssignments has already written its
// value into the size.
static double initialCompartmentSize(const Model* model, const Compartment* c)
{
   const Rule* r = model->getRule(c->getIdAttribute());
   if (r == NULL || !r->isAssignment()) return compartmentValue(c);

   const std::map<std::string, double> known = valuesAtStart(model);
   const std::map<std::string, double>::const_iterator it = known.find(c->getIdAttribute());
   return it != known.end() ? it->second : compartmentValue(c);
}

// A state that is not a species: a compartment or a parameter carrying a rate
// rule.  replaceReactions leaves those rules in the rule list, but the state
// vector is built from species, so without this the variable is emitted as a
// fixed number and its rule silently discarded.
struct RateState
{
   std::string    id;
   double         initial;
   const ASTNode* rate;
   const char*    what;   // for the comment beside the derivative
};

// Compartments then parameters carrying a rate rule. They join the state
// vector *after* the species, so every existing state index -- and with it the
// MATLAB/Julia mass-matrix rows -- keeps its position.
static std::vector<RateState> integratedRateStates(Model* model)
{
   std::vector<RateState> extra;
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     const Compartment* c = model->getCompartment(i);
     const int rule = rateRuleFor(model, c->getIdAttribute());
     if (rule >= 0)
     {
       extra.push_back({c->getIdAttribute(), compartmentValue(c),
                        model->getRule(rule)->getMath(), "compartment volume"});
     }
   }
   for (unsigned int i = 0; i < model->getNumParameters(); i++)
   {
     const Parameter* p = model->getParameter(i);
     const int rule = rateRuleFor(model, p->getIdAttribute());
     if (rule >= 0)
     {
       extra.push_back({p->getIdAttribute(), p->getValue(),
                        model->getRule(rule)->getMath(), "parameter"});
     }
   }

   // A model whose every variable follows an assignment rule integrates
   // nothing, and an empty state vector is not something any of the six
   // solvers accepts (deSolve: "`y' must be numeric").  Its trajectory is still
   // well defined -- the rules evaluated over time -- so give the solver one
   // inert state to step and let the observed variables carry the answer.
   if (extra.empty() && integratedSpecies(model).empty())
   {
     static ASTNode* zero = NULL;
     if (zero == NULL)
     {
       zero = new ASTNode(AST_INTEGER);
       zero->setValue(0);
     }
     extra.push_back({"r2sbml_placeholder", 0.0, zero,
                      "placeholder state, not in the SBML model"});
   }
   return extra;
}

// The variables assignment rules set, in rule order: species, parameters,
// compartments, even stoichiometry ids.  Each is computed as a local inside
// the RHS, so none is in the solver's state vector, and without being
// returned explicitly its trajectory cannot be read from the solution.
static std::vector<std::string> observedVariables(Model* model)
{
   std::vector<std::string> vars;
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     const Rule* r = model->getRule(i);
     if (r->isAssignment()) vars.push_back(r->getVariable());
   }
   return vars;
}

// Indices of the parameters that stay constant parameters.  One with a rate
// rule is a state (above), and one with an assignment rule is recomputed in
// the RHS like an assignment-rule species.  Neither may also be passed in as a
// constant: the target either rejects the duplicate name (mrgsolve, ubiquity)
// or carries a value the rule never uses -- often none at all, since SBML lets
// such a parameter omit `value`, which used to be written out as `nan`.
static std::vector<int> constantParameters(Model* model)
{
   std::vector<int> params;
   for (unsigned int i = 0; i < model->getNumParameters(); i++)
   {
     const std::string id = model->getParameter(i)->getIdAttribute();
     if (rateRuleFor(model, id) < 0 && !hasAssignmentRule(model, id))
     {
       params.push_back((int)i);
     }
   }
   return params;
}

// --- values SBML leaves undefined -------------------------------------------
//
// An unset size or value reads back as NaN, and operator<< prints that as the
// literal `nan` -- a number in none of the six targets, so the generated code
// failed with "object 'nan' not found" or a compile error that says nothing
// about the model.

// A number as a target spells it, so a NaN or an infinity comes out as that
// target's spelling -- `NaN`/`Inf`, or `NAN`/`INFINITY` in C++ -- rather
// than the `nan`/`inf` operator<< writes, which none of them accepts.  SBML
// allows value="INF", so a parameter may legitimately be infinite.
struct Spelling
{
   const char* nan;
   const char* inf;
};

// R, rxode2, MATLAB and Julia agree; C++ has its own macros.  ubiquity
// copies an *expression* -- <I>, <Ad>, <IF> -- verbatim into the R and the C
// model it generates (its build script only rewrites SIMINT_* calls), and no
// name is a number in both; IEEE division is, in C, R, Julia and MATLAB.
// A <P> value is data: it reaches the C model through parms[] at run time and
// is pasted only into R-like files, so it takes the plain spelling.  (`inf`
// is translated in the bounds columns alone -- not in the value.)
static const Spelling kSpellDefault  = {"NaN", "Inf"};
static const Spelling kSpellC        = {"NAN", "INFINITY"};
static const Spelling kSpellUbiquity = {"(0.0/0.0)", "(1.0/0.0)"};

struct Num
{
   double          value;
   const Spelling& spelling;
};

static std::ostream& operator<<(std::ostream& out, const Num& n)
{
   if (std::isnan(n.value)) return out << n.spelling.nan;
   if (std::isinf(n.value)) return out << (n.value < 0 ? "-" : "") << n.spelling.inf;
   return out << n.value;
}

// What is left without a value after the converters have expanded initial
// assignments.  An algebraic rule may determine it, but only one linear in a
// single undetermined variable is solved (solveLinearAlgebraicRules()), and
// the DAE path matches species alone.  Writing 1 instead would be a plausible
// wrong answer; NaN plus this warning is not.
static void warnUndefinedValues(Model* model, const std::string& target)
{
   std::string list;
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     const Compartment* c = model->getCompartment(i);
     if (std::isnan(compartmentValue(c)) && !hasAssignmentRule(model, c->getIdAttribute()))
     {
       list += (list.empty() ? "" : ", ") + c->getIdAttribute();
     }
   }
   for (unsigned int i = 0; i < model->getNumParameters(); i++)
   {
     const Parameter* p = model->getParameter(i);
     if (std::isnan(p->getValue()) && !hasAssignmentRule(model, p->getIdAttribute()))
     {
       list += (list.empty() ? "" : ", ") + p->getIdAttribute();
     }
   }
   if (list.empty()) return;

   Rcpp::warning("No value is defined for " + list + ", so " + target +
                 " receives NaN for it. If an algebraic rule determines it, "
                 "r2sbml could not solve that rule for it: only a rule "
                 "linear in a single undetermined variable is solved.");
}

// A varying compartment must not also be emitted as a constant: one with a
// rate rule is a state, one with an assignment rule is recomputed in the RHS
// by the same loop that handles assignment-rule species.
static bool compartmentIsVarying(Model* model, const std::string& id)
{
   return rateRuleFor(model, id) >= 0 || hasAssignmentRule(model, id);
}

// Species that carry a rate rule of their own in the file, as opposed to one
// replaceReactions made from their reactions.  Recorded by convertReactions()
// before converting, since afterwards the two look the same.
static std::set<std::string> ownRateRuleSpecies;

// --- dV/dt for a compartment an assignment rule sets ---------------------------
//
// The dilution term needs dV/dt.  A rate rule gives it outright; for an
// assignment rule V = g it is the total time derivative of g,
//
//   dV/dt = dg/dtime + sum over states x of dg/dx * dx/dt,
//
// taken once every assignment-rule variable in g has been substituted in
// (SBML Test Suite 00310-00318, where C = p1 * p2 and p2 has a rate rule).
// The states are the rate-rule parameters and compartments; anything else g
// reads is constant and contributes nothing.  A rule that reads a species is
// refused: the species' own derivative would then depend on the volume's.
//
// libSBML's ASTNode::derivative() is not used.  Its power rule returns
// n * A^(n-1) without the factor dA/dx, so d/dt (1 + g t)^2 came out as
// 2 + 2 g t, and its callers read a NULL from an unsupported node as a
// derivative of 0.  differentiate() below covers arithmetic, powers and
// roots, exp, ln, log and the plain and hyperbolic sine, cosine and tangent;
// anything else that depends on the variable fails the whole derivative, and
// the compartment keeps the warning in warnAssignmentRuleCompartment().

// Compartment id -> dV/dt, for each assignment-rule compartment whose rule
// could be differentiated; NULL when the derivative is identically 0.
static std::map<std::string, ASTNode*> derivedVolumeRates;

// Differentiating by this name means by the time csymbol.
static const char* const kTimeVariable = "\x01time";

static bool dependsOn(const ASTNode* node, const std::string& x)
{
   if (x == kTimeVariable ? node->getType() == AST_NAME_TIME
                          : (node->getType() == AST_NAME && node->getName() != NULL &&
                             x == node->getName())) return true;
   for (unsigned int i = 0; i < node->getNumChildren(); i++)
     if (dependsOn(node->getChild(i), x)) return true;
   return false;
}

// Builders that take ownership of their arguments.
static ASTNode* number(double v)
{
   ASTNode* n = new ASTNode(AST_REAL);
   n->setValue(v);
   return n;
}

static ASTNode* binary(ASTNodeType_t type, ASTNode* a, ASTNode* b)
{
   ASTNode* n = new ASTNode(type);
   n->addChild(a);
   n->addChild(b);
   return n;
}

static ASTNode* unary(ASTNodeType_t type, ASTNode* a)
{
   ASTNode* n = new ASTNode(type);
   n->addChild(a);
   return n;
}

// d(node)/dx, or NULL when it is identically 0.  Sets @p ok to false on a
// construct it cannot differentiate.
static ASTNode* differentiate(const ASTNode* node, const std::string& x, bool& ok)
{
   if (!ok || !dependsOn(node, x)) return NULL;

   const unsigned int n = node->getNumChildren();
   ASTNode* u  = n > 0 ? node->getChild(0)->deepCopy() : NULL;
   ASTNode* du = n > 0 ? differentiate(node->getChild(0), x, ok) : NULL;
   ASTNode* result = NULL;

   switch (node->getType())
   {
     case AST_NAME:
     case AST_NAME_TIME:
       result = number(1.0);
       break;

     case AST_PLUS:
     {
       ASTNode* sum = new ASTNode(AST_PLUS);
       if (du != NULL) { sum->addChild(du); du = NULL; }
       for (unsigned int i = 1; i < n; i++)
       {
         ASTNode* d = differentiate(node->getChild(i), x, ok);
         if (d != NULL) sum->addChild(d);
       }
       if (sum->getNumChildren() == 1) { result = sum->getChild(0)->deepCopy(); delete sum; }
       else result = sum;
       break;
     }

     case AST_MINUS:
       if (n == 1) { result = du != NULL ? unary(AST_MINUS, du) : NULL; du = NULL; break; }
       if (n == 2)
       {
         ASTNode* dv = differentiate(node->getChild(1), x, ok);
         if (dv == NULL)      result = du;
         else if (du == NULL) result = unary(AST_MINUS, dv);
         else                 result = binary(AST_MINUS, du, dv);
         du = NULL;
         break;
       }
       ok = false;
       break;

     case AST_TIMES:
     {
       // d(c1 c2 ... cn) = sum over i of dci * (product of the others)
       ASTNode* sum = new ASTNode(AST_PLUS);
       for (unsigned int i = 0; i < n; i++)
       {
         ASTNode* d = (i == 0) ? du : differentiate(node->getChild(i), x, ok);
         if (i == 0) du = NULL;
         if (d == NULL) continue;
         ASTNode* term = new ASTNode(AST_TIMES);
         term->addChild(d);
         for (unsigned int j = 0; j < n; j++)
           if (j != i) term->addChild(node->getChild(j)->deepCopy());
         sum->addChild(term);
       }
       if (sum->getNumChildren() == 1) { result = sum->getChild(0)->deepCopy(); delete sum; }
       else result = sum;
       break;
     }

     case AST_DIVIDE:
     {
       if (n != 2) { ok = false; break; }
       ASTNode* v  = node->getChild(1)->deepCopy();
       ASTNode* dv = differentiate(node->getChild(1), x, ok);
       if (dv == NULL)
       {
         result = binary(AST_DIVIDE, du, v);
       }
       else
       {
         // (du v - u dv) / v^2
         ASTNode* top = binary(AST_MINUS,
                               du != NULL ? binary(AST_TIMES, du, v->deepCopy()) : number(0.0),
                               binary(AST_TIMES, u->deepCopy(), dv));
         result = binary(AST_DIVIDE, top, binary(AST_POWER, v, number(2.0)));
       }
       du = NULL;
       break;
     }

     case AST_POWER:
     case AST_FUNCTION_POWER:
     case AST_FUNCTION_ROOT:
     {
       // root(n, u) is u^(1/n), and sqrt(u) has the single child u.
       ASTNode* base = NULL;
       ASTNode* expo = NULL;
       if (node->getType() == AST_FUNCTION_ROOT)
       {
         const bool degree = (n == 2);
         base = node->getChild(degree ? 1 : 0)->deepCopy();
         expo = binary(AST_DIVIDE, number(1.0),
                       degree ? node->getChild(0)->deepCopy() : number(2.0));
       }
       else if (n == 2)
       {
         base = u->deepCopy();
         expo = node->getChild(1)->deepCopy();
       }
       else { ok = false; break; }

       ASTNode* db = differentiate(base, x, ok);
       ASTNode* de = differentiate(expo, x, ok);
       ASTNode* sum = new ASTNode(AST_PLUS);
       if (db != NULL)   // e * b^(e - 1) * db
         sum->addChild(binary(AST_TIMES, binary(AST_TIMES, expo->deepCopy(),
                         binary(AST_POWER, base->deepCopy(),
                                binary(AST_MINUS, expo->deepCopy(), number(1.0)))), db));
       if (de != NULL)   // b^e * ln(b) * de
         sum->addChild(binary(AST_TIMES, binary(AST_TIMES,
                         binary(AST_POWER, base->deepCopy(), expo->deepCopy()),
                         unary(AST_FUNCTION_LN, base->deepCopy())), de));
       delete base;
       delete expo;
       if (sum->getNumChildren() == 1) { result = sum->getChild(0)->deepCopy(); delete sum; }
       else if (sum->getNumChildren() == 0) delete sum;
       else result = sum;
       break;
     }

     case AST_FUNCTION_EXP:
       if (n != 1 || du == NULL) { ok = ok && n == 1; break; }
       result = binary(AST_TIMES, du, node->deepCopy());
       du = NULL;
       break;

     case AST_FUNCTION_LN:
       if (n != 1 || du == NULL) { ok = ok && n == 1; break; }
       result = binary(AST_DIVIDE, du, u->deepCopy());
       du = NULL;
       break;

     case AST_FUNCTION_LOG:
     {
       // log(base, v); with one child the base is 10
       const bool based = (n == 2);
       const ASTNode* arg = node->getChild(based ? 1 : 0);
       if (based && dependsOn(node->getChild(0), x)) { ok = false; break; }
       ASTNode* dv = differentiate(arg, x, ok);
       if (dv == NULL) break;
       ASTNode* lnBase = unary(AST_FUNCTION_LN,
                               based ? node->getChild(0)->deepCopy() : number(10.0));
       result = binary(AST_DIVIDE, dv, binary(AST_TIMES, arg->deepCopy(), lnBase));
       break;
     }

     case AST_FUNCTION_SIN:
     case AST_FUNCTION_COS:
     case AST_FUNCTION_TAN:
     case AST_FUNCTION_SINH:
     case AST_FUNCTION_COSH:
     case AST_FUNCTION_TANH:
     {
       if (n != 1) { ok = false; break; }
       if (du == NULL) break;
       ASTNode* outer = NULL;
       switch (node->getType())
       {
         case AST_FUNCTION_SIN:  outer = unary(AST_FUNCTION_COS, u->deepCopy()); break;
         case AST_FUNCTION_COS:  outer = unary(AST_MINUS, unary(AST_FUNCTION_SIN, u->deepCopy())); break;
         case AST_FUNCTION_SINH: outer = unary(AST_FUNCTION_COSH, u->deepCopy()); break;
         case AST_FUNCTION_COSH: outer = unary(AST_FUNCTION_SINH, u->deepCopy()); break;
         case AST_FUNCTION_TAN:
           outer = binary(AST_POWER, unary(AST_FUNCTION_COS, u->deepCopy()), number(-2.0)); break;
         default:
           outer = binary(AST_POWER, unary(AST_FUNCTION_COSH, u->deepCopy()), number(-2.0)); break;
       }
       result = binary(AST_TIMES, outer, du);
       du = NULL;
       break;
     }

     default:
       ok = false;
       break;
   }

   delete u;
   delete du;
   if (!ok) { delete result; return NULL; }
   return result;
}

static void collectNames(const ASTNode* node, std::set<std::string>& names)
{
   if (node == NULL) return;
   if (node->getType() == AST_NAME && node->getName() != NULL) names.insert(node->getName());
   for (unsigned int i = 0; i < node->getNumChildren(); i++)
     collectNames(node->getChild(i), names);
}

// dV/dt for assignment-rule compartment @p cid; false if it cannot be formed.
// On success *rate is the derivative (owned by the caller), or NULL for 0.
static bool assignmentVolumeRate(Model* model, const std::string& cid, ASTNode** rate)
{
   *rate = NULL;
   const Rule* rule = model->getAssignmentRule(cid);
   if (rule == NULL || !rule->isSetMath()) return false;

   // Substitute assignment-rule variables until none is left; the bound
   // stops a cycle, which SBML forbids anyway.
   ASTNode* g = rule->getMath()->deepCopy();
   for (int pass = 0; ; pass++)
   {
     std::set<std::string> names;
     collectNames(g, names);
     bool replaced = false;
     for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
     {
       const Rule* ar = model->getAssignmentRule(*it);
       if (ar == NULL || !ar->isSetMath()) continue;
       g->replaceArgument(*it, const_cast<ASTNode*>(ar->getMath()));
       replaced = true;
     }
     if (!replaced) break;
     if (pass > 100) { delete g; return false; }
   }

   std::set<std::string> names;
   collectNames(g, names);
   bool ok = true;
   ASTNode* sum = new ASTNode(AST_PLUS);
   ASTNode* dt  = differentiate(g, kTimeVariable, ok);
   if (dt != NULL) sum->addChild(dt);
   for (std::set<std::string>::const_iterator it = names.begin(); ok && it != names.end(); ++it)
   {
     if (model->getSpecies(*it) != NULL) { ok = false; break; }
     const int rr = rateRuleFor(model, *it);
     if (rr < 0 || !model->getRule(rr)->isSetMath()) continue;
     ASTNode* dx = differentiate(g, *it, ok);
     if (dx == NULL) continue;
     sum->addChild(binary(AST_TIMES, dx, model->getRule(rr)->getMath()->deepCopy()));
   }
   delete g;

   if (!ok) { delete sum; return false; }
   if (sum->getNumChildren() == 0) { delete sum; return true; }
   ASTNode* result = sum;
   if (sum->getNumChildren() == 1)
   {
     result = sum->getChild(0)->deepCopy();
     delete sum;
   }
   // C++, MATLAB and ubiquity spell piecewise through a helper or a hoisted
   // <Ad> that only the rules themselves are given.
   if (r2sbml::hasPiecewise(result)) { delete result; return false; }
   *rate = result;
   return true;
}

static void clearDerivedVolumeRates()
{
   for (std::map<std::string, ASTNode*>::iterator it = derivedVolumeRates.begin();
        it != derivedVolumeRates.end(); ++it) delete it->second;
   derivedVolumeRates.clear();
}

// Run once the model is final, before any writer.
static void deriveVolumeRates(Model* model)
{
   clearDerivedVolumeRates();
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     const std::string id = model->getCompartment(i)->getIdAttribute();
     if (!hasAssignmentRule(model, id)) continue;
     ASTNode* rate = NULL;
     if (assignmentVolumeRate(model, id, &rate)) derivedVolumeRates[id] = rate;
   }
}

// dV/dt for a varying compartment: its rate rule, or the derived derivative of
// its assignment rule.  NULL when the volume does not change.
static const ASTNode* volumeRate(Model* model, const std::string& cid)
{
   const int rr = rateRuleFor(model, cid);
   if (rr >= 0) return model->getRule(rr)->getMath();
   std::map<std::string, ASTNode*>::const_iterator it = derivedVolumeRates.find(cid);
   return it == derivedVolumeRates.end() ? NULL : it->second;
}

// The compartment whose changing volume dilutes @p species, or "" for none.
// What is conserved, absent reactions, is the *amount*: so a species with no
// rule is diluted even when constant or a boundary species (SBML Test Suite
// cases 01117, 01118).  A hasOnlySubstanceUnits species integrates an amount,
// which dilution does not touch, and a species' own rate rule gives d[S]/dt
// outright, volume change included (01198, 01208).
static std::string dilutingCompartment(Model* model, const Species* species)
{
   if (species->getHasOnlySubstanceUnits() ||
       ownRateRuleSpecies.count(species->getIdAttribute())) return "";
   const std::string cid = species->getCompartment();
   return volumeRate(model, cid) != NULL ? cid : "";
}

// Right-hand side for a species, with the dilution term when one applies.
// @p serialise is the target's formula writer, so the composition below is the
// only target-specific part -- every target spells +, -, * and / infix.
static std::string speciesDerivative(Model* model, const Species* species,
                                     std::string (*serialise)(const ASTNode*))
{
   const std::string id   = species->getIdAttribute();
   const int         rule = rateRuleFor(model, id);
   const std::string cid  = dilutingCompartment(model, species);

   const std::string reaction =
       (rule >= 0) ? serialise(model->getRule(rule)->getMath()) : std::string("0");
   if (cid.empty()) return reaction;

   const std::string dV = serialise(volumeRate(model, cid));
   const std::string dilution = id + " * (" + dV + ") / " + cid;

   return (rule >= 0) ? "(" + reaction + ") - " + dilution : "-" + dilution;
}

// True when any rule uses piecewise.  C++ and MATLAB have no conditional
// expression for it to become, so their writers emit a one-line
// r2sbml_ifelse() helper -- only when this says one is called.
static bool usesPiecewise(Model* model)
{
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     if (r2sbml::hasPiecewise(model->getRule(i)->getMath())) return true;
   }
   return false;
}

// An assignment-rule compartment whose rule assignmentVolumeRate() could not
// differentiate gets its volume right but not its dilution.  Say so rather
// than emit a plausible wrong answer.
static void warnAssignmentRuleCompartment(Model* model, const std::string& target)
{
   std::string list;
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     const std::string id = model->getCompartment(i)->getIdAttribute();
     if (hasAssignmentRule(model, id) && !derivedVolumeRates.count(id))
     {
       if (!list.empty()) list += ", ";
       list += id;
     }
   }

   if (!list.empty())
     Rcpp::warning("Compartment(s) " + list + " follow an assignment rule, so " +
                   target + " receives the right volume but no dilution term for "
                   "the species inside them. d[S]/dt is missing -[S]*(dV/dt)/V: "
                   "the rule could not be differentiated (it reads a species, or "
                   "uses piecewise, min, max, abs or another function r2sbml "
                   "does not differentiate).");

   // The starting volume is the rule's value at t = 0 when that can be worked
   // out from the parameters and compartments alone.  A rule that reads a
   // species cannot be, and the declared size is used instead.
   const std::map<std::string, double> known = valuesAtStart(model);
   std::string unevaluated;
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     const std::string id = model->getCompartment(i)->getIdAttribute();
     if (hasAssignmentRule(model, id) && !known.count(id))
     {
       unevaluated += (unevaluated.empty() ? "" : ", ") + id;
     }
   }
   if (unevaluated.empty()) return;

   Rcpp::warning("The assignment rule for compartment(s) " + unevaluated +
                 " could not be evaluated at t = 0, so species initial values "
                 "inside them were converted with the declared size instead.");
}

// --- stoichiometry a rule sets ------------------------------------------------
//
// A speciesReference with an id can have its stoichiometry set by an
// assignment rule, a rate rule or an initial assignment.  replaceReactions
// (SBMLReactionConverter::determineStoichiometryNode) asks isSetStoichiometry()
// *first*, so as soon as the file also declares a number -- which Level 3
// files routinely do -- that number is used and the rule ignored.  SBML Test
// Suite case 00971 declares 1 under the rule `Xref = 3`, and X grew a third as
// fast as it should.  When no number is set, the converter does the right
// thing for an assignment rule (it writes the id, and creates a parameter of
// that name for the rule to set) and for an initial assignment (it inlines
// the expression), but has no case for a rate rule at all.
//
// So before converting: unset the declared number wherever a rule or initial
// assignment governs it, and for a rate rule swap in a placeholder assignment
// rule so the converter takes its by-name path.  The rate rule
// itself is held aside meanwhile -- the converter validates the model first and
// rejects two rules for one variable -- and so is any initial assignment on
// it, which the converter would otherwise inline ahead of the placeholder.
// After converting: drop the placeholders, put the held rules and initial
// assignments back, and give the parameter the declared number as its
// starting value, so integratedRateStates() integrates it like any other and
// expandInitialAssignments can still apply an initial assignment to it.
struct StoichiometryPatch
{
   std::vector<std::string>                     placeholders;
   std::vector<std::pair<std::string, double> > declared;
   std::vector<Rule*>                           heldRules;
   std::vector<InitialAssignment*>              held;
};

static StoichiometryPatch exposeRuleStoichiometry(Model* model)
{
   StoichiometryPatch patch;
   for (unsigned int i = 0; i < model->getNumReactions(); i++)
   {
     Reaction* rn = model->getReaction(i);
     for (unsigned int side = 0; side < 2; side++)
     {
       const unsigned int n = side ? rn->getNumProducts() : rn->getNumReactants();
       for (unsigned int j = 0; j < n; j++)
       {
         SpeciesReference* sr = side ? rn->getProduct(j) : rn->getReactant(j);
         if (!sr->isSetId()) continue;
         const std::string id = sr->getId();
         // Level 3 Version 2 lets a rule or initial assignment omit its math,
         // and one that does has no effect: the declared number stands (SBML
         // Test Suite cases 01464, 01465, 01552, 01553, 01657).
         const Rule*              ar0 = model->getAssignmentRule(id);
         const Rule*              rr0 = model->getRateRule(id);
         const InitialAssignment* ia0 = model->getInitialAssignment(id);
         const bool assigned = ar0 != NULL && ar0->isSetMath();
         const bool rated    = rr0 != NULL && rr0->isSetMath();
         const bool initial  = ia0 != NULL && ia0->isSetMath();
         if (!assigned && !rated && !initial) continue;

         patch.declared.push_back(std::make_pair(id, sr->isSetStoichiometry()
             ? sr->getStoichiometry() : std::numeric_limits<double>::quiet_NaN()));
         sr->unsetStoichiometry();

         if (rated && !assigned)
         {
           patch.heldRules.push_back(model->removeRuleByVariable(id));
           if (model->getInitialAssignment(id) != NULL)
           {
             patch.held.push_back(model->removeInitialAssignment(id));
           }
           // The converter only asks whether an assignment rule exists, so
           // the formula is never used.  It must not be `id` (circular, and
           // rejected) and must not be anything a rate law might contain, since
           // the converter rewrites any subexpression equal to an assignment
           // rule's formula into that rule's variable; NaN is neither.
           AssignmentRule* ar = model->createAssignmentRule();
           ar->setVariable(id);
           ASTNode placeholder(AST_REAL);
           placeholder.setValue(std::numeric_limits<double>::quiet_NaN());
           ar->setMath(&placeholder);
           patch.placeholders.push_back(id);
         }
       }
     }
   }
   return patch;
}

static void restoreRuleStoichiometry(Model* model, StoichiometryPatch& patch)
{
   for (size_t k = 0; k < patch.placeholders.size(); k++)
   {
     for (unsigned int i = model->getNumRules(); i-- > 0; )
     {
       const Rule* r = model->getRule(i);
       if (r->isAssignment() && r->getVariable() == patch.placeholders[k])
       {
         delete model->removeRule(i);
         break;
       }
     }
   }
   for (size_t k = 0; k < patch.heldRules.size(); k++)
   {
     model->addRule(patch.heldRules[k]);
     delete patch.heldRules[k];
   }
   patch.heldRules.clear();
   for (size_t k = 0; k < patch.held.size(); k++)
   {
     model->addInitialAssignment(patch.held[k]);
     delete patch.held[k];
   }
   patch.held.clear();

   // The converter creates the parameter with no value; a rate rule needs one
   // to start from.  An assignment rule overrides whatever is set here.
   for (size_t k = 0; k < patch.declared.size(); k++)
   {
     Parameter* p = model->getParameter(patch.declared[k].first);
     if (p != NULL && !p->isSetValue() && !std::isnan(patch.declared[k].second))
     {
       p->setValue(patch.declared[k].second);
     }
   }
}

// --- conversion factors -------------------------------------------------------
//
// In Level 3 a reaction changes a species by stoichiometry * rate *
// conversionFactor, taking the species' own conversionFactor, else the
// model's.  replaceReactions ignores both, so every reaction term came out
// short by that factor (SBML Test Suite cases 00975-00977, a factor of 4).
// The factor must be a constant parameter, so it can be applied afterwards as
// a single multiplier on each reaction-derived rate rule.
//
// Which rate rules are reaction-derived has to be decided before converting:
// a species that already has a rate rule takes no part in reactions (SBML
// requires it to be a boundary species if it appears in one), so it is
// excluded, as are species no reaction changes.
static std::vector<std::pair<std::string, std::string> > reactionConversionFactors(Model* model)
{
   std::vector<std::pair<std::string, std::string> > factors;
   const std::string modelFactor =
       model->isSetConversionFactor() ? model->getConversionFactor() : "";

   for (unsigned int i = 0; i < model->getNumSpecies(); i++)
   {
     const Species* s = model->getSpecies(i);
     const std::string factor =
         s->isSetConversionFactor() ? s->getConversionFactor() : modelFactor;
     if (factor.empty() || s->getBoundaryCondition() || s->getConstant() ||
         model->getRateRule(s->getIdAttribute()) != NULL) continue;

     bool inReaction = false;
     for (unsigned int j = 0; j < model->getNumReactions() && !inReaction; j++)
     {
       const Reaction* rn = model->getReaction(j);
       inReaction = rn->getReactant(s->getIdAttribute()) != NULL ||
                    rn->getProduct(s->getIdAttribute()) != NULL;
     }
     if (inReaction) factors.push_back(std::make_pair(s->getIdAttribute(), factor));
   }
   return factors;
}

static void applyConversionFactors(Model* model,
                                   const std::vector<std::pair<std::string, std::string> >& factors)
{
   for (size_t k = 0; k < factors.size(); k++)
   {
     const int rule = rateRuleFor(model, factors[k].first);
     if (rule < 0 || !model->getRule(rule)->isSetMath()) continue;

     ASTNode scaled(AST_TIMES);
     ASTNode* cf = new ASTNode(AST_NAME);
     cf->setName(factors[k].second.c_str());
     scaled.addChild(cf);
     scaled.addChild(model->getRule(rule)->getMath()->deepCopy());
     model->getRule(rule)->setMath(&scaled);
   }
}

// --- reaction ids used in math --------------------------------------------------
//
// In Level 3 a reaction's id, used in math, denotes its rate: the value of its
// kinetic law.  replaceReactions removes the reactions, leaving the name
// undefined (SBML Test Suite cases 01225, 01226, 01231, 01301, 01302, 01306).
// The kinetic laws are captured beforehand, and each referenced one comes back
// afterwards as a parameter set by an assignment rule.  Local parameters are
// constants, so their values are inlined; a reaction with no kinetic law gets
// the parameter alone, which warnUndefinedValues() reports.
static bool mentionsName(const ASTNode* node, const std::string& id)
{
   if (node == NULL) return false;
   if (node->getType() == AST_NAME && id == node->getName()) return true;
   for (unsigned int i = 0; i < node->getNumChildren(); i++)
     if (mentionsName(node->getChild(i), id)) return true;
   return false;
}

static bool mentionedInMath(Model* model, const std::string& id)
{
   for (unsigned int i = 0; i < model->getNumRules(); i++)
     if (mentionsName(model->getRule(i)->getMath(), id)) return true;
   for (unsigned int i = 0; i < model->getNumInitialAssignments(); i++)
     if (mentionsName(model->getInitialAssignment(i)->getMath(), id)) return true;
   for (unsigned int i = 0; i < model->getNumReactions(); i++)
   {
     const KineticLaw* kl = model->getReaction(i)->getKineticLaw();
     if (kl != NULL && mentionsName(kl->getMath(), id)) return true;
   }
   return false;
}

typedef std::vector<std::pair<std::string, ASTNode*> > ReactionRates;

static ReactionRates referencedReactionRates(Model* model)
{
   ReactionRates rates;
   for (unsigned int i = 0; i < model->getNumReactions(); i++)
   {
     const Reaction* rn = model->getReaction(i);
     if (!mentionedInMath(model, rn->getIdAttribute())) continue;

     ASTNode* rate = NULL;
     const KineticLaw* kl = rn->getKineticLaw();
     if (kl != NULL && kl->isSetMath())
     {
       rate = kl->getMath()->deepCopy();
       for (unsigned int j = 0; j < kl->getNumParameters(); j++)
       {
         const Parameter* local = kl->getParameter(j);
         ASTNode value(AST_REAL);
         value.setValue(local->getValue());
         rate->replaceArgument(local->getIdAttribute(), &value);
       }
     }
     rates.push_back(std::make_pair(rn->getIdAttribute(), rate));
   }
   return rates;
}

static void restoreReactionRates(Model* model, ReactionRates& rates)
{
   for (size_t k = 0; k < rates.size(); k++)
   {
     Parameter* p = model->createParameter();
     p->setId(rates[k].first);
     p->setConstant(false);
     if (rates[k].second == NULL) continue;

     AssignmentRule* rule = model->createAssignmentRule();
     rule->setVariable(rates[k].first);
     rule->setMath(rates[k].second);
     delete rates[k].second;
   }
}

// --- one species referenced more than once by a reaction ----------------------
//
// SBML allows a species to appear several times among a reaction's reactants
// (or products), and the stoichiometries add.  replaceReactions looks each
// reference up by species id (SBMLReactionConverter::determineStoichiometryNode
// calls getReactant(speciesId)), so every repeat counts with the *first*
// reference's stoichiometry: 1 + 2 came out as 1 + 1 (SBML Test Suite cases
// 01421-01452).  So merge the repeats into the first reference beforehand.
// Plain numbers are summed.  When a rule or initial assignment sets any part,
// each such part becomes a parameter of its own id -- so that rule or
// assignment still applies, and still means what it did -- and the merged
// reference takes a fresh id and an assignment rule summing the parts.
static void mergeRepeatedReferences(Model* model)
{
   for (unsigned int i = 0; i < model->getNumReactions(); i++)
   {
     Reaction* rn = model->getReaction(i);
     for (unsigned int side = 0; side < 2; side++)
     {
       std::map<std::string, std::vector<unsigned int> > bySpecies;
       std::vector<std::string> order;
       const unsigned int n = side ? rn->getNumProducts() : rn->getNumReactants();
       for (unsigned int j = 0; j < n; j++)
       {
         const SpeciesReference* sr = side ? rn->getProduct(j) : rn->getReactant(j);
         if (!bySpecies.count(sr->getSpecies())) order.push_back(sr->getSpecies());
         bySpecies[sr->getSpecies()].push_back(j);
       }

       std::vector<unsigned int> drop;
       std::vector<std::pair<std::string, double> > parts;  // id, declared value
       std::vector<bool> partVaries;
       for (size_t k = 0; k < order.size(); k++)
       {
         const std::vector<unsigned int>& refs = bySpecies[order[k]];
         if (refs.size() < 2) continue;

         ASTNode sum(AST_PLUS);
         double total = 0;
         bool assigned = false;
         for (size_t r = 0; r < refs.size(); r++)
         {
           SpeciesReference* sr = side ? rn->getProduct(refs[r]) : rn->getReactant(refs[r]);
           const double declared = sr->isSetStoichiometry()
               ? sr->getStoichiometry() : std::numeric_limits<double>::quiet_NaN();
           const std::string id = sr->isSetId() ? sr->getId() : "";
           const bool ruled = !id.empty() && model->getRule(id) != NULL;
           if (ruled || (!id.empty() && model->getInitialAssignment(id) != NULL))
           {
             ASTNode* name = new ASTNode(AST_NAME);
             name->setName(id.c_str());
             sum.addChild(name);
             parts.push_back(std::make_pair(id, declared));
             partVaries.push_back(ruled);
             assigned = true;
           }
           else
           {
             ASTNode* value = new ASTNode(AST_REAL);
             value->setValue(declared);
             sum.addChild(value);
           }
           total += declared;
           if (r > 0) drop.push_back(refs[r]);
         }

         SpeciesReference* first = side ? rn->getProduct(refs[0]) : rn->getReactant(refs[0]);
         if (assigned)
         {
           const std::string id = "r2sbml_" + rn->getIdAttribute() + "_" + order[k] +
                                  (side ? "_products" : "_reactants");
           first->setId(id);
           first->setConstant(false);
           AssignmentRule* rule = model->createAssignmentRule();
           rule->setVariable(id);
           rule->setMath(&sum);
         }
         else
         {
           first->setStoichiometry(total);
         }
       }

       std::sort(drop.begin(), drop.end());
       for (size_t d = drop.size(); d-- > 0; )
       {
         delete (side ? rn->removeProduct(drop[d]) : rn->removeReactant(drop[d]));
       }
       // Only now are the parts' ids free for the parameters to take.
       for (size_t q = 0; q < parts.size(); q++)
       {
         Parameter* p = model->createParameter();
         p->setId(parts[q].first);
         p->setConstant(!partVaries[q]);
         if (!std::isnan(parts[q].second)) p->setValue(parts[q].second);
       }
     }
   }
}

// --- species-reference ids that other math reads ------------------------------
//
// A speciesReference id used in math denotes its stoichiometry.  When no rule
// sets it, replaceReactions removes the reaction and leaves the name pointing
// at nothing, which its own validation then rejects ("Conversion failed.",
// SBML Test Suite cases 00974, 01121, 01498, 01566 and others).  So beforehand,
// the reference takes a fresh id and a constant parameter takes over the old
// one, with the stoichiometry as its value.  An initial assignment to the old
// id now sets that parameter, and the reference gets one of its own reading
// it, so the converter inlines the parameter's name.  Rule-set ids need none
// of this: the converter makes a parameter for those itself.
static void exposeReferencedStoichiometry(Model* model)
{
   for (unsigned int i = 0; i < model->getNumReactions(); i++)
   {
     Reaction* rn = model->getReaction(i);
     for (unsigned int side = 0; side < 2; side++)
     {
       const unsigned int n = side ? rn->getNumProducts() : rn->getNumReactants();
       for (unsigned int j = 0; j < n; j++)
       {
         SpeciesReference* sr = side ? rn->getProduct(j) : rn->getReactant(j);
         if (!sr->isSetId()) continue;
         const std::string id = sr->getId();
         const Rule* rule = model->getRule(id);
         if ((rule != NULL && rule->isSetMath()) || !mentionedInMath(model, id)) continue;

         const std::string fresh = "r2sbml_sr_" + id;
         sr->setId(fresh);
         Parameter* p = model->createParameter();
         p->setId(id);
         p->setConstant(true);
         if (sr->isSetStoichiometry()) p->setValue(sr->getStoichiometry());

         if (model->getInitialAssignment(id) != NULL)
         {
           InitialAssignment* ia = model->createInitialAssignment();
           ia->setSymbol(fresh);
           ASTNode name(AST_NAME);
           name.setName(id.c_str());
           ia->setMath(&name);
         }
       }
     }
   }
}

// --- rules and initial assignments with no math -------------------------------
//
// Level 3 Version 2 lets either omit its math, and then it has no effect.  Left
// in, every writer emitted `p = ` with nothing after it (SBML Test Suite cases
// 01235, 01236, 01555, 01556), so they are removed before anything else.
static void dropEmptyMath(Model* model)
{
   for (unsigned int i = model->getNumRules(); i-- > 0; )
     if (!model->getRule(i)->isSetMath()) delete model->removeRule(i);
   for (unsigned int i = model->getNumInitialAssignments(); i-- > 0; )
     if (!model->getInitialAssignment(i)->isSetMath())
       delete model->removeInitialAssignment(i);
}

// --- initial assignments to NaN ------------------------------------------------
//
// libSBML's evaluator returns NaN for "cannot evaluate", so
// expandInitialAssignments never expands one whose value genuinely is NaN,
// and then reports the whole conversion as failed (SBML Test Suite 00950,
// 01813).  An assignment that names nothing -- only numbers and constants --
// can be evaluated here instead, so such a NaN is set directly beforehand.
static bool namesNothing(const ASTNode* node)
{
   if (node == NULL) return true;
   if (node->isName() || node->getType() == AST_FUNCTION) return false;
   for (unsigned int i = 0; i < node->getNumChildren(); i++)
     if (!namesNothing(node->getChild(i))) return false;
   return true;
}

static void expandNaNInitialAssignments(Model* model)
{
   for (unsigned int i = model->getNumInitialAssignments(); i-- > 0; )
   {
     const InitialAssignment* ia = model->getInitialAssignment(i);
     if (!ia->isSetMath() || !namesNothing(ia->getMath())) continue;
     if (!std::isnan(SBMLTransforms::evaluateASTNode(ia->getMath(), model))) continue;

     const std::string id = ia->getSymbol();
     const double nan = std::numeric_limits<double>::quiet_NaN();
     if (Parameter* p = model->getParameter(id)) p->setValue(nan);
     else if (Compartment* c = model->getCompartment(id)) c->setSize(nan);
     else if (Species* s = model->getSpecies(id))
     {
       if (s->getHasOnlySubstanceUnits()) s->setInitialAmount(nan);
       else s->setInitialConcentration(nan);
     }
     else if (SpeciesReference* sr =
                  dynamic_cast<SpeciesReference*>(model->getElementBySId(id)))
       sr->setStoichiometry(nan);
     else continue;
     delete model->removeInitialAssignment(i);
   }
}

// --- when replaceReactions leaves out the division by the volume ---------------
//
// SBMLReactionConverter::useCompartmentSize() divides a reaction term by the
// compartment size except for a zero-dimensional compartment, and for a
// constant one of declared size 1 that the kinetic law does not name.  The
// first is wrong whenever the compartment is sized: the suite reads a species
// in it as size * concentration (01809).  The second is wrong when an initial
// assignment gives the compartment another size.  So, while converting, each
// such compartment is presented as three-dimensional and of unknown size.
struct CompartmentPatch
{
   std::vector<std::pair<std::string, double> > dimensions;
   std::vector<std::string>                     unitSize;
};

static CompartmentPatch exposeCompartmentSizes(Model* model)
{
   CompartmentPatch patch;
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     Compartment* c = model->getCompartment(i);
     const std::string id = c->getIdAttribute();
     if (c->isSetSpatialDimensions() && c->getSpatialDimensionsAsDouble() == 0 &&
         c->isSetSize())
     {
       patch.dimensions.push_back(std::make_pair(id, c->getSpatialDimensionsAsDouble()));
       c->setSpatialDimensions(3.0);
     }
     const InitialAssignment* ia = model->getInitialAssignment(id);
     if (c->isSetSize() && c->getSize() == 1.0 && ia != NULL)
     {
       patch.unitSize.push_back(id);
       c->setSize(std::numeric_limits<double>::quiet_NaN());
     }
   }
   return patch;
}

static void restoreCompartmentSizes(Model* model, const CompartmentPatch& patch)
{
   for (size_t k = 0; k < patch.dimensions.size(); k++)
   {
     Compartment* c = model->getCompartment(patch.dimensions[k].first);
     if (c != NULL) c->setSpatialDimensions(patch.dimensions[k].second);
   }
   for (size_t k = 0; k < patch.unitSize.size(); k++)
   {
     // The converter may already have applied the initial assignment itself.
     Compartment* c = model->getCompartment(patch.unitSize[k]);
     if (c != NULL && std::isnan(c->getSize())) c->setSize(1.0);
   }
}

// --- ids a target cannot use as names ---------------------------------------
//
// An SBML id is any [A-Za-z_][A-Za-z0-9_]*, so it can be a reserved word, a
// built-in constant or a function name in the target (Inf, TRUE, if, exp), or
// one of the names the generated code itself uses (time, states, p).  Written
// as is, the file does not parse, or quietly reads the wrong thing: a Julia
// parameter `p` overwrote the parameter vector it was read from.  Such an id
// is renamed before writing by appending underscores until it is free --
// `Inf` becomes `Inf_` -- everywhere it occurs, via libSBML's own
// renameSIdRefs().  deSolve output keeps the SBML id as the name of each
// state, parameter and output column (see sbmlId()); the other targets show
// the new name, and the conversion reports each one.

// R's reserved words and constants.
static const char* const kReservedR[] = {
   "if", "else", "repeat", "while", "function", "for", "in", "next", "break",
   "TRUE", "FALSE", "NULL", "Inf", "NaN", "NA", "NA_integer_", "NA_real_",
   "NA_character_", "NA_complex_", NULL };

static const char* const kReservedDeSolve[] = {
   "time", "states", "derivs", "params", "parameters", "InitialAmounts",
   "InitialDerivatives", "massBalances", "MassBalances", "Residuals", NULL };

static const char* const kReservedRxode2[] = {
   "time", "t", "Time", "TIME", "pi", "M_PI", "tlast", "tad", "podo", "amt", "cmt",
   "evid", "rate", "dur", "ID", "CMT", "AMT", "EVID", NULL };

static const char* const kReservedC[] = {
   // C++ keywords
   "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
   "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
   "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
   "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
   "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
   "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
   "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
   "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
   "protected", "public", "register", "reinterpret_cast", "requires", "return",
   "short", "signed", "sizeof", "static", "static_assert", "static_cast",
   "struct", "switch", "template", "this", "thread_local", "throw", "true",
   "try", "typedef", "typeid", "typename", "union", "unsigned", "using",
   "virtual", "void", "volatile", "wchar_t", "while", "xor", "xor_eq",
   // C macros the generated code or libm defines
   "INFINITY", "NAN", "HUGE_VAL", "M_PI", "M_E",
   // mrgsolve's reserved names (mrgsolve:::Reserved)
   "ID", "amt", "cmt", "ii", "ss", "evid", "addl", "rate", "time", "SOLVERTIME",
   "table", "ETA", "EPS", "AMT", "CMT", "TIME", "EVID", "simeps", "self",
   "simeta", "NEWIND", "DONE", "CFONSTOP", "DXDTZERO", "INITSOLV", "_F", "_R",
   "_ALAG", "SETINIT", "report", "_VARS_", "VARS", "SS_ADVANCE",
   "END_OF_INFUSION", "CHECK_MODELED_INFUSIONS", "FINAL_ROW", "FINAL_IROW",
   "II", "SS", "ADDL", "RATE", "THETA", "capture", "until", "now", NULL };

static const char* const kReservedMatlab[] = {
   "break", "case", "catch", "classdef", "continue", "else", "elseif", "end",
   "for", "function", "global", "if", "otherwise", "parfor", "persistent",
   "return", "spmd", "switch", "try", "while",
   "Inf", "inf", "NaN", "nan", "pi", "true", "false",
   // names the generated function uses
   "t", "y", "tspan", "y0", "M", "opts", "time", "states", "dydt", "nargin",
   "massBalances", "observables", "ode15s", "odeset", "zeros", "eye", "numel",
   NULL };

static const char* const kReservedJulia[] = {
   "baremodule", "begin", "break", "catch", "const", "continue", "do", "else",
   "elseif", "end", "export", "false", "finally", "for", "function", "global",
   "if", "import", "let", "local", "macro", "module", "quote", "return",
   "struct", "true", "try", "using", "while", "abstract", "mutable",
   "primitive", "type", "where", "in", "isa", "outer",
   "Inf", "NaN", "Inf64", "NaN64", "Inf32", "NaN32", "pi", "missing",
   "nothing", "ifelse",
   // names the generated file uses
   "t", "u", "du", "p", "time", "u0", "tspan", "M", "prob", "sol",
   "massbalances", "observables", "ODEFunction", "ODEProblem", NULL };

// Math functions the serialisers emit as calls.  A variable of the same name
// shadows the function in C++, MATLAB and Julia (R looks calls up apart).
static const char* const kMathFunctions[] = {
   "pow", "exp", "log", "log10", "sqrt", "abs", "fabs", "floor", "ceil",
   "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh",
   "asinh", "acosh", "atanh", "factorial", "max", "min", NULL };

static void addNames(std::set<std::string>& set, const char* const* names)
{
   for (; *names != NULL; names++) set.insert(*names);
}

// The names an SBML id must not take for @p format; empty for an unknown one.
static std::set<std::string> reservedNames(const std::string& format)
{
   std::set<std::string> names;
   if (format == "R" || format == "deSolve")
   {
     addNames(names, kReservedR);
     addNames(names, kReservedDeSolve);
   }
   else if (format == "nlmixr2" || format == "rxode2")
   {
     addNames(names, kReservedR);
     addNames(names, kReservedRxode2);
   }
   else if (format == "mrgsolve")
   {
     // $PARAM is parsed by R, the model blocks are C++
     addNames(names, kReservedR);
     addNames(names, kReservedC);
     addNames(names, kMathFunctions);
   }
   else if (format == "MATLAB" || format == "matlab")
   {
     addNames(names, kReservedMatlab);
     addNames(names, kMathFunctions);
   }
   else if (format == "Julia" || format == "julia")
   {
     addNames(names, kReservedJulia);
     addNames(names, kMathFunctions);
   }
   else if (format == "ubiquity" || format == "Ubiquity")
   {
     // pasted verbatim into both an R and a C model
     addNames(names, kReservedR);
     addNames(names, kReservedC);
     addNames(names, kMathFunctions);
   }
   return names;
}

// Renamed id -> SBML id, for the current conversion.
static std::map<std::string, std::string> renamedIds;

// The SBML id behind a (possibly renamed) id.
static std::string sbmlId(const std::string& id)
{
   std::map<std::string, std::string>::const_iterator it = renamedIds.find(id);
   return it == renamedIds.end() ? id : it->second;
}

// The SBML id as an R name tag, `c(name = ...)`: backquoted when renamed,
// since only a name the parser would reject is ever renamed.
static std::string rNameTag(const std::string& id)
{
   std::map<std::string, std::string>::const_iterator it = renamedIds.find(id);
   return it == renamedIds.end() ? id : "`" + it->second + "`";
}

static void renameReservedIds(Model* model, const std::string& format)
{
   renamedIds.clear();
   const std::set<std::string> reserved = reservedNames(format);
   if (reserved.empty()) return;

   List* all = model->getAllElements();
   std::set<std::string> taken;
   for (unsigned int i = 0; i < all->getSize(); i++)
   {
     const SBase* e = static_cast<const SBase*>(all->get(i));
     if (e->isSetIdAttribute()) taken.insert(e->getIdAttribute());
   }

   std::vector<std::pair<std::string, std::string> > renames;
   for (unsigned int i = 0; i < all->getSize(); i++)
   {
     SBase* e = static_cast<SBase*>(all->get(i));
     const int type = e->getTypeCode();
     if (type != SBML_SPECIES && type != SBML_COMPARTMENT && type != SBML_PARAMETER &&
         type != SBML_SPECIES_REFERENCE && type != SBML_REACTION) continue;
     if (!e->isSetIdAttribute() || reserved.count(e->getIdAttribute()) == 0) continue;

     const std::string old = e->getIdAttribute();
     std::string fresh = old + "_";
     while (taken.count(fresh) || reserved.count(fresh)) fresh += "_";
     taken.insert(fresh);
     e->setIdAttribute(fresh);
     renames.push_back(std::make_pair(old, fresh));
   }

   for (size_t k = 0; k < renames.size(); k++)
   {
     model->renameSIdRefs(renames[k].first, renames[k].second);
     for (unsigned int i = 0; i < all->getSize(); i++)
       static_cast<SBase*>(all->get(i))->renameSIdRefs(renames[k].first, renames[k].second);
     renamedIds[renames[k].second] = renames[k].first;
     Rcpp::Rcout << "Renamed " << renames[k].first << " to " << renames[k].second
                 << ": the name is reserved in " << format << "." << endl;
   }
   delete all;
}

// --- algebraic rules solvable for one variable ---------------------------------
//
// An algebraic rule 0 = f is a constraint, and only deSolve, MATLAB and Julia
// can solve a DAE.  But a rule that names exactly one undetermined variable x,
// and is linear in it, has the closed-form solution x = -f[x:=0] / (f[x:=1] -
// f[x:=0]), so it becomes an ordinary assignment rule every target handles.
// That covers every algebraic rule in the SBML Test Suite's failures --
// parameters (00533), compartment sizes (00539), modifier-only species
// (00549) -- where algebraicStates() matched species alone.
//
// The undetermined variables are the non-constant species, parameters and
// compartments with no rate or assignment rule, which after replaceReactions
// includes every species no reaction changes.  Solving one makes it
// determined, so the rules are peeled repeatedly; each solution reads only
// variables already determined, so no cycle can arise.  Rules naming two or
// more undetermined variables (inst/examples/sbmlalgebraicrules.xml) are left
// for the DAE path.

// Whether @p node contains the name @p x: 0 if not, 1 if linearly, -1 if in
// any other way.
static int linearIn(const ASTNode* node, const std::string& x)
{
   if (node == NULL) return 0;
   if (node->getType() == AST_NAME) return x == node->getName() ? 1 : 0;

   const unsigned int n = node->getNumChildren();
   std::vector<int> kids(n);
   bool any = false;
   for (unsigned int i = 0; i < n; i++)
   {
     kids[i] = linearIn(node->getChild(i), x);
     if (kids[i] < 0) return -1;
     any = any || kids[i] != 0;
   }
   if (!any) return 0;

   switch (node->getType())
   {
     case AST_PLUS:
     case AST_MINUS:
       return 1;
     case AST_TIMES:
     {
       int with = 0;
       for (unsigned int i = 0; i < n; i++) with += kids[i];
       return with == 1 ? 1 : -1;
     }
     case AST_DIVIDE:
       return n == 2 && kids[1] == 0 ? 1 : -1;
     default:
       return -1;
   }
}

static std::set<std::string> undeterminedVariables(Model* model)
{
   std::set<std::string> ids;
   for (unsigned int i = 0; i < model->getNumSpecies(); i++)
   {
     const Species* s = model->getSpecies(i);
     if (!s->getConstant()) ids.insert(s->getIdAttribute());
   }
   for (unsigned int i = 0; i < model->getNumParameters(); i++)
   {
     const Parameter* p = model->getParameter(i);
     if (!p->getConstant()) ids.insert(p->getIdAttribute());
   }
   for (unsigned int i = 0; i < model->getNumCompartments(); i++)
   {
     const Compartment* c = model->getCompartment(i);
     if (!c->getConstant()) ids.insert(c->getIdAttribute());
   }
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     const Rule* r = model->getRule(i);
     if (!r->isAlgebraic()) ids.erase(r->getVariable());
   }
   return ids;
}

static void solveLinearAlgebraicRules(Model* model)
{
   std::set<std::string> unknown = undeterminedVariables(model);
   bool progress = true;
   while (progress)
   {
     progress = false;
     for (unsigned int i = 0; i < model->getNumRules(); i++)
     {
       const Rule* rule = model->getRule(i);
       if (!rule->isAlgebraic() || !rule->isSetMath()) continue;

       std::set<std::string> names;
       collectNames(rule->getMath(), names);
       std::string x;
       int count = 0;
       for (std::set<std::string>::const_iterator it = names.begin(); it != names.end(); ++it)
         if (unknown.count(*it)) { x = *it; count++; }
       if (count != 1 || linearIn(rule->getMath(), x) != 1) continue;

       ASTNode zero(AST_REAL);
       zero.setValue(0.0);
       ASTNode one(AST_REAL);
       one.setValue(1.0);
       ASTNode* b = rule->getMath()->deepCopy();
       b->replaceArgument(x, &zero);
       ASTNode* b1 = rule->getMath()->deepCopy();
       b1->replaceArgument(x, &one);

       // x = -b / (f[x:=1] - b)
       ASTNode* a = new ASTNode(AST_MINUS);
       a->addChild(b1);
       a->addChild(b->deepCopy());
       ASTNode* negb = new ASTNode(AST_MINUS);
       negb->addChild(b);
       ASTNode solution(AST_DIVIDE);
       solution.addChild(negb);
       solution.addChild(a);

       delete model->removeRule(i);
       AssignmentRule* ar = model->createAssignmentRule();
       ar->setVariable(x);
       ar->setMath(&solution);

       unknown.erase(x);
       progress = true;
       break;
     }
   }
}

//'convertReactions
//'@param infile input file name
//'@param outfile output file name
//'@param format output code format, one of 'R'/'deSolve' (default), 'mrgsolve',
//' 'nlmixr2' (or 'rxode2'), 'MATLAB', 'Julia' or 'ubiquity'.  An unrecognised format is an error.
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'out_file <- tempfile(fileext = ".R")
//'convertReactions(sbml_file, out_file, format = "R")
//'out_file_mrg <- tempfile(fileext = ".cpp")
//'convertReactions(sbml_file, out_file_mrg, format = "mrgsolve")
//'out_file_rx <- tempfile(fileext = ".R")
//'convertReactions(sbml_file, out_file_rx, format = "nlmixr2")
//'out_file_m <- tempfile(fileext = ".m")
//'convertReactions(sbml_file, out_file_m, format = "MATLAB")
//'out_file_jl <- tempfile(fileext = ".jl")
//'convertReactions(sbml_file, out_file_jl, format = "Julia")
//'out_file_ub <- tempfile(fileext = ".txt")
//'convertReactions(sbml_file, out_file_ub, format = "ubiquity")
//'@return NULL invisibly
// [[Rcpp::export]]
 void convertReactions(SEXP infile, SEXP outfile, std::string format = "R"){

   if(!infile) Rcpp::stop("Input file is not present.\n");
   if(!outfile) Rcpp::stop("Output file is not present.\n");

   // read document and assign file for writing
   std::string inputFile = Rcpp::as<std::string>(infile);
   if (inputFile == "") Rcpp::stop("Input file path is empty.\n");
   std::string outputFile = Rcpp::as<std::string>(outfile);

   SBMLReader reader;
   SBMLDocument* document  = reader.readSBMLFromFile(inputFile);

   // getNumErrors(severity) counts that severity exactly, so a fatal-severity
   // diagnostic is not included in the error-severity count and has to be
   // asked for separately.  getModel() applies the same rule.
   unsigned int  errors    = document->getNumErrors(LIBSBML_SEV_ERROR) +
                             document->getNumErrors(LIBSBML_SEV_FATAL);

   // stop in case of errors
   if (errors > 0)
   {
     Rcpp::Rcerr << "Encountered the following SBML errors:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Cannot read '" + inputFile + "': libSBML reported " +
                std::to_string(errors) +
                " SBML error(s), listed above.  Nothing was written.");
   }

   dropEmptyMath(document->getModel());

   ownRateRuleSpecies.clear();
   for (unsigned int i = 0; i < document->getModel()->getNumSpecies(); i++)
   {
     const std::string id = document->getModel()->getSpecies(i)->getIdAttribute();
     const Rule* rr = document->getModel()->getRateRule(id);
     if (rr != NULL && rr->isSetMath()) ownRateRuleSpecies.insert(id);
   }

   // Six things replaceReactions gets wrong have to be set up beforehand: see
   // exposeReferencedStoichiometry(), mergeRepeatedReferences(),
   // exposeCompartmentSizes(), exposeRuleStoichiometry(),
   // reactionConversionFactors() and referencedReactionRates().
   exposeReferencedStoichiometry(document->getModel());
   mergeRepeatedReferences(document->getModel());
   const CompartmentPatch compartments = exposeCompartmentSizes(document->getModel());
   StoichiometryPatch stoichiometry = exposeRuleStoichiometry(document->getModel());
   const std::vector<std::pair<std::string, std::string> > factors =
       reactionConversionFactors(document->getModel());
   ReactionRates rates = referencedReactionRates(document->getModel());

   // create conversion object that identifies the function definition converter
   ConversionProperties props;

   props.addOption("replaceReactions", true,
                   "Replace reactions with rateRules");
   // convert
   int success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion failed.");
   }

   restoreCompartmentSizes(document->getModel(), compartments);
   restoreRuleStoichiometry(document->getModel(), stoichiometry);
   applyConversionFactors(document->getModel(), factors);
   restoreReactionRates(document->getModel(), rates);

   // promote local parameters to global parameters
   props.addOption("promoteLocalParameters", true,
                   "Promote Local Parameters to Global");
   // convert
   success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion failed.");
   }

   // expand Initial Assignments
   expandNaNInitialAssignments(document->getModel());
   props.addOption("expandInitialAssignments", true,
                   "Expanding Initial Assignments");
   // convert
   success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion failed.");
   }

   // create conversion object that identifies the function definition expansion converter
   props.addOption("expandFunctionDefinitions", true,
                   "Replace reactions with rateRules");
   // convert
   success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion failed.");
   }

   solveLinearAlgebraicRules(document->getModel());

   // Every writer emits assignment rules in list order, as statements, but
   // SBML lets a rule precede the ones it reads -- and restoreReactionRates()
   // and solveLinearAlgebraicRules() append their rules last.  Sort them so each comes after what it uses.
   // The sorter fails on a rule with no math, but dropEmptyMath() has
   // removed those.
   ConversionProperties sortProps;
   sortProps.addOption("sortRules", true, "Sort rules by dependency");
   success = document->convert(sortProps);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion failed.");
   }

   renameReservedIds(document->getModel(), format);
   deriveVolumeRates(document->getModel());

   // make names equal to ID if name doesn't exist

   Rcpp::Rcout << "Conversion completed." << endl;
   Rcpp::Rcout << "Number of ODEs - " << document->getModel()->getNumRules() << endl;

   // Every writeFile* helper opens outputFile itself; nothing is written here.
   if (format.compare("R") == 0 || format.compare("deSolve") == 0)  {
      writeFileR(document, outputFile);
   }
   else if (format.compare("mrgsolve") == 0)  {
      writeFileMrgsolve(document, outputFile);
   }
   else if (format.compare("nlmixr2") == 0 || format.compare("rxode2") == 0)  {
      writeFileNlmixr2(document, outputFile);
   }
   else if (format.compare("MATLAB") == 0 || format.compare("matlab") == 0)  {
      writeFileMatlab(document, outputFile);
   }
   else if (format.compare("Julia") == 0 || format.compare("julia") == 0)  {
      writeFileJulia(document, outputFile);
   }
   else if (format.compare("ubiquity") == 0 || format.compare("Ubiquity") == 0)  {
      writeFileUbiquity(document, outputFile);
   }
   else {
      // Without this an unrecognised format quietly leaves an empty file
      // behind, which looks like a successful conversion.
      Rcpp::stop("Unknown format '" + format + "'. Use one of 'R'/'deSolve', "
                 "'mrgsolve', 'nlmixr2'/'rxode2', 'MATLAB', 'Julia' or 'ubiquity'.");
   }

   // SBMLWriter writer;
   // writer.writeSBMLToFile(document, outputFile);
   // libsbml::writeSBMLToFile(document, outputFile);
 }

// Indices (into the rule list) of the algebraic rules.
static std::vector<int> algebraicRules(Model* model)
{
   std::vector<int> alg;
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     if (model->getRule(i)->isAlgebraic()) alg.push_back((int)i);
   }
   return alg;
}

// Positions *within @p states* of the states whose value an algebraic rule
// fixes, rather than integration.
//
// SBML does not record which variable a given algebraic rule determines, and
// libSBML exposes no matching for it.  What can be worked out is the candidate
// set: a species that is not constant and carries neither a rate rule nor an
// assignment rule is undetermined, and the specification requires the
// algebraic rules to determine exactly the undetermined variables.  So when
// the two counts agree the system is square -- see daeIsSquare() -- and which
// rule lands in which of these rows does not matter, because the solver treats
// the residuals as one simultaneous system.
static std::vector<int> algebraicStates(Model* model, const std::vector<int>& states)
{
   std::vector<int> alg;
   for (size_t i = 0; i < states.size(); i++)
   {
     const Species* s = model->getSpecies(states[i]);
     if (!s->getConstant() && rateRuleFor(model, s->getIdAttribute()) < 0)
     {
       alg.push_back((int)i);
     }
   }
   return alg;
}

// A DAE is only emitted when there is one algebraic rule per undetermined
// state.  Anything else means the model is over- or under-determined, or that
// the candidate set has been misread, and guessing there would produce a
// solver failure with no obvious cause.
static bool daeIsSquare(const std::vector<int>& algRules,
                        const std::vector<int>& algStates)
{
   return !algRules.empty() && algRules.size() == algStates.size();
}

// Warning text for a target that cannot express algebraic rules at all, and
// for a model whose algebraic rules do not form a square system.
static void warnAlgebraicUnsolved(const std::string& target, size_t nRules,
                                  bool square)
{
   std::string msg = target + " cannot enforce the " + std::to_string(nRules) +
                     " algebraic rule(s) in this model";
   if (!square)
   {
     msg = "The algebraic rules in this model do not determine exactly the "
           "undetermined variables, so no DAE was written for " + target;
   }
   Rcpp::warning(msg + ". They are written out as comments, and the species "
                 "they constrain are left at a zero derivative, so the "
                 "generated code runs but does not honour the constraint.");
}

// The deSolve extra-output vector, `c(X = X, ...)`, for the variables
// assignment rules set; "" when there are none.  ode() and daspk() both take a
// list whose later elements are extra outputs, and give each one a column of
// the solution beside the states.
static std::string deSolveOutputs(Model* model)
{
   const std::vector<std::string> vars = observedVariables(model);
   if (vars.empty()) return "";
   std::string list;
   for (size_t i = 0; i < vars.size(); i++)
   {
     list += (i ? ", " : "") + rNameTag(vars[i]) + " = " + vars[i];
   }
   return "c(" + list + ")";
}

// write output ODEs for R
int writeFileR(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   Model* model = document->getModel();

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();

   const std::vector<int> algRules  = algebraicRules(model);
   const std::vector<int> algStates = algebraicStates(model, states);
   const bool dae = daeIsSquare(algRules, algStates);
   if (!algRules.empty() && !dae) warnAlgebraicUnsolved("deSolve", algRules.size(), false);

   const std::vector<RateState> rateStates = integratedRateStates(model);
   warnAssignmentRuleCompartment(model, "deSolve");
   warnUndefinedValues(model, "deSolve");

   // auto now = std::chrono::system_clock::now();
   out << "## Automatically generated model file by r2sbml at " << endl;

   out << "## Writing the model ODEs for solving in R" << endl;
   out << "## Solving the model requires desolve package \n" << endl;
   out << "## Model Summary " << endl;
   out << "## functionDefinitions: " << model->getNumFunctionDefinitions() << endl;
   out << "##     unitDefinitions: " << model->getNumUnitDefinitions    () << endl;
   out << "##    compartmentTypes: " << model->getNumCompartmentTypes   () << endl;
   out << "##        speciesTypes: " << model->getNumSpeciesTypes       () << endl;
   out << "##        compartments: " << model->getNumCompartments       () << endl;
   out << "##             species: " << model->getNumSpecies            () << endl;
   out << "##          parameters: " << model->getNumParameters         () << endl;
   out << "##  initialAssignments: " << model->getNumInitialAssignments () << endl;
   out << "##               rules: " << model->getNumRules              () << endl;
   out << "##         constraints: " << model->getNumConstraints        () << endl;
   out << "##           reactions: " << model->getNumReactions          () << endl;
   out << "##              events: " << model->getNumEvents             () << endl;
   out << endl;

   out << "## Load required packages" << endl;
   out << "library(deSolve)" << endl << endl;

   out << "## Units Definitions" << endl;
   int nUnitsDef = model->getNumUnitDefinitions();
   for (int i = 0; i<nUnitsDef; i++){
     int nListofUnits = model->getUnitDefinition(i)->getNumUnits();
     for (int j = 0; j<nListofUnits; j++){
       out << "## " << model->getUnitDefinition(i)->getId() << ": Kind=" << model->getUnitDefinition(i)->getUnit(j)->getName() << ", : Exponent=" << model->getUnitDefinition(i)->getUnit(j)->getExponent()  << endl;
     }
   }

   out << endl;
   out << "## Compartments " << endl;
   // out << "Comaparmtments <- c(" << endl;
   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        // a varying compartment is a state or a local, not a constant
        if (compartmentIsVarying(model, model->getCompartment(i)->getIdAttribute())) continue;
        out <<  model->getCompartment(i)->getId() << " = " << Num{compartmentValue(model->getCompartment(i)), kSpellDefault} << endl; //" # (" << model->getCompartment(i)->getUnits() << ")" << endl;
   }

   out << endl;

   out <<"## Initial Amounts" << endl;
   out << "InitialAmounts <- c(" << endl;
   const int numEntries = numStates + (int)rateStates.size();
   for (int i = 0; i < numStates; i++){
      const Species* s = model->getSpecies(states[i]);
      out << "         " << rNameTag(s->getIdAttribute()) << " = " << speciesInitialValue(model, s)
          << (i + 1 < numEntries ? "," : "") << endl;
   }
   for (size_t i = 0; i < rateStates.size(); i++){
      const RateState& c = rateStates[i];
      out << "         " << rNameTag(c.id) << " = " << Num{c.initial, kSpellDefault}
          << ((int)(numStates + i + 1) < numEntries ? "," : "")
          << "   ## " << c.what << ", integrated" << endl;
   }

  out << "                    )" << endl;

   out << endl;

   out << "## Parameters" << endl;
   out << "parameters <- c(" << endl;
   const std::vector<int> params = constantParameters(model);
   const int numParams = (int)params.size();
   for (int i = 0; i < numParams; i++){
      const Parameter* p = model->getParameter(params[i]);
      out << "         " << rNameTag(p->getIdAttribute()) << " = " << Num{p->getValue(), kSpellDefault}
          << (i + 1 < numParams ? "," : "") << endl;
   }

   // for (int i = 0; i < numParams; i++) {
   //      out << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << endl; // "# (" << model->getParameter(i)->getUnits() << ")" << endl;
   // }

   out << "               )" << endl;
   out << endl;

   // for (int i = 0; i < numIAs-1; i++) {
   //      out << model->getSpecies(i)->getIdAttribute() << " = " << speciesInitialValue(model, model->getSpecies(i)) << endl; // " # (" <<  model->getSpecies(i)->getUnits() << ")" << endl;
   // }


   // Reactions (only comments)
   out << "## Reactions " << endl;
   int numReactions = model->getNumReactions();
   for (int i = 0; i < numReactions; i++){
        out <<  "## Reaction " << i << ": " << model->getReaction(i)->getKineticLaw() << endl; //" # (" << model->getCompartment(i)->getUnits() << ")" << endl;
   }

   out << endl;

   if (dae){
     // daspk needs y' at t = 0 to satisfy the residuals.  The differential
     // rows are evaluated from the same expressions the residuals use; the
     // algebraic rows are never read, so they stay 0.
     out << "## Consistent initial derivatives for daspk.  The components for" << endl;
     out << "## states fixed by an algebraic rule are not read." << endl;
     // A parameter or species an assignment rule sets is in neither
     // InitialAmounts nor parameters, so the rules have to run first, in the
     // order massBalances runs them.
     out << "InitialDerivatives <- with(as.list(c(InitialAmounts, parameters)), {" << endl;
     // The list is named by SBML id, the code below by the renamed one.
     std::vector<std::string> listed;
     for (int i = 0; i < numStates; i++) listed.push_back(model->getSpecies(states[i])->getIdAttribute());
     for (size_t i = 0; i < rateStates.size(); i++) listed.push_back(rateStates[i].id);
     for (int i = 0; i < numParams; i++) listed.push_back(model->getParameter(params[i])->getIdAttribute());
     for (size_t i = 0; i < listed.size(); i++){
       if (sbmlId(listed[i]) != listed[i])
         out << "   " << listed[i] << " = `" << sbmlId(listed[i]) << "`" << endl;
     }
     out << "   time = 0" << endl;
     for (unsigned int i = 0; i < model->getNumRules(); i++){
       const Rule* r = model->getRule(i);
       if (r->isAssignment()){
         out << "   " << r->getVariable() << " = "
             << r2sbml::formulaToInfix(r->getMath()) << endl;
       }
     }
     out << "   c(" << endl;
     for (int i = 0; i < numStates; i++){
       const Species* sp = model->getSpecies(states[i]);
       out << "         " << rNameTag(sp->getIdAttribute()) << " = "
           << speciesDerivative(model, sp, r2sbml::formulaToInfix)
           << (i + 1 < numEntries ? "," : "") << endl;
     }
     for (size_t i = 0; i < rateStates.size(); i++){
       const RateState& c = rateStates[i];
       out << "         " << rNameTag(c.id) << " = "
           << r2sbml::formulaToInfix(c.rate)
           << ((int)(numStates + i + 1) < numEntries ? "," : "") << endl;
     }
     out << "   )})" << endl << endl;

     out << "## Solve with:" << endl;
     out << "##   solution <- daspk(y = InitialAmounts, dy = InitialDerivatives," << endl;
     out << "##                     times = seq(0, 10, by = 0.1)," << endl;
     out << "##                     res = massBalances, parms = parameters)" << endl;
     out << endl;
   }

   out << "## Mass-Balances (ODEs)" << endl;
   if (dae){
     out << "## This model carries algebraic rules, so it is a DAE: massBalances is" << endl;
     out << "## a residual function for daspk, not a derivative function for ode." << endl;
     out << "massBalances <- function(time, states, derivs, params){" << endl << endl;
   } else {
     out << "massBalances <- function(time, states, params){" << endl << endl;
   }

   out << "   ## Get States Names " << endl;
   for (int i = 0; i < numStates; i++){
        out << "   " << model->getSpecies(states[i])->getIdAttribute() << " = states[[\"" << sbmlId(model->getSpecies(states[i])->getIdAttribute()) << "\"]]" << endl;
   }
   for (size_t i = 0; i < rateStates.size(); i++){
        const std::string cid = rateStates[i].id;
        out << "   " << cid << " = states[[\"" << sbmlId(cid) << "\"]]" << endl;
   }

   out << endl;
   out << "   ## Get Parameter Names " << endl;
   for (int i = 0; i<numParams; i++){
        const std::string pid = model->getParameter(params[i])->getIdAttribute();
        out << "   " << pid << " = params[[\"" << sbmlId(pid) << "\"]]" << endl;
   }

   int numODEs = model->getNumRules();

   // Assignment rules come after the state unpacking, since they may read
   // states, and before the derivatives, which may read them in turn.
   bool anyAssignment = false;
   for (int i = 0; i < numODEs; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       if (!anyAssignment){ out << endl << "   ## Assignment Rules" << endl; anyAssignment = true; }
       out << "   " << r->getVariable() << " = "
           << r2sbml::formulaToInfix(r->getMath()) << endl;
     }
   }

   out << endl;
   if (dae){
     // One residual per state: dy - f for an integrated state, the rule itself
     // for a state an algebraic rule fixes.  Which rule fills which algebraic
     // row is arbitrary; daspk solves the residuals simultaneously.
     out << "   ## Residuals: 0 = d<state>/dt - f() for an integrated state," << endl;
     out << "   ##            0 = g()               for one fixed by an algebraic rule" << endl;
     size_t nextAlgRule = 0;
     for (int i = 0; i < numStates; i++){
       const std::string id = model->getSpecies(states[i])->getIdAttribute();
       int rule = rateRuleFor(model, id);
       const bool diluted = !dilutingCompartment(model, model->getSpecies(states[i])).empty();
       if (rule >= 0 || diluted){
         out << "   res_" << id << " = derivs[[\"" << sbmlId(id) << "\"]] - ("
             << speciesDerivative(model, model->getSpecies(states[i]),
                                  r2sbml::formulaToInfix) << ")" << endl;
       } else if (nextAlgRule < algRules.size() &&
                  std::find(algStates.begin(), algStates.end(), i) != algStates.end()){
         const Rule* r = model->getRule(algRules[nextAlgRule++]);
         out << "   res_" << id << " = "
             << r2sbml::formulaToInfix(r->getMath())
             << "  ## algebraic rule fixing " << id << endl;
       } else {
         out << "   res_" << id << " = derivs[[\"" << sbmlId(id) << "\"]]  ## " << id
             << " is constant" << endl;
       }
     }
     for (size_t i = 0; i < rateStates.size(); i++){
       const std::string cid = rateStates[i].id;
       out << "   res_" << cid << " = derivs[[\"" << sbmlId(cid) << "\"]] - ("
           << r2sbml::formulaToInfix(
                  rateStates[i].rate) << ")" << endl;
     }

     out << endl;
     out << "   ## Make a list of Residuals" << endl;
     out << "   Residuals <- c(" << endl;
     for (int i = 0; i < numStates; i++){
        out << "     res_" << model->getSpecies(states[i])->getIdAttribute()
            << (i + 1 < numEntries ? " ," : "") << endl;
     }
     for (size_t i = 0; i < rateStates.size(); i++){
        out << "     res_" << rateStates[i].id
            << ((int)(numStates + i + 1) < numEntries ? " ," : "") << endl;
     }
     out << "   )" << endl;
     const std::string outputs = deSolveOutputs(model);
     out << "   return(list(Residuals" << (outputs.empty() ? "" : ", " + outputs) << "))" << endl;
     out << endl << endl << "}" << endl << endl;
     out.close();
     return 0;
   }

   out << "   ## Mass Balances" << endl;
   for (int i = 0; i < numStates; i++){
     const Species* sp = model->getSpecies(states[i]);
     const std::string id = sp->getIdAttribute();
     int rule = rateRuleFor(model, id);
     const bool diluted = !dilutingCompartment(model, sp).empty();
     if (rule >= 0 || diluted){
       out << "   d" << id << "_dt = "
           << speciesDerivative(model, sp, r2sbml::formulaToInfix)
           << (diluted ? "  ## includes dilution by the changing volume" : "") << endl;
     } else {
       out << "   d" << id << "_dt = 0  ## " << id
           << " has no rate rule and is held constant" << endl;
     }
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const std::string cid = rateStates[i].id;
     out << "   d" << cid << "_dt = "
         << r2sbml::formulaToInfix(rateStates[i].rate)
         << "  ## " << rateStates[i].what << endl;
   }

   for (int i = 0; i < numODEs; i++){
     const Rule* r = model->getRule(i);
     if (r->isAlgebraic()){
       out << "   ## Algebraic rule, NOT enforced here: 0 = "
           << r2sbml::formulaToInfix(r->getMath()) << endl;
     }
   }

   out << endl;
   out << "   ## Make a list of Mass Balances" << endl;
   out << "   MassBalances <- c(" << endl;
   for (int i = 0; i < numStates; i++){
      out << "     d" << model->getSpecies(states[i])->getIdAttribute() << "_dt"
          << (i + 1 < numEntries ? " ," : "") << endl;
   }
   for (size_t i = 0; i < rateStates.size(); i++){
      out << "     d" << rateStates[i].id << "_dt"
          << ((int)(numStates + i + 1) < numEntries ? " ," : "") << endl;
   }
   out << "   )" << endl;
   // Assignment-rule variables ride along as extra outputs, so the solution
   // has a column for each beside the states.
   const std::string outputs = deSolveOutputs(model);
   out << "   return(list(MassBalances" << (outputs.empty() ? "" : ", " + outputs) << "))" << endl;
   out << endl;


   out << "}" << endl;
   // out << "## Algebraic Rules" << endl;
   // out << "algebraicRules <- c(" << endl;
   // out << "                  )" << endl;
   out << endl;


   out.close();
   return 0;

}

// write output ODEs for mrgsolve
// SBML's time csymbol serialises as `time`, which in mrgsolve's C++ is libc's
// time() and does not compile.  mrgsolve calls the time SOLVERTIME in $ODE,
// where the solver may be between output times, and TIME in $TABLE.
static ASTNode* withTimeNamed(const ASTNode* math, const char* name)
{
   ASTNode* copy = math->deepCopy();
   std::vector<ASTNode*> stack(1, copy);
   while (!stack.empty())
   {
     ASTNode* node = stack.back();
     stack.pop_back();
     if (node->getType() == AST_NAME_TIME)
     {
       node->setType(AST_NAME);
       node->setName(name);
     }
     for (unsigned int i = 0; i < node->getNumChildren(); i++)
       stack.push_back(node->getChild(i));
   }
   return copy;
}

static std::string mrgsolveFormula(const ASTNode* math, const char* timeName)
{
   if (math == NULL) return std::string();
   ASTNode* named = withTimeNamed(math, timeName);
   const std::string formula = r2sbml::formulaToInfixC(named);
   delete named;
   return formula;
}

static std::string mrgsolveOde(const ASTNode* math)   { return mrgsolveFormula(math, "SOLVERTIME"); }
static std::string mrgsolveTable(const ASTNode* math) { return mrgsolveFormula(math, "TIME"); }

int writeFileMrgsolve(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   Model* model = document->getModel();

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();
   const std::vector<RateState> rateStates = integratedRateStates(model);
   warnAssignmentRuleCompartment(model, "mrgsolve");
   warnUndefinedValues(model, "mrgsolve");

   out << "## Automatically generated mrgsolve model file by r2sbml\n" << endl;
   out << "$PROB\n" << endl;
   if (usesPiecewise(model)){
     // piecewise becomes nested r2sbml_ifelse(cond, value, otherwise) calls.
     out << "$GLOBAL\n";
     out << "inline double r2sbml_ifelse(bool c, double a, double b) { return c ? a : b; }\n\n";
   }
   // mrgsolve warns about an empty $PARAM block, so it is left out when no
   // parameter is constant.
   const std::vector<int> params = constantParameters(model);
   if (!params.empty()) out << "$PARAM\n";
   for (size_t i = 0; i < params.size(); i++){
        const Parameter* p = model->getParameter(params[i]);
        out << p->getIdAttribute() << " = " << Num{p->getValue(), kSpellDefault} << "\n";
   }

   out << "\n$CMT\n";
   for (int i = 0; i < numStates; i++){
        out << model->getSpecies(states[i])->getIdAttribute() << "\n";
   }
   // a compartment or parameter with a rate rule is integrated alongside the species
   for (size_t i = 0; i < rateStates.size(); i++){
        out << rateStates[i].id << "\n";
   }

   out << "\n$MAIN\n";
   for (int i = 0; i < numStates; i++){
        out << model->getSpecies(states[i])->getIdAttribute() << "_0 = "
            << speciesInitialValue(model, model->getSpecies(states[i])) << ";\n";
   }
   for (size_t i = 0; i < rateStates.size(); i++){
        const RateState& c = rateStates[i];
        out << c.id << "_0 = " << Num{c.initial, kSpellC} << ";\n";
   }

   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        if (compartmentIsVarying(model, model->getCompartment(i)->getIdAttribute())) continue;
        // declared, or the C++ does not compile; mrgsolve makes a declaration
        // in $MAIN global, so $ODE and $TABLE see it too
        out << "double " << model->getCompartment(i)->getId() << " = " << Num{compartmentValue(model->getCompartment(i)), kSpellC} << ";\n";
   }

   out << "\n$ODE\n";
   int numODEs = model->getNumRules();

   // Assignment rules read states, so they are locals inside $ODE rather than
   // compartments.  They have to precede the derivatives, which read them.
   for (int i = 0; i < numODEs; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       out << "double " << r->getVariable() << " = "
           << mrgsolveOde(r->getMath()) << ";\n";
     }
   }

   for (int i = 0; i < numStates; i++){
     const Species* sp = model->getSpecies(states[i]);
     const std::string id = sp->getIdAttribute();
     int rule = rateRuleFor(model, id);
     const bool diluted = !dilutingCompartment(model, sp).empty();
     // mrgsolve model blocks are C++, so powers have to be pow() calls.
     if (rule >= 0 || diluted){
       out << "dxdt_" << id << " = "
           << speciesDerivative(model, sp, mrgsolveOde) << ";"
           << (diluted ? " // includes dilution by the changing volume" : "") << "\n";
     } else {
       out << "dxdt_" << id << " = 0; // " << id
           << " has no rate rule and is held constant\n";
     }
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const std::string cid = rateStates[i].id;
     out << "dxdt_" << cid << " = "
         << mrgsolveOde(rateStates[i].rate)
         << "; // " << rateStates[i].what << "\n";
   }

   // mrgsolve integrates ODEs only, so a constraint can only be recorded.
   const std::vector<int> algRules = algebraicRules(model);
   for (size_t i = 0; i < algRules.size(); i++){
     out << "// Algebraic rule, NOT enforced here: 0 = "
         << mrgsolveOde(model->getRule(algRules[i])->getMath()) << "\n";
   }
   if (!algRules.empty()) warnAlgebraicUnsolved("mrgsolve", algRules.size(), true);

   // The assignment-rule locals declared in $ODE are global to the model, but
   // their values there are from the solver's last internal step, not the
   // output time, so $TABLE recomputes them from the output-time states before
   // $CAPTURE records them.
   const std::vector<std::string> observed = observedVariables(model);
   if (!observed.empty()){
     out << "\n$TABLE\n";
     for (int i = 0; i < numODEs; i++){
       const Rule* r = model->getRule(i);
       if (r->isAssignment()){
         out << r->getVariable() << " = "
             << mrgsolveTable(r->getMath()) << ";\n";
       }
     }
     out << "\n$CAPTURE";
     for (size_t i = 0; i < observed.size(); i++) out << " " << observed[i];
     out << "\n";
   }

   out.close();
   return 0;
}

// write output ODEs for nlmixr2 / rxode
int writeFileNlmixr2(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   Model* model = document->getModel();

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();
   const std::vector<RateState> rateStates = integratedRateStates(model);
   warnAssignmentRuleCompartment(model, "rxode2");
   warnUndefinedValues(model, "rxode2");

   out << "## Automatically generated nlmixr2/rxode model file by r2sbml\n" << endl;
   out << "model <- function() {\n";
   out << "  ini({\n";

   const std::vector<int> params = constantParameters(model);
   for (size_t i = 0; i < params.size(); i++){
        const Parameter* p = model->getParameter(params[i]);
        out << "    " << p->getIdAttribute() << " <- " << Num{p->getValue(), kSpellDefault} << "\n";
   }

   out << "  })\n";
   out << "  model({\n";

   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        if (compartmentIsVarying(model, model->getCompartment(i)->getIdAttribute())) continue;
        out << "    " << model->getCompartment(i)->getId() << " <- " << Num{compartmentValue(model->getCompartment(i)), kSpellDefault} << "\n";
   }

   for (int i = 0; i < numStates; i++){
        out << "    " << model->getSpecies(states[i])->getIdAttribute() << "(0) <- "
            << speciesInitialValue(model, model->getSpecies(states[i])) << "\n";
   }
   // a compartment or parameter with a rate rule is integrated alongside the species
   for (size_t i = 0; i < rateStates.size(); i++){
        const RateState& c = rateStates[i];
        out << "    " << c.id << "(0) <- " << Num{c.initial, kSpellDefault} << "\n";
   }

   out << "\n";
   int numODEs = model->getNumRules();

   // Assignment rules read states, so they are plain assignments evaluated
   // ahead of the derivatives that read them.
   for (int i = 0; i < numODEs; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       out << "    " << r->getVariable() << " <- "
           << r2sbml::formulaToInfix(r->getMath()) << "\n";
     }
   }

   for (int i = 0; i < numStates; i++){
     const Species* sp = model->getSpecies(states[i]);
     const std::string id = sp->getIdAttribute();
     int rule = rateRuleFor(model, id);
     const bool diluted = !dilutingCompartment(model, sp).empty();
     if (rule >= 0 || diluted){
       out << "    d/dt(" << id << ") <- "
           << speciesDerivative(model, sp, r2sbml::formulaToInfix)
           << (diluted ? " # includes dilution by the changing volume" : "") << "\n";
     } else {
       out << "    d/dt(" << id << ") <- 0 # " << id
           << " has no rate rule and is held constant\n";
     }
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const std::string cid = rateStates[i].id;
     out << "    d/dt(" << cid << ") <- "
         << r2sbml::formulaToInfix(rateStates[i].rate)
         << " # " << rateStates[i].what << "\n";
   }

   // rxode2 integrates ODEs only, so a constraint can only be recorded.
   const std::vector<int> algRules = algebraicRules(model);
   for (size_t i = 0; i < algRules.size(); i++){
     out << "    # Algebraic rule, NOT enforced here: 0 = "
         << r2sbml::formulaToInfix(model->getRule(algRules[i])->getMath()) << "\n";
   }
   if (!algRules.empty()) warnAlgebraicUnsolved("rxode2", algRules.size(), true);

   out << "  })\n";
   out << "}\n";

   out.close();
   return 0;
}


// MATLAB resolves a function by file name, so the leading function in a file
// has to be named after it.  Derive that name from the output path and force
// it into a legal identifier: letters, digits and underscores, starting with
// a letter.
static std::string matlabFunctionName(const std::string& path)
{
   std::string base = path;

   std::string::size_type slash = base.find_last_of("/\\");
   if (slash != std::string::npos) base = base.substr(slash + 1);

   std::string::size_type dot = base.find_last_of('.');
   if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);

   for (std::string::size_type i = 0; i < base.size(); i++)
   {
     if (!isalnum((unsigned char)base[i])) base[i] = '_';
   }
   if (base.empty() || !isalpha((unsigned char)base[0])) base = "sbml_" + base;

   return base;
}

// write output ODEs for MATLAB
int writeFileMatlab(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   out << std::setprecision(15);
   Model* model = document->getModel();

   const std::string fname = matlabFunctionName(outfilename);
   const std::vector<int> states = integratedSpecies(model);
   const std::vector<std::string> observed = observedVariables(model);
   const int numStates = (int)states.size();

   const std::vector<int> algRules  = algebraicRules(model);
   const std::vector<int> algStates = algebraicStates(model, states);
   const bool dae = daeIsSquare(algRules, algStates);
   if (!algRules.empty() && !dae) warnAlgebraicUnsolved("MATLAB", algRules.size(), false);

   const std::vector<RateState> rateStates = integratedRateStates(model);
   const int numEntries = numStates + (int)rateStates.size();
   warnAssignmentRuleCompartment(model, "MATLAB");
   warnUndefinedValues(model, "MATLAB");

   int numParams = model->getNumParameters();
   int numCmt    = model->getNumCompartments();
   int numRules  = model->getNumRules();

   out << "% Automatically generated MATLAB model file by r2sbml\n";
   out << "%\n";
   out << "% Save as " << fname << ".m, then solve with:  [t, y"
       << (observed.empty() ? "" : ", obs") << "] = " << fname << "();\n";
   out << "% Columns of y are, in order: ";
   for (int i = 0; i < numStates; i++){
     out << model->getSpecies(states[i])->getIdAttribute()
         << (i + 1 < numEntries ? ", " : "\n");
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     out << rateStates[i].id
         << ((int)(numStates + i + 1) < numEntries ? ", " : "\n");
   }
   if (!observed.empty()){
     out << "% Columns of obs, the assignment-rule variables, are: ";
     for (size_t i = 0; i < observed.size(); i++){
       out << observed[i] << (i + 1 < observed.size() ? ", " : "\n");
     }
   }
   out << "%\n";
   out << "% Model Summary\n";
   out << "%        compartments: " << numCmt << "\n";
   out << "%             species: " << model->getNumSpecies() << "\n";
   out << "%          parameters: " << numParams << "\n";
   out << "%               rules: " << numRules << "\n";
   out << "%              events: " << model->getNumEvents() << "\n\n";

   // A nested function shares every variable its parent also uses, so the
   // parent's own names carry an r2sbml_ prefix: a loop index `k` would
   // otherwise overwrite an SBML parameter `k` that massBalances reads.
   out << "function [t, y" << (observed.empty() ? "" : ", r2sbml_obs") << "] = "
       << fname << "(tspan, y0)\n\n";

   out << "    if nargin < 1 || isempty(tspan)\n";
   out << "        tspan = [0 10];\n";
   out << "    end\n\n";

   out << "    % Compartments\n";
   for (int i = 0; i < numCmt; i++){
     if (compartmentIsVarying(model, model->getCompartment(i)->getIdAttribute())) continue;
     out << "    " << model->getCompartment(i)->getId() << " = "
         << Num{compartmentValue(model->getCompartment(i)), kSpellDefault} << ";\n";
   }
   out << "\n";

   out << "    % Parameters\n";
   const std::vector<int> params = constantParameters(model);
   for (size_t i = 0; i < params.size(); i++){
     out << "    " << model->getParameter(params[i])->getIdAttribute() << " = "
         << Num{model->getParameter(params[i])->getValue(), kSpellDefault} << ";\n";
   }
   out << "\n";

   out << "    % Initial amounts\n";
   out << "    if nargin < 2 || isempty(y0)\n";
   out << "        y0 = [ ...\n";
   for (int i = 0; i < numStates; i++){
     out << "            " << speciesInitialValue(model, model->getSpecies(states[i]))
         << "; % " << model->getSpecies(states[i])->getIdAttribute() << "\n";
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const RateState& c = rateStates[i];
     out << "            " << Num{c.initial, kSpellDefault} << "; % " << c.id
         << " (" << c.what << ")\n";
   }
   out << "        ];\n";
   out << "    end\n\n";

   if (dae){
     // M*y' = f(t, y) with a zero row wherever an algebraic rule fixes the
     // state: that row reads 0 = f_i, which is the constraint.  ode15s solves
     // index-1 systems in this form.
     out << "    % Mass matrix: a zero row marks a state fixed by an algebraic\n";
     out << "    % rule, so that row reads 0 = residual rather than dy/dt = f.\n";
     out << "    M = eye(" << numEntries << ");\n";
     for (size_t i = 0; i < algStates.size(); i++){
       out << "    M(" << algStates[i] + 1 << ", " << algStates[i] + 1 << ") = 0; % "
           << model->getSpecies(states[algStates[i]])->getIdAttribute() << "\n";
     }
     out << "    opts = odeset('Mass', M, 'MassSingular', 'yes');\n\n";
     out << "    [t, y] = ode15s(@massBalances, tspan, y0, opts);\n\n";
   } else {
     out << "    [t, y] = ode15s(@massBalances, tspan, y0);\n\n";
   }

   if (!observed.empty()){
     // ode15s returns states only, so the assignment-rule variables are
     // recomputed from each row of y by the same rules massBalances uses.
     out << "    % Assignment-rule variables at each output time\n";
     out << "    r2sbml_obs = zeros(numel(t), " << observed.size() << ");\n";
     out << "    for r2sbml_i = 1:numel(t)\n";
     out << "        r2sbml_obs(r2sbml_i, :) = observables(t(r2sbml_i), y(r2sbml_i, :).');\n";
     out << "    end\n\n";
   }

   // The state unpacking and assignment rules, shared by massBalances and
   // observables.  Assignment rules come after the unpacking, since they may
   // read states, and before the derivatives, which may read them in turn.
   auto unpackAndAssign = [&](){
     out << "        % States\n";
     for (int i = 0; i < numStates; i++){
       out << "        " << model->getSpecies(states[i])->getIdAttribute()
           << " = states(" << i + 1 << ");\n";
     }
     for (size_t i = 0; i < rateStates.size(); i++){
       out << "        " << rateStates[i].id
           << " = states(" << numStates + (int)i + 1 << ");\n";
     }
     out << "\n";

     bool anyAssignment = false;
     for (int i = 0; i < numRules; i++){
       const Rule* r = model->getRule(i);
       if (r->isAssignment()){
         if (!anyAssignment){ out << "        % Assignment rules\n"; anyAssignment = true; }
         out << "        " << r->getVariable() << " = "
             << r2sbml::formulaToInfixMatlab(r->getMath()) << ";\n";
       }
     }
     if (anyAssignment) out << "\n";
   };

   // Nested, so the compartment and parameter values above stay in scope.
   out << "    function dydt = massBalances(time, states)\n\n";
   unpackAndAssign();

   out << "        % Mass balances\n";
   out << "        dydt = zeros(" << numEntries << ", 1);\n";
   size_t nextAlgRule = 0;
   for (int i = 0; i < numStates; i++){
     const Species* sp = model->getSpecies(states[i]);
     const std::string id = sp->getIdAttribute();
     int rule = rateRuleFor(model, id);
     const bool diluted = !dilutingCompartment(model, sp).empty();
     if (rule >= 0 || diluted){
       out << "        dydt(" << i + 1 << ") = "
           << speciesDerivative(model, sp, r2sbml::formulaToInfixMatlab)
           << "; % " << id << (diluted ? ", with dilution" : "") << "\n";
     } else if (dae && nextAlgRule < algRules.size() &&
                std::find(algStates.begin(), algStates.end(), i) != algStates.end()){
       // Paired with a zero row of M, so this is a constraint, not a rate.
       const Rule* r = model->getRule(algRules[nextAlgRule++]);
       out << "        dydt(" << i + 1 << ") = "
           << r2sbml::formulaToInfixMatlab(r->getMath())
           << "; % algebraic rule fixing " << id << "\n";
     } else {
       out << "        % dydt(" << i + 1 << ") stays 0: " << id
           << " has no rate rule and is held constant\n";
     }
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const std::string cid = rateStates[i].id;
     out << "        dydt(" << numStates + (int)i + 1 << ") = "
         << r2sbml::formulaToInfixMatlab(
                rateStates[i].rate)
         << "; % " << cid << " (" << rateStates[i].what << ")\n";
   }

   if (!dae){
     for (int i = 0; i < numRules; i++){
       const Rule* r = model->getRule(i);
       if (r->isAlgebraic()){
         out << "        % Algebraic rule, NOT enforced here: 0 = "
             << r2sbml::formulaToInfixMatlab(r->getMath()) << "\n";
       }
     }
   }

   out << "\n";
   out << "    end\n";

   if (!observed.empty()){
     out << "\n";
     out << "    function r2sbml_row = observables(time, states)\n\n";
     unpackAndAssign();
     out << "        r2sbml_row = [";
     for (size_t i = 0; i < observed.size(); i++){
       out << observed[i] << (i + 1 < observed.size() ? ", " : "");
     }
     out << "];\n";
     out << "    end\n";
   }
   out << "end\n";

   // A local function after the main one: MATLAB has no conditional
   // expression, and piecewise becomes nested r2sbml_ifelse() calls.
   if (usesPiecewise(model)){
     out << "\nfunction v = r2sbml_ifelse(c, a, b)\n";
     out << "    if c\n";
     out << "        v = a;\n";
     out << "    else\n";
     out << "        v = b;\n";
     out << "    end\n";
     out << "end\n";
   }

   out.close();
   return 0;
}

// write output ODEs for Julia
int writeFileJulia(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   out << std::setprecision(15);
   Model* model = document->getModel();

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();

   const std::vector<int> algRules  = algebraicRules(model);
   const std::vector<int> algStates = algebraicStates(model, states);
   const bool dae = daeIsSquare(algRules, algStates);
   if (!algRules.empty() && !dae) warnAlgebraicUnsolved("Julia", algRules.size(), false);

   const std::vector<RateState> rateStates = integratedRateStates(model);
   const int numEntries = numStates + (int)rateStates.size();
   warnAssignmentRuleCompartment(model, "Julia");
   warnUndefinedValues(model, "Julia");

   int numParams = model->getNumParameters();
   int numCmt    = model->getNumCompartments();
   int numRules  = model->getNumRules();

   out << "# Automatically generated Julia model file by r2sbml\n";
   out << "#\n";
   // A singular mass matrix needs a solver that can take one; the default
   // choice picked by solve(prob) cannot.
   if (dae){
     out << "# Solve with:  using DifferentialEquations; sol = solve(prob, Rodas5())\n";
   } else {
     out << "# Solve with:  using DifferentialEquations; sol = solve(prob)\n";
   }
   out << "# Elements of u are, in order: ";
   for (int i = 0; i < numStates; i++){
     out << model->getSpecies(states[i])->getIdAttribute()
         << (i + 1 < numEntries ? ", " : "\n");
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     out << rateStates[i].id
         << ((int)(numStates + i + 1) < numEntries ? ", " : "\n");
   }
   out << "#\n";
   out << "# Model Summary\n";
   out << "#        compartments: " << numCmt << "\n";
   out << "#             species: " << model->getNumSpecies() << "\n";
   out << "#          parameters: " << numParams << "\n";
   out << "#               rules: " << numRules << "\n";
   out << "#              events: " << model->getNumEvents() << "\n\n";

   const std::vector<std::string> observed = observedVariables(model);
   if (!observed.empty()){
     out << "# Assignment-rule variables at each saved time:\n";
     out << "#   obs = [observables(sol.u[i], p, sol.t[i]) for i in eachindex(sol.t)]\n";
     out << "#\n";
   }

   out << "using DifferentialEquations\n\n";

   // p carries only the constant parameters, so its indices skip any
   // parameter that is integrated in u instead.
   const std::vector<int> params = constantParameters(model);

   // Everything ahead of the mass balances, shared by massbalances! and
   // observables: the time alias, constants, parameters, states, and the
   // assignment rules, which may read any of them.
   auto preamble = [&](){
     // SBML's csymbol for time serialises as `time`; the solver passes `t`.
     out << "    time = t\n\n";

     out << "    # Compartments\n";
     for (int i = 0; i < numCmt; i++){
       if (compartmentIsVarying(model, model->getCompartment(i)->getIdAttribute())) continue;
       out << "    " << model->getCompartment(i)->getId() << " = "
           << Num{compartmentValue(model->getCompartment(i)), kSpellDefault} << "\n";
     }
     out << "\n";

     out << "    # Parameters\n";
     for (size_t i = 0; i < params.size(); i++){
       out << "    " << model->getParameter(params[i])->getIdAttribute()
           << " = p[" << i + 1 << "]\n";
     }
     out << "\n";

     out << "    # States\n";
     for (int i = 0; i < numStates; i++){
       out << "    " << model->getSpecies(states[i])->getIdAttribute()
           << " = u[" << i + 1 << "]\n";
     }
     for (size_t i = 0; i < rateStates.size(); i++){
       out << "    " << rateStates[i].id
           << " = u[" << numStates + (int)i + 1 << "]\n";
     }
     out << "\n";

     bool anyAssignment = false;
     for (int i = 0; i < numRules; i++){
       const Rule* r = model->getRule(i);
       if (r->isAssignment()){
         if (!anyAssignment){ out << "    # Assignment rules\n"; anyAssignment = true; }
         out << "    " << r->getVariable() << " = "
             << r2sbml::formulaToInfixJulia(r->getMath()) << "\n";
       }
     }
     if (anyAssignment) out << "\n";
   };

   out << "function massbalances!(du, u, p, t)\n\n";
   preamble();

   out << "    # Mass balances\n";
   size_t nextAlgRule = 0;
   for (int i = 0; i < numStates; i++){
     const Species* sp = model->getSpecies(states[i]);
     const std::string id = sp->getIdAttribute();
     int rule = rateRuleFor(model, id);
     const bool diluted = !dilutingCompartment(model, sp).empty();
     if (rule >= 0 || diluted){
       out << "    du[" << i + 1 << "] = "
           << speciesDerivative(model, sp, r2sbml::formulaToInfixJulia)
           << "  # " << id << (diluted ? ", with dilution" : "") << "\n";
     } else if (dae && nextAlgRule < algRules.size() &&
                std::find(algStates.begin(), algStates.end(), i) != algStates.end()){
       // Paired with a zero row of M, so this row reads 0 = residual.
       const Rule* r = model->getRule(algRules[nextAlgRule++]);
       out << "    du[" << i + 1 << "] = "
           << r2sbml::formulaToInfixJulia(r->getMath())
           << "  # algebraic rule fixing " << id << "\n";
     } else {
       out << "    du[" << i + 1 << "] = 0.0  # " << id
           << " has no rate rule and is held constant\n";
     }
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const std::string cid = rateStates[i].id;
     out << "    du[" << numStates + (int)i + 1 << "] = "
         << r2sbml::formulaToInfixJulia(rateStates[i].rate)
         << "  # " << cid << " (" << rateStates[i].what << ")\n";
   }

   if (!dae){
     for (int i = 0; i < numRules; i++){
       const Rule* r = model->getRule(i);
       if (r->isAlgebraic()){
         out << "    # Algebraic rule, NOT enforced here: 0 = "
             << r2sbml::formulaToInfixJulia(r->getMath()) << "\n";
       }
     }
   }

   out << "\n";
   out << "    return nothing\n";
   out << "end\n\n";

   // The solution holds states only; this recomputes the assignment-rule
   // variables from a saved state with the same rules massbalances! uses.
   if (!observed.empty()){
     out << "function observables(u, p, t)\n\n";
     preamble();
     out << "    return (";
     for (size_t i = 0; i < observed.size(); i++){
       out << observed[i] << " = " << observed[i]
           << (i + 1 < observed.size() ? ", " : "");
     }
     // a one-element NamedTuple needs the trailing comma
     out << (observed.size() == 1 ? ",)" : ")") << "\n";
     out << "end\n\n";
   }

   // Float64[] rather than [], so a model whose initial amounts happen to be
   // whole numbers still gets a floating point state vector.
   out << "u0 = Float64[";
   for (int i = 0; i < numStates; i++){
     out << speciesInitialValue(model, model->getSpecies(states[i]))
         << (i + 1 < numEntries ? ", " : "");
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     out << Num{rateStates[i].initial, kSpellDefault}
         << ((int)(numStates + i + 1) < numEntries ? ", " : "");
   }
   out << "]\n";

   out << "p = Float64[";
   for (size_t i = 0; i < params.size(); i++){
     out << Num{model->getParameter(params[i])->getValue(), kSpellDefault} << (i + 1 < params.size() ? ", " : "");
   }
   out << "]\n";

   out << "tspan = (0.0, 10.0)\n";
   if (dae){
     out << "\n# Mass matrix: a zero row marks a state fixed by an algebraic rule,\n";
     out << "# so that row reads 0 = residual rather than du = f.\n";
     out << "M = zeros(" << numEntries << ", " << numEntries << ")\n";
     for (int i = 0; i < numStates; i++){
       if (std::find(algStates.begin(), algStates.end(), i) == algStates.end()){
         out << "M[" << i + 1 << ", " << i + 1 << "] = 1.0  # "
             << model->getSpecies(states[i])->getIdAttribute() << "\n";
       }
     }
     for (size_t i = 0; i < rateStates.size(); i++){
       out << "M[" << numStates + (int)i + 1 << ", " << numStates + (int)i + 1
           << "] = 1.0  # " << rateStates[i].id << "\n";
     }
     out << "\nmassbalances = ODEFunction(massbalances!; mass_matrix = M)\n";
     out << "prob = ODEProblem(massbalances, u0, tspan, p)\n";
   } else {
     out << "prob = ODEProblem(massbalances!, u0, tspan, p)\n";
   }

   out.close();
   return 0;
}

// --- piecewise in ubiquity -------------------------------------------------
//
// ubiquity has no conditional *expression*.  Its conditional is a statement
// that defines a whole dynamic secondary parameter:
//
//     <Ad> kel = 0.0
//     <IF:kel:COND> SIMINT_GT[Cp][Cth]; kelf
//     <IF:kel:ELSE> kels
//
// So every piecewise is hoisted out into a parameter of its own, named
// r2sbml_pw<n>, and the expression reads that name instead.  Children are
// hoisted first, so a piecewise nested inside another is defined before the
// one that reads it.

typedef std::vector<std::pair<std::string, ASTNode*> > HoistedPiecewise;

static ASTNode* hoistPiecewise(ASTNode* node, HoistedPiecewise& hoisted, int& counter)
{
   for (unsigned int i = 0; i < node->getNumChildren(); i++)
   {
     ASTNode* child = node->getChild(i);
     ASTNode* now   = hoistPiecewise(child, hoisted, counter);
     if (now != child) node->replaceChild(i, now, true);
   }
   if (node->getType() != AST_FUNCTION_PIECEWISE) return node;

   const std::string name = "r2sbml_pw" + std::to_string(++counter);
   hoisted.push_back(std::make_pair(name, node->deepCopy()));
   return r2sbml::nameNode(name.c_str());
}

// The <Ad>/<IF> block defining each hoisted piecewise; frees the copies.  With
// no `otherwise` SBML leaves the value undefined, so ELSE is NaN.
static void writeHoistedPiecewise(std::ostream& out, HoistedPiecewise& hoisted)
{
   for (size_t h = 0; h < hoisted.size(); h++)
   {
     const std::string& name = hoisted[h].first;
     const ASTNode*     pw   = hoisted[h].second;
     const unsigned int kids = pw->getNumChildren();

     out << "<Ad> " << name << " = 0.0   # piecewise, defined by the <IF> lines below\n";
     for (unsigned int p = 0; p + 1 < kids; p += 2)
     {
       out << "<IF:" << name << ":COND> "
           << r2sbml::formulaToUbiquity(pw->getChild(p + 1)) << "; "
           << r2sbml::formulaToUbiquity(pw->getChild(p)) << "\n";
     }
     out << "<IF:" << name << ":ELSE> "
         << (kids % 2 == 1 ? r2sbml::formulaToUbiquity(pw->getChild(kids - 1))
                           : std::string(kSpellUbiquity.nan)) << "\n";
     delete hoisted[h].second;
   }
   hoisted.clear();
}

// write output ODEs for ubiquity
int writeFileUbiquity(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   out << std::setprecision(15);
   Model* model = document->getModel();

   // Hoist every piecewise out of the rules first: the rules' math is what
   // integratedRateStates() and speciesDerivative() read, and RateState keeps
   // a pointer into it, so this has to happen before either runs.  Only this
   // writer sees the converted document, so rewriting it in place is safe.
   std::vector<HoistedPiecewise> rulePiecewise(model->getNumRules());
   int pwCounter = 0;
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     Rule* r = model->getRule(i);
     if (!r->isSetMath() || !r2sbml::hasPiecewise(r->getMath())) continue;
     ASTNode* copy = r->getMath()->deepCopy();
     ASTNode* root = hoistPiecewise(copy, rulePiecewise[i], pwCounter);
     r->setMath(root);
     if (root != copy) delete copy;
     delete root;
   }

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();
   const std::vector<RateState> rateStates = integratedRateStates(model);
   warnAssignmentRuleCompartment(model, "ubiquity");
   warnUndefinedValues(model, "ubiquity");

   int numParams = model->getNumParameters();
   int numCmt    = model->getNumCompartments();
   int numRules  = model->getNumRules();

   // Constructs with no ubiquity spelling pass through as plain calls, which
   // build_system() accepts but the generated C then fails to compile, with an
   // error naming a shared object rather than the model.  Say so up front.
   std::vector<std::string> unsupported;
   for (int i = 0; i < numRules; i++){
     if (model->getRule(i)->isSetMath()){
       r2sbml::ubiquityUnsupported(model->getRule(i)->getMath(), unsupported);
     }
     // the hoisted piecewise bodies are written out too
     for (size_t h = 0; h < rulePiecewise[i].size(); h++){
       const ASTNode* pw = rulePiecewise[i][h].second;
       for (unsigned int k = 0; k < pw->getNumChildren(); k++){
         r2sbml::ubiquityUnsupported(pw->getChild(k), unsupported);
       }
     }
   }
   std::sort(unsupported.begin(), unsupported.end());
   unsupported.erase(std::unique(unsupported.begin(), unsupported.end()),
                     unsupported.end());

   out << "# Automatically generated ubiquity system file by r2sbml\n";
   out << "#\n";
   out << "# Build with:  cfg <- build_system(system_file = \"<this file>\")\n";
   out << "#\n";

   if (!unsupported.empty()){
     std::string list;
     for (size_t i = 0; i < unsupported.size(); i++){
       if (i) list += ", ";
       list += unsupported[i];
     }
     out << "# WARNING: this model uses constructs with no ubiquity equivalent,\n";
     out << "#          left below as plain calls: " << list << "\n";
     out << "#          The file will not build until they are replaced by hand.\n";
     out << "#\n";
     Rcpp::warning("ubiquity has no equivalent for: " + list +
                   ". They were written out unchanged and the system file "
                   "will not build until they are replaced.");
   }

   out << "# Model Summary\n";
   out << "#        compartments: " << numCmt << "\n";
   out << "#             species: " << model->getNumSpecies() << "\n";
   out << "#          parameters: " << numParams << "\n";
   out << "#               rules: " << numRules << "\n";
   out << "#              events: " << model->getNumEvents() << "\n\n";

   // ubiquity has no compartment concept of its own: a compartment volume is
   // just a constant the rate expressions divide by, so it becomes a <P>.
   // Bounds are -inf/inf rather than the eps/inf seen in hand-written systems,
   // because an SBML value may legitimately be zero or negative and a lower
   // bound above the value is inconsistent.  They only matter for estimation.
   out << "# Compartment volumes\n";
   for (int i = 0; i < numCmt; i++){
     const Compartment* c = model->getCompartment(i);
     // a varying compartment is a state (<I>/<ODE:>) or a dynamic secondary
     // parameter (<Ad>), not a constant
     if (compartmentIsVarying(model, c->getIdAttribute())) continue;
     out << "<P> " << c->getId() << " " << Num{compartmentValue(c), kSpellDefault}
         << " -inf inf " << (c->isSetUnits() ? c->getUnits() : "1")
         << " yes System\n";
   }
   out << "\n";

   out << "# Parameters\n";
   const std::vector<int> params = constantParameters(model);
   for (size_t i = 0; i < params.size(); i++){
     const Parameter* p = model->getParameter(params[i]);
     out << "<P> " << p->getIdAttribute() << " " << Num{p->getValue(), kSpellDefault}
         << " -inf inf " << (p->isSetUnits() ? p->getUnits() : "1")
         << " yes System\n";
   }
   out << "\n";

   out << "# Initial conditions\n";
   for (int i = 0; i < numStates; i++){
     out << "<I> " << model->getSpecies(states[i])->getIdAttribute() << " = "
         << speciesInitialValue(model, model->getSpecies(states[i])) << "\n";
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const RateState& c = rateStates[i];
     out << "<I> " << c.id << " = " << Num{c.initial, kSpellUbiquity}
         << "   # " << c.what << ", integrated\n";
   }
   out << "\n";

   // Assignment rules read states, so they are dynamic secondary parameters.
   // A hoisted piecewise is defined just before the rule that reads it, so
   // it can read the assignment rules ahead of that one.
   bool anyAssignment = false;
   for (int i = 0; i < numRules; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       if (!anyAssignment){ out << "# Assignment rules\n"; anyAssignment = true; }
       writeHoistedPiecewise(out, rulePiecewise[i]);
       out << "<Ad> " << r->getVariable() << " = "
           << r2sbml::formulaToUbiquity(r->getMath()) << "\n";
     }
   }
   if (anyAssignment) out << "\n";

   // Rate and algebraic rules are written below, after every <Ad>.
   bool anyHoisted = false;
   for (int i = 0; i < numRules; i++){
     if (rulePiecewise[i].empty()) continue;
     if (!anyHoisted){ out << "# piecewise expressions in the mass balances\n"; anyHoisted = true; }
     writeHoistedPiecewise(out, rulePiecewise[i]);
   }
   if (anyHoisted) out << "\n";

   out << "# Mass balances\n";
   for (int i = 0; i < numStates; i++){
     const Species* sp = model->getSpecies(states[i]);
     const std::string id = sp->getIdAttribute();
     int rule = rateRuleFor(model, id);
     const bool diluted = !dilutingCompartment(model, sp).empty();
     if (rule >= 0 || diluted){
       out << "<ODE:" << id << "> "
           << speciesDerivative(model, sp, r2sbml::formulaToUbiquity)
           << (diluted ? "   # includes dilution by the changing volume" : "") << "\n";
     } else {
       // An <I> alone would leave the state with no equation; be explicit.
       out << "<ODE:" << id << "> 0   # no rate rule, held constant\n";
     }
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const std::string cid = rateStates[i].id;
     out << "<ODE:" << cid << "> "
         << r2sbml::formulaToUbiquity(rateStates[i].rate)
         << "   # " << rateStates[i].what << "\n";
   }

   // ubiquity integrates ODEs only, so a constraint can only be recorded.
   const std::vector<int> algRules = algebraicRules(model);
   for (size_t i = 0; i < algRules.size(); i++){
     out << "# Algebraic rule, NOT enforced here: 0 = "
         << r2sbml::formulaToUbiquity(model->getRule(algRules[i])->getMath()) << "\n";
   }
   if (!algRules.empty()) warnAlgebraicUnsolved("ubiquity", algRules.size(), true);

   out << "\n";
   out << "# Outputs\n";
   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     out << "<O> " << id << "_out = " << id << "\n";
   }
   for (size_t i = 0; i < rateStates.size(); i++){
     const std::string cid = rateStates[i].id;
     out << "<O> " << cid << "_out = " << cid << "\n";
   }
   // the assignment-rule variables, which are <Ad> secondary parameters and
   // so are not in the state vector the outputs above cover
   const std::vector<std::string> observed = observedVariables(model);
   for (size_t i = 0; i < observed.size(); i++){
     out << "<O> " << observed[i] << "_out = " << observed[i] << "\n";
   }

   out.close();
   return 0;
}
