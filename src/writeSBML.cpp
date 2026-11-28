/* Writing a (possibly edited) model back to an SBML file. */

#include <Rcpp.h>
#include <string>
#include <sbml/SBMLTypes.h>

using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'Write a model to an SBML file
//'
//'Writes the model, with any edits made by `setParameterValue()`,
//'`setCompartmentSize()` or `setSpeciesIC()`, to an SBML file.
//'
//'The model is first run through libSBML's consistency checks. A model with
//'error- or fatal-severity diagnostics is not written, so an edit can never
//'leave an invalid file behind; lesser diagnostics are raised as a warning and
//'the file is written anyway, the rule `getModel()` applies when reading.
//'Because these checks are more thorough than the ones made on reading, a
//'model `getModel()` accepted can still be refused here.
//'
//'libSBML writes the whole document in its own layout, so a diff against the
//'original file shows changes of indentation and attribute order besides the
//'edits. The SBML level and version are kept.
//'@param input_model a model pointer from `getModel()`
//'@param filename output file name, conventionally ending in `.xml`
//'@return NULL invisibly
//'@examples
//'sbml_file <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'setParameterValue(model, "k1", 1000)
//'out_file <- tempfile(fileext = ".xml")
//'writeSBML(model, out_file)
//'getParameterTable(getModel(out_file))
// [[Rcpp::export]]
void writeSBML(SEXP input_model, std::string filename) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  if (model == NULL) Rcpp::stop("No model present.");
  if (filename == "") Rcpp::stop("Output file path is empty.");

  // The document belongs to the external pointer's finalizer: only serialise
  // it here, never delete it.
  SBMLDocument* document = model->getSBMLDocument();
  if (document == NULL) Rcpp::stop("The model has no SBML document.");

  // checkConsistency() appends to the document's error log, which still holds
  // whatever the reader reported, so clear it first or every write would
  // count those again.
  document->getErrorLog()->clearLog();
  document->checkConsistency();

  // The severity rule shared with getModel() and convertReactions():
  // getNumErrors(severity) counts that severity exactly, so FATAL has to be
  // added separately.
  const unsigned int fatal    = document->getNumErrors(LIBSBML_SEV_ERROR) +
                                document->getNumErrors(LIBSBML_SEV_FATAL);
  const unsigned int reported = document->getNumErrors();
  const unsigned int minor    = reported > fatal ? reported - fatal : 0;

  if (fatal > 0)
  {
    Rcpp::Rcerr << "Encountered the following SBML errors:" << std::endl;
    document->printErrors(Rcpp::Rcerr);
    document->getErrorLog()->clearLog();
    Rcpp::stop("Cannot write '" + filename + "': libSBML reported " +
               std::to_string(fatal) +
               " SBML error(s), listed above.  Nothing was written.");
  }

  if (minor > 0)
  {
    Rcpp::Rcerr << "libSBML reported the following non-fatal diagnostics:"
                << std::endl;
    document->printErrors(Rcpp::Rcerr);
  }
  document->getErrorLog()->clearLog();

  SBMLWriter writer;
  if (!writer.writeSBMLToFile(document, filename))
    Rcpp::stop("Could not write '" + filename + "'.");

  if (minor > 0)
    Rcpp::warning("'" + filename + "' was written with " +
                  std::to_string(minor) +
                  " SBML warning(s), listed above.");
}
