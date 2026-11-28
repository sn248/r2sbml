#include <Rcpp.h>
#include <iostream>
#include <memory>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

// The Model belongs to its SBMLDocument, so the pointer handed to R must free
// the document -- which frees the Model with it -- and never the Model alone.
// Deleting the Model directly, as the default XPtr finalizer does, left the
// document leaked and holding a dangling pointer to its own model.
static void finalizeModel(Model* model) {
  if (model) delete model->getSBMLDocument();
}

//'Read an SBML model
//'
//'Reads an SBML file and keeps the parsed document alive for the other
//'query functions. A file with error- or fatal-severity diagnostics, or with
//'no model in it, is an error; lesser diagnostics are raised as warnings.
//'@param filename input file name, should end with .xml
//'@return an external pointer to the model, to pass to the other functions
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
// [[Rcpp::export]]
SEXP getModel (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  SBMLReader reader;
  // Owned here until R takes it over, so every Rcpp::stop() below frees it.
  std::unique_ptr<SBMLDocument> document(reader.readSBMLFromFile(fname));

  // Only error- and fatal-severity diagnostics are worth refusing the file
  // over; libSBML also reports warnings and informational notes, and counting
  // those made getModel() stricter than convertReactions(), which has always
  // filtered by severity.  Note that getNumErrors(severity) counts that
  // severity *exactly*, so FATAL has to be asked for separately.
  const unsigned int fatal    = document->getNumErrors(LIBSBML_SEV_ERROR) +
                                document->getNumErrors(LIBSBML_SEV_FATAL);
  const unsigned int reported = document->getNumErrors();
  const unsigned int minor    = reported > fatal ? reported - fatal : 0;

  Rcpp::Rcout << std::endl;
  Rcpp::Rcout << "  filename: " << fname << std::endl;
  Rcpp::Rcout << "  error(s): " << fatal  << std::endl;
  if (minor > 0) Rcpp::Rcout << "  warning(s): " << minor << std::endl;
  Rcpp::Rcout << std::endl;

  if (fatal > 0)
  {
    Rcpp::Rcerr << "Encountered the following SBML errors:" << std::endl;
    document->printErrors(Rcpp::Rcerr);
    Rcpp::stop("Cannot read '" + fname + "': libSBML reported " +
               std::to_string(fatal) +
               " SBML error(s), listed above.  The model was not loaded.");
  }

  if (minor > 0)
  {
    Rcpp::Rcerr << "libSBML reported the following non-fatal diagnostics:"
                << std::endl;
    document->printErrors(Rcpp::Rcerr);
    Rcpp::warning("'" + fname + "' was read with " + std::to_string(minor) +
                  " SBML warning(s), listed above.  The model was loaded "
                  "anyway; convertReactions() would accept it too.");
  }

  unsigned int level   = document->getLevel  ();
  unsigned int version = document->getVersion();
  Rcpp::Rcout << std::endl
              << "File: " << fname
              << " (Level " << level << ", version " << version << ")" << endl;

  // SBML Level 3 makes <model> optional, so a valid document can hold none.
  // Refuse it here: a NULL pointer would only fail later, in whichever query
  // function came first, with Rcpp's bare "external pointer is not valid".
  Model* model = document->getModel();
  if (model == NULL)
    Rcpp::stop("Cannot read '" + fname + "': the file contains no model.");

  // From here the external pointer owns the document, through the Model.
  document.release();
  return Rcpp::XPtr<Model, Rcpp::PreserveStorage, finalizeModel, true>(model, true);

}

//'Copy a model
//'
//'Edits made with `setParameterValue()`, `setCompartmentSize()` and
//'`setSpeciesIC()` change a model in place, and every R variable that holds
//'the same pointer sees them: `model2 <- model` copies the pointer, not the
//'model. `copyModel()` makes an independent copy of the whole SBML document,
//'so the two can be edited separately.
//'@param input_model a model pointer from `getModel()`
//'@return an external pointer to the copy
//'@examples
//'sbml_file <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'edited <- copyModel(model)
//'setParameterValue(edited, "k1", 1000)
//'getParameterTable(model)$Value[1]
//'getParameterTable(edited)$Value[1]
// [[Rcpp::export]]
SEXP copyModel (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  if (model == NULL || model->getSBMLDocument() == NULL)
    Rcpp::stop("No model present.");

  std::unique_ptr<SBMLDocument> document(model->getSBMLDocument()->clone());
  Model* copy = document->getModel();
  if (copy == NULL) Rcpp::stop("The copied document contains no model.");

  document.release();
  return Rcpp::XPtr<Model, Rcpp::PreserveStorage, finalizeModel, true>(copy, true);

}
