#include <Rcpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getCmtNames
//'Outputs the Names of Compartments
//'@param SBMLmodel input should be an SBML Model
// [[Rcpp::export]]
Rcpp::StringVector getCmtNames (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  // std::vector<string> cmtNames;
  Rcpp::StringVector cmtNames;

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numCmt = model->getNumCompartments();
  for(int i = 0; i < numCmt; i++){

    cmtNames.push_back(model->getCompartment(i)->getId());
    Rcpp::Rcout << cmtNames[i] << std::endl;
  }

  return cmtNames;
}


//'getCmtSizes
//'Outputs the Sizes of Compartments
//'@param SBMLmodel input should be an SBML Model
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



