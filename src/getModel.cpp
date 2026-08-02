#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getModel
//'Outputs an external pointer to the Model from SBML file
//'@param filename input file name, should end with .xml
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
// [[Rcpp::export]]
SEXP getModel (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  SBMLReader reader;
  SBMLDocument* document = reader.readSBMLFromFile(fname);

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

  Model* model = document->getModel();

  // convert to XPtr which is an external pointer in R
  Rcpp::XPtr<Model> model_extptr(model, true);

  return model_extptr;

}
