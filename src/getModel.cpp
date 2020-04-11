#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getModel
//'Outputs an external pointer to the Model from SBML file
//'@param filename input file name, should end with .xml
// [[Rcpp::export]]
SEXP getModel (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  SBMLReader reader;
  SBMLDocument* document = reader.readSBMLFromFile(fname);

  unsigned int errors = document->getNumErrors();
  Rcpp::Rcout << std::endl;
  Rcpp::Rcout << "  filename: " << fname << std::endl;
  Rcpp::Rcout << "  error(s): " << errors  << std::endl;
  Rcpp::Rcout << std::endl;

  if (document->getNumErrors() > 0)
  {
    Rcpp::Rcerr << "Encountered the following SBML errors:" << std::endl;
    document->printErrors(Rcpp::Rcerr);
    // return 1
    stop("Stopping!");
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
