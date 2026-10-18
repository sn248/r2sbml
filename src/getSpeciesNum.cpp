#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'Number of species in a model
//'
//'Counts the species the model declares.
//'@param input_model input should be an SBML model
//'@return an integer, the number of species
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getNumSpecies(model)
// [[Rcpp::export]]
int getNumSpecies (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  return model->getNumSpecies();
}
