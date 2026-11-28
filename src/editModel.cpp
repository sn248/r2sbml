/* Editing a loaded model in place: parameter values, compartment sizes and
 * species initial values.  Each setter returns the model pointer invisibly,
 * so calls chain, and mutates the document the pointer owns -- every R
 * variable holding the same pointer sees the change.
 */

#include <Rcpp.h>
#include <string>
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
