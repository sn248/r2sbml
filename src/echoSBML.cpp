#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace Rcpp;
LIBSBML_CPP_NAMESPACE_USE

//'echoSBML
//'@param filename input file name
// [[Rcpp::export]]
int echoSBML (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  SBMLReader reader;
  SBMLDocument* document = reader.readSBMLFromFile(fname);

  unsigned int errors = document->getNumErrors();
  Rcpp::Rcout << std::endl;
  Rcpp::Rcout << "  filename: " << fname << std::endl;
  Rcpp::Rcout << "  error(s): " << errors  << std::endl;
  Rcpp::Rcout << std::endl;

  if (errors > 0) document->printErrors(Rcpp::Rcerr);
  return errors;

}


