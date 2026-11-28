/* Editing a loaded model in place: parameter values, compartment sizes,
 * species initial values and rate laws, and removing reactions.  Each
 * function returns the model pointer invisibly, so calls chain, and mutates
 * the document the pointer owns -- every R variable holding the same pointer
 * sees the change.
 */

#include <Rcpp.h>
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <sbml/SBMLTypes.h>

using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

static Model* editableModel(SEXP input_model) {
  Model* model = Rcpp::XPtr<Model>(input_model);
  if (model == NULL) Rcpp::stop("No model present.");
  return model;
}

// A single number.  NA -- logical, integer or double -- means "unset the
// attribute", and is reported through `unset`; NaN and the infinities are
// values SBML can hold, so they are set like any other.
static double editValue(SEXP value, bool& unset) {
  unset = false;
  if (Rf_length(value) != 1 || Rf_isFactor(value))
    Rcpp::stop("`value` must be a single number, or NA to unset it.");
  if (Rf_isLogical(value)) {
    if (LOGICAL(value)[0] == NA_LOGICAL) { unset = true; return NA_REAL; }
    Rcpp::stop("`value` must be a single number, or NA to unset it.");
  }
  if (!Rf_isNumeric(value))
    Rcpp::stop("`value` must be a single number, or NA to unset it.");
  const double v = Rcpp::as<double>(value);
  if (R_IsNA(v)) unset = true;
  return v;
}

// What, if anything, overrides the declared value of `id`.  An initial
// assignment or an assignment rule replaces it, so a new value has no effect;
// a rate rule integrates from it, so it does.  Math-less rules and initial
// assignments are ignored, as Level 3 Version 2 specifies and as
// convertReactions() does.
static std::string overriddenBy(const Model* model, const std::string& id) {
  const InitialAssignment* ia = model->getInitialAssignment(id);
  if (ia != NULL && ia->isSetMath()) return "an initial assignment";
  const Rule* rule = model->getRule(id);
  if (rule != NULL && rule->isAssignment() && rule->isSetMath())
    return "an assignment rule";
  return "";
}

static void warnIfOverridden(const Model* model, const std::string& what,
                             const std::string& id) {
  const std::string by = overriddenBy(model, id);
  if (by != "")
    Rcpp::warning("The new value of " + what + " '" + id +
                  "' has no effect: " + by + " sets it.");
}

//'Edit a model's values
//'
//'Set the value of a parameter, the size of a compartment, or the initial
//'value of a species, in place.
//'
//'The model is changed in place, unlike an ordinary R object: every variable
//'that holds the same pointer sees the edit. Use `copyModel()` first to keep
//'the original. The query functions show the edit straight away, and
//'`convertReactions()` and `writeSBML()` use it.
//'
//'A species carries an initial amount or an initial concentration, never
//'both, so `setSpeciesIC()` needs to be told which `value` is: it sets that
//'attribute and unsets the other.
//'
//'An unknown `id` is an error. When an initial assignment or an assignment
//'rule sets the component, the new value is stored but has no effect, and a
//'warning says so; a rate rule starts from the new value, so it does have
//'one.
//'@param input_model a model pointer from `getModel()`
//'@param id the id of the parameter, compartment or species
//'@param value the new value, a single number; `NA` unsets it
//'@param as for `setSpeciesIC()`, whether `value` is an `"amount"` or a
//'  `"concentration"`
//'@return the model pointer, invisibly, so calls can be chained
//'@examples
//'sbml_file <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'setParameterValue(model, "k1", 1000)
//'setCompartmentSize(model, "nucleus", 2)
//'setSpeciesIC(model, "X", 0.5, as = "concentration")
//'getParameterTable(model)
//'getSpeciesTable(model)
//'@name editModel
//'@rdname editModel
// [[Rcpp::export(invisible = true)]]
SEXP setParameterValue(SEXP input_model, std::string id, SEXP value) {
  Model* model = editableModel(input_model);
  Parameter* p = model->getParameter(id);
  if (p == NULL) Rcpp::stop("No parameter '" + id + "' in the model.");

  bool unset;
  const double v = editValue(value, unset);
  if (unset) p->unsetValue(); else p->setValue(v);

  warnIfOverridden(model, "parameter", id);
  return input_model;
}

//'@rdname editModel
// [[Rcpp::export(invisible = true)]]
SEXP setCompartmentSize(SEXP input_model, std::string id, SEXP value) {
  Model* model = editableModel(input_model);
  Compartment* c = model->getCompartment(id);
  if (c == NULL) Rcpp::stop("No compartment '" + id + "' in the model.");

  bool unset;
  const double v = editValue(value, unset);
  if (unset) c->unsetSize(); else c->setSize(v);

  warnIfOverridden(model, "compartment", id);
  return input_model;
}

//'@rdname editModel
// [[Rcpp::export(invisible = true)]]
SEXP setSpeciesIC(SEXP input_model, std::string id, SEXP value, std::string as) {
  Model* model = editableModel(input_model);
  if (as != "amount" && as != "concentration")
    Rcpp::stop("`as` must be \"amount\" or \"concentration\".");
  Species* s = model->getSpecies(id);
  if (s == NULL) Rcpp::stop("No species '" + id + "' in the model.");

  bool unset;
  const double v = editValue(value, unset);
  if (as == "amount") {
    s->unsetInitialConcentration();
    if (unset) s->unsetInitialAmount(); else s->setInitialAmount(v);
  } else {
    s->unsetInitialAmount();
    if (unset) s->unsetInitialConcentration(); else s->setInitialConcentration(v);
  }

  warnIfOverridden(model, "species", id);
  return input_model;
}

// Names in `math` that kinetic-law math cannot resolve.  A plain name may be a
// compartment, species, global parameter, reaction or species reference
// (Level 3) of the model, or one of the law's own local parameters; a call
// must name a function definition.  csymbols (time, avogadro, delay, rateOf)
// and constants have their own node types, so they never reach here.
static void unresolvedNames(const ASTNode* math, const Model* model,
                            const KineticLaw* kl,
                            std::vector<std::string>& bad) {
  if (math == NULL) return;
  const ASTNodeType_t type = math->getType();
  if (type == AST_NAME && math->getName() != NULL) {
    const std::string name = math->getName();
    const bool ok =
      model->getCompartment(name) != NULL || model->getSpecies(name) != NULL ||
      model->getParameter(name) != NULL || model->getReaction(name) != NULL ||
      model->getSpeciesReference(name) != NULL ||
      model->getModifierSpeciesReference(name) != NULL ||
      (kl != NULL && (kl->getLocalParameter(name) != NULL ||
                      kl->getParameter(name) != NULL));
    if (!ok && std::find(bad.begin(), bad.end(), name) == bad.end())
      bad.push_back(name);
  } else if (type == AST_FUNCTION && math->getName() != NULL) {
    const std::string name = math->getName();
    if (model->getFunctionDefinition(name) == NULL &&
        std::find(bad.begin(), bad.end(), name + "()") == bad.end())
      bad.push_back(name + "()");
  }
  for (unsigned int i = 0; i < math->getNumChildren(); i++)
    unresolvedNames(math->getChild(i), model, kl, bad);
}

//'Replace a reaction's rate law
//'
//'Parse an infix formula and make it the kinetic law of a reaction, in place.
//'
//'`formula` uses libSBML's Level 3 infix syntax, the same syntax
//'`getReactionMath()` prints: `k1 * S`, `Vmax * S / (Km + S)`, `S^2`,
//'`exp(-k * time)`, and calls to the model's function definitions. Every name
//'in it must be a compartment, species, parameter, reaction or species
//'reference of the model, one of the reaction's local parameters, or a
//'function definition; anything else is an error and the model is left
//'unchanged.
//'
//'The reaction's local parameters are kept, whether or not the new formula
//'uses them. A reaction without a kinetic law gets one. As with the other
//'setters the model changes in place; see [editModel]. `writeSBML()` runs the
//'full consistency checks, so it reports units that do not add up as warnings.
//'@param input_model a model pointer from `getModel()`
//'@param id the id of the reaction
//'@param formula the new rate law, a single string
//'@return the model pointer, invisibly, so calls can be chained
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'# vcat's local parameter kcat is kept, so the new law can still use it
//'setRateLaw(model, "vcat", "comp * kcat * ES / (1 + P)")
//'getReactionMath(model)
// [[Rcpp::export(invisible = true)]]
SEXP setRateLaw(SEXP input_model, std::string id, SEXP formula) {
  Model* model = editableModel(input_model);
  Reaction* r = model->getReaction(id);
  if (r == NULL) Rcpp::stop("No reaction '" + id + "' in the model.");

  if (TYPEOF(formula) != STRSXP || Rf_length(formula) != 1 ||
      STRING_ELT(formula, 0) == NA_STRING)
    Rcpp::stop("`formula` must be a single string.");
  const std::string text = CHAR(STRING_ELT(formula, 0));

  std::unique_ptr<ASTNode> math(
    SBML_parseL3FormulaWithModel(text.c_str(), model));
  if (math.get() == NULL) {
    char* err = SBML_getLastParseL3Error();
    const std::string msg = err != NULL ? err : "unknown parse error";
    free(err);
    Rcpp::stop("Cannot parse the rate law of reaction '" + id + "': " + msg);
  }

  std::vector<std::string> bad;
  unresolvedNames(math.get(), model, r->getKineticLaw(), bad);
  if (!bad.empty()) {
    std::string names;
    for (size_t i = 0; i < bad.size(); i++)
      names += (i ? ", '" : "'") + bad[i] + "'";
    Rcpp::stop("The rate law of reaction '" + id + "' names " +
               (bad.size() == 1 ? "an undefined id: " : "undefined ids: ") +
               names + ". The model was not changed.");
  }

  KineticLaw* kl = r->isSetKineticLaw() ? r->getKineticLaw()
                                        : r->createKineticLaw();
  // setMath() deep-copies; `math` frees the parsed tree
  if (kl == NULL || kl->setMath(math.get()) != LIBSBML_OPERATION_SUCCESS)
    Rcpp::stop("Could not set the rate law of reaction '" + id + "'.");
  return input_model;
}

// Does `math` read any of `ids` as a plain name?  `shadow` holds ids a
// kinetic law's local parameters hide.
static bool readsAny(const ASTNode* math, const std::vector<std::string>& ids,
                     const KineticLaw* shadow = NULL) {
  if (math == NULL) return false;
  if (math->getType() == AST_NAME && math->getName() != NULL) {
    const std::string name = math->getName();
    if (std::find(ids.begin(), ids.end(), name) != ids.end() &&
        (shadow == NULL || (shadow->getLocalParameter(name) == NULL &&
                            shadow->getParameter(name) == NULL)))
      return true;
  }
  for (unsigned int i = 0; i < math->getNumChildren(); i++)
    if (readsAny(math->getChild(i), ids, shadow)) return true;
  return false;
}

static bool isOneOf(const std::string& x, const std::vector<std::string>& ids) {
  return std::find(ids.begin(), ids.end(), x) != ids.end();
}

static std::string labelOf(const SBase* x, const std::string& kind) {
  return x->isSetIdAttribute() ? kind + " '" + x->getIdAttribute() + "'" : kind;
}

static void addUse(std::vector<std::string>& uses, const std::string& use) {
  if (std::find(uses.begin(), uses.end(), use) == uses.end())
    uses.push_back(use);
}

// Everything left in the model, once reaction `skip` is gone, that reads or
// sets one of `ids`: the reaction id is its rate in Level 3 math, and a
// species reference id is its stoichiometry, which math reads and rules,
// initial assignments and event assignments can set.
static std::vector<std::string> usesOf(const Model* model,
                                       const std::vector<std::string>& ids,
                                       const Reaction* skip) {
  std::vector<std::string> uses;
  for (unsigned int i = 0; i < model->getNumRules(); i++) {
    const Rule* r = model->getRule(i);
    const std::string kind = r->isAssignment() ? "assignment rule"
                           : r->isRate() ? "rate rule" : "algebraic rule";
    if (readsAny(r->getMath(), ids) ||
        (r->isSetVariable() && isOneOf(r->getVariable(), ids)))
      addUse(uses, r->isSetVariable()
                     ? "the " + kind + " for '" + r->getVariable() + "'"
                     : "an " + kind);
  }
  for (unsigned int i = 0; i < model->getNumInitialAssignments(); i++) {
    const InitialAssignment* ia = model->getInitialAssignment(i);
    if (readsAny(ia->getMath(), ids) || isOneOf(ia->getSymbol(), ids))
      addUse(uses, "the initial assignment for '" + ia->getSymbol() + "'");
  }
  for (unsigned int i = 0; i < model->getNumReactions(); i++) {
    const Reaction* rn = model->getReaction(i);
    if (rn == skip) continue;
    const KineticLaw* kl = rn->getKineticLaw();
    bool used = kl != NULL && readsAny(kl->getMath(), ids, kl);
    // Level 2 stoichiometryMath
    const unsigned int nr = rn->getNumReactants();
    for (unsigned int j = 0; !used && j < nr + rn->getNumProducts(); j++) {
      const SpeciesReference* sr = j < nr
        ? rn->getReactant(j) : rn->getProduct(j - nr);
      used = sr->isSetStoichiometryMath() &&
             readsAny(sr->getStoichiometryMath()->getMath(), ids);
    }
    if (used) addUse(uses, labelOf(rn, "reaction"));
  }
  for (unsigned int i = 0; i < model->getNumEvents(); i++) {
    const Event* e = model->getEvent(i);
    bool used =
      (e->isSetTrigger() && readsAny(e->getTrigger()->getMath(), ids)) ||
      (e->isSetDelay() && readsAny(e->getDelay()->getMath(), ids)) ||
      (e->isSetPriority() && readsAny(e->getPriority()->getMath(), ids));
    for (unsigned int j = 0; !used && j < e->getNumEventAssignments(); j++) {
      const EventAssignment* ea = e->getEventAssignment(j);
      used = readsAny(ea->getMath(), ids) || isOneOf(ea->getVariable(), ids);
    }
    if (used) addUse(uses, labelOf(e, "event"));
  }
  for (unsigned int i = 0; i < model->getNumConstraints(); i++)
    if (readsAny(model->getConstraint(i)->getMath(), ids))
      addUse(uses, "a constraint");
  return uses;
}

//'Remove a reaction
//'
//'Delete a reaction from a model, in place, with its species references and
//'kinetic law (local parameters included).
//'
//'Removal is refused, and the model left unchanged, when something that
//'stays behind still uses the reaction: math that reads its id (in Level 3
//'that is the reaction's rate) or the id of one of its species references
//'(that is a stoichiometry), or a rule, initial assignment or event assignment
//'that sets such a stoichiometry. The error lists what uses it.
//'
//'The species stay. A species no other reaction or rule changes keeps its
//'initial value from then on. As with the other setters the model changes in
//'place; use [copyModel()] first to keep the original. To switch a reaction
//'off but keep it, set its rate law to zero with `setRateLaw(model, id, "0")`.
//'@param input_model a model pointer from `getModel()`
//'@param id the id of the reaction
//'@return the model pointer, invisibly, so calls can be chained
//'@examples
//'sbml_file <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'removeReaction(model, "degradation")
//'getReactionMath(model)
// [[Rcpp::export(invisible = true)]]
SEXP removeReaction(SEXP input_model, std::string id) {
  Model* model = editableModel(input_model);
  Reaction* r = model->getReaction(id);
  if (r == NULL) Rcpp::stop("No reaction '" + id + "' in the model.");

  std::vector<std::string> ids(1, id);
  for (unsigned int j = 0; j < r->getNumReactants(); j++)
    if (r->getReactant(j)->isSetId()) ids.push_back(r->getReactant(j)->getId());
  for (unsigned int j = 0; j < r->getNumProducts(); j++)
    if (r->getProduct(j)->isSetId()) ids.push_back(r->getProduct(j)->getId());
  for (unsigned int j = 0; j < r->getNumModifiers(); j++)
    if (r->getModifier(j)->isSetId()) ids.push_back(r->getModifier(j)->getId());

  const std::vector<std::string> uses = usesOf(model, ids, r);
  if (!uses.empty()) {
    std::string list;
    for (size_t i = 0; i < uses.size(); i++)
      list += (i == 0 ? "" : i + 1 == uses.size() ? " and " : ", ") + uses[i];
    Rcpp::stop("Cannot remove reaction '" + id + "': " + list +
               (uses.size() == 1 ? " uses" : " use") +
               " it. The model was not changed.");
  }

  // removeReaction() hands back the detached reaction for the caller to free
  std::unique_ptr<Reaction> removed(model->removeReaction(id));
  if (removed.get() == NULL)
    Rcpp::stop("Could not remove reaction '" + id + "'.");
  return input_model;
}
