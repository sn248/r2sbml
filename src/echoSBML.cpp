#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace Rcpp;
LIBSBML_CPP_NAMESPACE_USE

//'Check an SBML file for errors
//'
//'Reads the file and prints the number of diagnostics libSBML reports, then
//'the diagnostics themselves.
//'@param filename input file name
//'@return the number of diagnostics, as an integer
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'echoSBML(sbml_file)
// [[Rcpp::export]]
int echoSBML (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  SBMLReader reader;
  // SBMLDocument* document = reader.readSBMLFromFile(fname);
  SBMLDocument* document = reader.readSBML(fname);

  unsigned int errors = document->getNumErrors();
  Rcpp::Rcout << std::endl;
  Rcpp::Rcout << "  filename: " << fname << std::endl;
  Rcpp::Rcout << "  error(s): " << errors  << std::endl;
  Rcpp::Rcout << std::endl;

  if (errors > 0) document->printErrors(Rcpp::Rcerr);
  return errors;

}


