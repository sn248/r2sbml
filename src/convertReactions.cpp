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
#include <chrono>
#include <ctime>
#include <sbml/SBMLTypes.h>
#include <sbml/conversion/ConversionProperties.h>

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
static double speciesInitialValue(const Model* model, const Species* species)
{
   const Compartment* c = model->getCompartment(species->getCompartment());

   // A zero-dimensional compartment has no volume to convert through, and a
   // size of 0 would only turn the conversion into an inf or a NaN.
   double volume = 1.0;
   if (c != NULL && c->getSpatialDimensions() > 0 &&
       c->isSetSize() && c->getSize() != 0.0)
   {
     volume = c->getSize();
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
static int rateRuleForSpecies(Model* model, const std::string& speciesId)
{
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     const Rule* r = model->getRule(i);
     if (r->isRate() && r->getVariable() == speciesId) return (int)i;
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

//'convertReactions
//'@param infile input file name
//'@param outfile output file name
//'@param format output code format, one of 'R'/'deSolve' (default), 'mrgsolve',
//' 'nlmixr2' (or 'rxode'), 'MATLAB', 'Julia' or 'ubiquity'.  An unrecognised format is an error.
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

   unsigned int  errors    = document->getNumErrors(LIBSBML_SEV_ERROR);

   // stop in case of errors
   if (errors > 0)
   {
     Rcpp::Rcerr << "Encountered the following SBML errors:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion skipped.  Please correct the problems above first.");
   }

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
   else if (format.compare("nlmixr2") == 0 || format.compare("rxode") == 0)  {
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
                 "'mrgsolve', 'nlmixr2'/'rxode', 'MATLAB', 'Julia' or 'ubiquity'.");
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
     if (!s->getConstant() && rateRuleForSpecies(model, s->getIdAttribute()) < 0)
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
        out <<  model->getCompartment(i)->getId() << " = " << model->getCompartment(i)->getVolume() << endl; //" # (" << model->getCompartment(i)->getUnits() << ")" << endl;
   }

   out << endl;

   out <<"## Initial Amounts" << endl;
   out << "InitialAmounts <- c(" << endl;
   for (int i = 0; i < numStates; i++){
      const Species* s = model->getSpecies(states[i]);
      out << "         " << s->getIdAttribute() << " = " << speciesInitialValue(model, s)
          << (i + 1 < numStates ? "," : "") << endl;
   }

  out << "                    )" << endl;

   out << endl;

   out << "## Parameters" << endl;
   out << "parameters <- c(" << endl;
   int numParams = model->getNumParameters();
   for (int i = 0; i < numParams; i++){
      if (i != numParams-1 ){
        out << "         " << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << "," << endl;
      }
      if (i == numParams-1) {
        out << "         " << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << endl;
      }
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
     out << "InitialDerivatives <- with(as.list(c(InitialAmounts, parameters)), c(" << endl;
     for (int i = 0; i < numStates; i++){
       const std::string id = model->getSpecies(states[i])->getIdAttribute();
       int rule = rateRuleForSpecies(model, id);
       out << "         " << id << " = "
           << (rule >= 0 ? r2sbml::formulaToInfix(model->getRule(rule)->getMath()) : "0")
           << (i + 1 < numStates ? "," : "") << endl;
     }
     out << "                    ))" << endl << endl;

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
        out << "   " << model->getSpecies(states[i])->getIdAttribute() << " = states[[\"" << model->getSpecies(states[i])->getIdAttribute() << "\"]]" << endl;
   }

   out << endl;
   out << "   ## Get Parameter Names " << endl;
   for (int i = 0; i<numParams; i++){
        out << "   " << model->getParameter(i)->getIdAttribute() << " = params[[\"" << model->getParameter(i)->getIdAttribute() << "\"]]" << endl;
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
       int rule = rateRuleForSpecies(model, id);
       if (rule >= 0){
         out << "   res_" << id << " = derivs[[\"" << id << "\"]] - ("
             << r2sbml::formulaToInfix(model->getRule(rule)->getMath()) << ")" << endl;
       } else if (nextAlgRule < algRules.size() &&
                  std::find(algStates.begin(), algStates.end(), i) != algStates.end()){
         const Rule* r = model->getRule(algRules[nextAlgRule++]);
         out << "   res_" << id << " = "
             << r2sbml::formulaToInfix(r->getMath())
             << "  ## algebraic rule fixing " << id << endl;
       } else {
         out << "   res_" << id << " = derivs[[\"" << id << "\"]]  ## " << id
             << " is constant" << endl;
       }
     }

     out << endl;
     out << "   ## Make a list of Residuals" << endl;
     out << "   Residuals <- c(" << endl;
     for (int i = 0; i < numStates; i++){
        out << "     res_" << model->getSpecies(states[i])->getIdAttribute()
            << (i + 1 < numStates ? " ," : "") << endl;
     }
     out << "   )" << endl;
     out << "   return(list(Residuals))" << endl;
     out << endl << endl << "}" << endl << endl;
     out.close();
     return 0;
   }

   out << "   ## Mass Balances" << endl;
   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     if (rule >= 0){
       out << "   d" << id << "_dt = "
           << r2sbml::formulaToInfix(model->getRule(rule)->getMath()) << endl;
     } else {
       out << "   d" << id << "_dt = 0  ## " << id
           << " has no rate rule and is held constant" << endl;
     }
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
          << (i + 1 < numStates ? " ," : "") << endl;
   }
   out << "   )" << endl;
   out << "   return(list(MassBalances))" << endl;
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
int writeFileMrgsolve(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   Model* model = document->getModel();

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();

   out << "## Automatically generated mrgsolve model file by r2sbml\n" << endl;
   out << "$PROB\n" << endl;
   out << "$PARAM\n";

   int numParams = model->getNumParameters();
   for (int i = 0; i < numParams; i++){
        out << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << "\n";
   }

   out << "\n$CMT\n";
   for (int i = 0; i < numStates; i++){
        out << model->getSpecies(states[i])->getIdAttribute() << "\n";
   }

   out << "\n$MAIN\n";
   for (int i = 0; i < numStates; i++){
        out << model->getSpecies(states[i])->getIdAttribute() << "_0 = "
            << speciesInitialValue(model, model->getSpecies(states[i])) << ";\n";
   }

   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        out <<  model->getCompartment(i)->getId() << " = " << model->getCompartment(i)->getVolume() << ";\n";
   }

   out << "\n$ODE\n";
   int numODEs = model->getNumRules();

   // Assignment rules read states, so they are locals inside $ODE rather than
   // compartments.  They have to precede the derivatives, which read them.
   for (int i = 0; i < numODEs; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       out << "double " << r->getVariable() << " = "
           << r2sbml::formulaToInfixC(r->getMath()) << ";\n";
     }
   }

   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     // mrgsolve model blocks are C++, so powers have to be pow() calls.
     if (rule >= 0){
       out << "dxdt_" << id << " = "
           << r2sbml::formulaToInfixC(model->getRule(rule)->getMath()) << ";\n";
     } else {
       out << "dxdt_" << id << " = 0; // " << id
           << " has no rate rule and is held constant\n";
     }
   }

   // mrgsolve integrates ODEs only, so a constraint can only be recorded.
   const std::vector<int> algRules = algebraicRules(model);
   for (size_t i = 0; i < algRules.size(); i++){
     out << "// Algebraic rule, NOT enforced here: 0 = "
         << r2sbml::formulaToInfixC(model->getRule(algRules[i])->getMath()) << "\n";
   }
   if (!algRules.empty()) warnAlgebraicUnsolved("mrgsolve", algRules.size(), true);

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

   out << "## Automatically generated nlmixr2/rxode model file by r2sbml\n" << endl;
   out << "model <- function() {\n";
   out << "  ini({\n";

   int numParams = model->getNumParameters();
   for (int i = 0; i < numParams; i++){
        out << "    " << model->getParameter(i)->getIdAttribute() << " <- " << model->getParameter(i)->getValue() << "\n";
   }

   out << "  })\n";
   out << "  model({\n";

   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        out << "    " << model->getCompartment(i)->getId() << " <- " << model->getCompartment(i)->getVolume() << "\n";
   }

   for (int i = 0; i < numStates; i++){
        out << "    " << model->getSpecies(states[i])->getIdAttribute() << "(0) <- "
            << speciesInitialValue(model, model->getSpecies(states[i])) << "\n";
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
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     if (rule >= 0){
       out << "    d/dt(" << id << ") <- "
           << r2sbml::formulaToInfix(model->getRule(rule)->getMath()) << "\n";
     } else {
       out << "    d/dt(" << id << ") <- 0 # " << id
           << " has no rate rule and is held constant\n";
     }
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
   const int numStates = (int)states.size();

   const std::vector<int> algRules  = algebraicRules(model);
   const std::vector<int> algStates = algebraicStates(model, states);
   const bool dae = daeIsSquare(algRules, algStates);
   if (!algRules.empty() && !dae) warnAlgebraicUnsolved("MATLAB", algRules.size(), false);

   int numParams = model->getNumParameters();
   int numCmt    = model->getNumCompartments();
   int numRules  = model->getNumRules();

   out << "% Automatically generated MATLAB model file by r2sbml\n";
   out << "%\n";
   out << "% Save as " << fname << ".m, then solve with:  [t, y] = " << fname << "();\n";
   out << "% Columns of y are, in order: ";
   for (int i = 0; i < numStates; i++){
     out << model->getSpecies(states[i])->getIdAttribute()
         << (i + 1 < numStates ? ", " : "\n");
   }
   out << "%\n";
   out << "% Model Summary\n";
   out << "%        compartments: " << numCmt << "\n";
   out << "%             species: " << model->getNumSpecies() << "\n";
   out << "%          parameters: " << numParams << "\n";
   out << "%               rules: " << numRules << "\n";
   out << "%              events: " << model->getNumEvents() << "\n\n";

   out << "function [t, y] = " << fname << "(tspan, y0)\n\n";

   out << "    if nargin < 1 || isempty(tspan)\n";
   out << "        tspan = [0 10];\n";
   out << "    end\n\n";

   out << "    % Compartments\n";
   for (int i = 0; i < numCmt; i++){
     out << "    " << model->getCompartment(i)->getId() << " = "
         << model->getCompartment(i)->getVolume() << ";\n";
   }
   out << "\n";

   out << "    % Parameters\n";
   for (int i = 0; i < numParams; i++){
     out << "    " << model->getParameter(i)->getIdAttribute() << " = "
         << model->getParameter(i)->getValue() << ";\n";
   }
   out << "\n";

   out << "    % Initial amounts\n";
   out << "    if nargin < 2 || isempty(y0)\n";
   out << "        y0 = [ ...\n";
   for (int i = 0; i < numStates; i++){
     out << "            " << speciesInitialValue(model, model->getSpecies(states[i]))
         << "; % " << model->getSpecies(states[i])->getIdAttribute() << "\n";
   }
   out << "        ];\n";
   out << "    end\n\n";

   if (dae){
     // M*y' = f(t, y) with a zero row wherever an algebraic rule fixes the
     // state: that row reads 0 = f_i, which is the constraint.  ode15s solves
     // index-1 systems in this form.
     out << "    % Mass matrix: a zero row marks a state fixed by an algebraic\n";
     out << "    % rule, so that row reads 0 = residual rather than dy/dt = f.\n";
     out << "    M = eye(" << numStates << ");\n";
     for (size_t i = 0; i < algStates.size(); i++){
       out << "    M(" << algStates[i] + 1 << ", " << algStates[i] + 1 << ") = 0; % "
           << model->getSpecies(states[algStates[i]])->getIdAttribute() << "\n";
     }
     out << "    opts = odeset('Mass', M, 'MassSingular', 'yes');\n\n";
     out << "    [t, y] = ode15s(@massBalances, tspan, y0, opts);\n\n";
   } else {
     out << "    [t, y] = ode15s(@massBalances, tspan, y0);\n\n";
   }

   // Nested, so the compartment and parameter values above stay in scope.
   out << "    function dydt = massBalances(time, states)\n\n";

   out << "        % States\n";
   for (int i = 0; i < numStates; i++){
     out << "        " << model->getSpecies(states[i])->getIdAttribute()
         << " = states(" << i + 1 << ");\n";
   }
   out << "\n";

   // Assignment rules come after the state unpacking, since they may read
   // states, and before the derivatives, which may read them in turn.
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

   out << "        % Mass balances\n";
   out << "        dydt = zeros(" << numStates << ", 1);\n";
   size_t nextAlgRule = 0;
   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     if (rule >= 0){
       out << "        dydt(" << i + 1 << ") = "
           << r2sbml::formulaToInfixMatlab(model->getRule(rule)->getMath())
           << "; % " << id << "\n";
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
   out << "end\n";

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
         << (i + 1 < numStates ? ", " : "\n");
   }
   out << "#\n";
   out << "# Model Summary\n";
   out << "#        compartments: " << numCmt << "\n";
   out << "#             species: " << model->getNumSpecies() << "\n";
   out << "#          parameters: " << numParams << "\n";
   out << "#               rules: " << numRules << "\n";
   out << "#              events: " << model->getNumEvents() << "\n\n";

   out << "using DifferentialEquations\n\n";

   out << "function massbalances!(du, u, p, t)\n\n";

   // SBML's csymbol for time serialises as `time`; the solver passes `t`.
   out << "    time = t\n\n";

   out << "    # Compartments\n";
   for (int i = 0; i < numCmt; i++){
     out << "    " << model->getCompartment(i)->getId() << " = "
         << model->getCompartment(i)->getVolume() << "\n";
   }
   out << "\n";

   out << "    # Parameters\n";
   for (int i = 0; i < numParams; i++){
     out << "    " << model->getParameter(i)->getIdAttribute()
         << " = p[" << i + 1 << "]\n";
   }
   out << "\n";

   out << "    # States\n";
   for (int i = 0; i < numStates; i++){
     out << "    " << model->getSpecies(states[i])->getIdAttribute()
         << " = u[" << i + 1 << "]\n";
   }
   out << "\n";

   bool anyAssignment = false;
   for (int i = 0; i < numRules; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       if (!anyAssignment){ out << "    # Assignment rules\n"; anyAssignment = true; }
       out << "    " << r->getVariable() << " = "
           << r2sbml::formulaToInfix(r->getMath()) << "\n";
     }
   }
   if (anyAssignment) out << "\n";

   out << "    # Mass balances\n";
   size_t nextAlgRule = 0;
   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     if (rule >= 0){
       out << "    du[" << i + 1 << "] = "
           << r2sbml::formulaToInfix(model->getRule(rule)->getMath())
           << "  # " << id << "\n";
     } else if (dae && nextAlgRule < algRules.size() &&
                std::find(algStates.begin(), algStates.end(), i) != algStates.end()){
       // Paired with a zero row of M, so this row reads 0 = residual.
       const Rule* r = model->getRule(algRules[nextAlgRule++]);
       out << "    du[" << i + 1 << "] = "
           << r2sbml::formulaToInfix(r->getMath())
           << "  # algebraic rule fixing " << id << "\n";
     } else {
       out << "    du[" << i + 1 << "] = 0.0  # " << id
           << " has no rate rule and is held constant\n";
     }
   }

   if (!dae){
     for (int i = 0; i < numRules; i++){
       const Rule* r = model->getRule(i);
       if (r->isAlgebraic()){
         out << "    # Algebraic rule, NOT enforced here: 0 = "
             << r2sbml::formulaToInfix(r->getMath()) << "\n";
       }
     }
   }

   out << "\n";
   out << "    return nothing\n";
   out << "end\n\n";

   // Float64[] rather than [], so a model whose initial amounts happen to be
   // whole numbers still gets a floating point state vector.
   out << "u0 = Float64[";
   for (int i = 0; i < numStates; i++){
     out << speciesInitialValue(model, model->getSpecies(states[i]))
         << (i + 1 < numStates ? ", " : "");
   }
   out << "]\n";

   out << "p = Float64[";
   for (int i = 0; i < numParams; i++){
     out << model->getParameter(i)->getValue() << (i + 1 < numParams ? ", " : "");
   }
   out << "]\n";

   out << "tspan = (0.0, 10.0)\n";
   if (dae){
     out << "\n# Mass matrix: a zero row marks a state fixed by an algebraic rule,\n";
     out << "# so that row reads 0 = residual rather than du = f.\n";
     out << "M = zeros(" << numStates << ", " << numStates << ")\n";
     for (int i = 0; i < numStates; i++){
       if (std::find(algStates.begin(), algStates.end(), i) == algStates.end()){
         out << "M[" << i + 1 << ", " << i + 1 << "] = 1.0  # "
             << model->getSpecies(states[i])->getIdAttribute() << "\n";
       }
     }
     out << "\nmassbalances = ODEFunction(massbalances!; mass_matrix = M)\n";
     out << "prob = ODEProblem(massbalances, u0, tspan, p)\n";
   } else {
     out << "prob = ODEProblem(massbalances!, u0, tspan, p)\n";
   }

   out.close();
   return 0;
}

// write output ODEs for ubiquity
int writeFileUbiquity(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   out << std::setprecision(15);
   Model* model = document->getModel();

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();

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
     out << "<P> " << c->getId() << " " << c->getVolume()
         << " -inf inf " << (c->isSetUnits() ? c->getUnits() : "1")
         << " yes System\n";
   }
   out << "\n";

   out << "# Parameters\n";
   for (int i = 0; i < numParams; i++){
     const Parameter* p = model->getParameter(i);
     out << "<P> " << p->getIdAttribute() << " " << p->getValue()
         << " -inf inf " << (p->isSetUnits() ? p->getUnits() : "1")
         << " yes System\n";
   }
   out << "\n";

   out << "# Initial conditions\n";
   for (int i = 0; i < numStates; i++){
     out << "<I> " << model->getSpecies(states[i])->getIdAttribute() << " = "
         << speciesInitialValue(model, model->getSpecies(states[i])) << "\n";
   }
   out << "\n";

   // Assignment rules read states, so they are dynamic secondary parameters.
   bool anyAssignment = false;
   for (int i = 0; i < numRules; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       if (!anyAssignment){ out << "# Assignment rules\n"; anyAssignment = true; }
       out << "<Ad> " << r->getVariable() << " = "
           << r2sbml::formulaToUbiquity(r->getMath()) << "\n";
     }
   }
   if (anyAssignment) out << "\n";

   out << "# Mass balances\n";
   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     if (rule >= 0){
       out << "<ODE:" << id << "> "
           << r2sbml::formulaToUbiquity(model->getRule(rule)->getMath()) << "\n";
     } else {
       // An <I> alone would leave the state with no equation; be explicit.
       out << "<ODE:" << id << "> 0   # no rate rule, held constant\n";
     }
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

   out.close();
   return 0;
}
