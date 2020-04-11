#include <Rcpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getSpeciesNames
//'Outputs the Names of Compartments
//'@param SBMLmodel input should be an SBML Model
// [[Rcpp::export]]
Rcpp::StringVector getSpeciesNames (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  // std::vector<string> cmtNames;
  Rcpp::StringVector speciesNames;

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numSpecies = model->getNumSpecies();
  if (numSpecies == 0)
  {
    Rcpp::Rcout << "No species present." << std::endl;
    stop("Stopping!");
  }

  for(int i = 0; i < numSpecies; i++){

    speciesNames.push_back(model->getSpecies(i)->getName());
    Rcpp::Rcout << speciesNames[i] << std::endl;
  }

  return speciesNames;
}


//'getspeciesIC
//'Outputs the Initial Concentrations of Species
//'@param SBMLmodel input should be an SBML Model
// [[Rcpp::export]]
Rcpp::NumericVector getSpeciesIC (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  Rcpp::NumericVector speciesIC;

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numSpecies = model->getNumSpecies();
  if (numSpecies == 0)
  {
    Rcpp::Rcout << "No species present." << std::endl;
    stop("Stopping!");
  }

  for(int i = 0; i < numSpecies; i++){

    speciesIC.push_back(model->getSpecies(i)->getInitialConcentration());
    Rcpp::Rcout << speciesIC[i] << std::endl;
  }

  return speciesIC;
}

//'getspeciesTable
//'Outputs the Information Table for Species
//'@param SBMLmodel input should be an SBML Model
// [[Rcpp::export]]
Rcpp::DataFrame getSpeciesTable (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numSpecies = model->getNumSpecies();
  if (numSpecies == 0)
  {
    Rcpp::Rcout << "No species present." << std::endl;
    stop("Stopping!");
  }

  Rcpp::NumericVector speciesNum;
  Rcpp::StringVector speciesName;
  Rcpp::StringVector speciesID;
  Rcpp::NumericVector speciesIC;
  Rcpp::StringVector speciesCmt;
  Rcpp::NumericVector speciesBC;

  for(int i = 0; i < numSpecies; i++){

    Species* species = model->getSpecies(i);
    speciesNum.push_back(i+1);
    speciesID.push_back(species->getId());
    speciesName.push_back(species->getName());
    speciesIC.push_back(species->getInitialConcentration());
    speciesCmt.push_back(species->getCompartment());
    speciesBC.push_back(species->getBoundaryCondition());

  }

  Rcpp::DataFrame speciesTable = Rcpp::DataFrame::create(_("Number") = speciesNum,
                                                         _["Name"] = speciesName,
                                                         _["ID"] = speciesID,
                                                         _["InitialConcentration"] = speciesIC,
                                                         _["Compartment"] = speciesCmt,
                                                         _["BoundaryCondition"] = speciesBC);

  return speciesTable;
}
