/* Editing a loaded model in place: parameter values, compartment sizes,
 * species initial values and rate laws.  Each setter returns the model
 * pointer invisibly, so calls chain, and mutates the document the pointer
 * owns -- every R variable holding the same pointer sees the change.
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
