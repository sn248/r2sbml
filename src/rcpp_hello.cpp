#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace Rcpp;

// This is a simple function using Rcpp that creates an R list
// containing a character vector and a numeric vector.
//
// Learn more about how to use Rcpp at:
//
//   http://www.rcpp.org/
//   http://adv-r.had.co.nz/Rcpp.html
//
// and browse examples of code using Rcpp at:
//
//   http://gallery.rcpp.org/
//

// [[Rcpp::export]]
Rcpp::List rcpp_hello() {
  CharacterVector x = CharacterVector::create("foo", "bar");
  NumericVector y   = NumericVector::create(0.0, 1.0);
  Rcpp::List z      = Rcpp::List::create(x, y);
  return z;
}

// [[Rcpp::export]]
int echoSBML (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  libsbml::SBMLReader reader;
  libsbml::SBMLDocument* document = reader.readSBMLFromFile(fname);

  unsigned int errors = document->getNumErrors();
  std::cout << std::endl;
  std::cout << "  filename: " << fname << std::endl;
  std::cout << "  error(s): " << errors  << std::endl;
  std::cout << std::endl;

  if (errors > 0) document->printErrors(std::cerr);
  return errors;

}


