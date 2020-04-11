#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getSpeciesNum
//'Outputs the number of Species in the model
//'@param filename input file name, should end with .xml
// [[Rcpp::export]]
int getNumSpecies (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  return model->getNumSpecies();
}
