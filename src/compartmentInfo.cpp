#include <Rcpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getCmtNames
//'Names of the compartments in a model
//'
//'Returns the `name` where one is set and falls back to the `id` otherwise,
//'the same rule `getSpeciesNames()` uses. Previously this returned the `id`
//'unconditionally while `getSpeciesNames()` returned the `name`, so the two
//'disagreed about what a "name" was. For a model whose compartments set no
//'`name` -- which is all ten of the bundled examples -- the result is unchanged.
//'@param input_model input should be an SBML Model
//'@return a character vector, one entry per compartment, in model order
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getCmtNames(model)
// [[Rcpp::export]]
Rcpp::StringVector getCmtNames (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  Rcpp::StringVector cmtNames;

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numCmt = model->getNumCompartments();
  for(int i = 0; i < numCmt; i++){

    const Compartment* c = model->getCompartment(i);
    cmtNames.push_back(c->isSetName() ? c->getName() : c->getIdAttribute());
  }

  return cmtNames;
}


//'getCmtSizes
//'Outputs the Sizes of Compartments
//'@param input_model input should be an SBML Model
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getCmtSizes(model)
// [[Rcpp::export]]
Rcpp::NumericVector getCmtSizes (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  // std::vector<string> cmtNames;
  Rcpp::NumericVector cmtSizes;

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numCmt = model->getNumCompartments();
  for(int i = 0; i < numCmt; i++){
    // cmtNames.push_back(model->getCompartment(i)->getId());
    cmtSizes.push_back(model->getCompartment(i)->getSize());
    Rcpp::Rcout << cmtSizes[i] << std::endl;
  }

  return cmtSizes;
}



