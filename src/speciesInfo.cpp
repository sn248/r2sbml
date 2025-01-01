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
//'@param input_model input should be an SBML Model
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
//'@param input_model input should be an SBML Model
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
    // Rcpp::Rcout << speciesIC[i] << std::endl;
  }

  return speciesIC;
}

//'getspeciesTable
//'Outputs the Information Table for Species
//'@param input_model input should be an SBML Model
// [[Rcpp::export]]
Rcpp::DataFrame getSpeciesTable (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    Rcpp::stop("Stopping!");
  }

  int numSpecies = model->getNumSpecies();
  if (numSpecies == 0)
  {
    Rcpp::Rcout << "No species present." << std::endl;
    Rcpp::stop("Stopping!");
  }

  Rcpp::NumericVector speciesNum;       // Species Number
  Rcpp::StringVector speciesName;       // Species Name
  Rcpp::StringVector speciesID;         // Species ID
  Rcpp::NumericVector speciesIC;        // Species Initial Concentration
  Rcpp::NumericVector speciesIA;        // Species Initial Amount
  Rcpp::StringVector speciesCmt;        // Species Compartment
  Rcpp::NumericVector speciesCmtVol;    // Species Compartment Volume
  Rcpp::NumericVector speciesBC;        // Is Species Boundary Condition
  Rcpp::LogicalVector speciesConstant;  // Is Species a constant

  for(int i = 0; i < numSpecies; i++){

    Species* species = model->getSpecies(i);
    speciesNum.push_back(i+1);
    speciesID.push_back(species->getIdAttribute());
    speciesName.push_back(species->getName());
    speciesIC.push_back(species->getInitialConcentration());
    speciesIA.push_back(species->getInitialAmount());
    speciesCmt.push_back(species->getCompartment());           // returns the ID attribute of Compartment

    Compartment* cmt = model->getCompartment(species->getCompartment());     // get the compartment first
    speciesCmtVol.push_back(cmt->getVolume());

    speciesBC.push_back(species->getBoundaryCondition());
    speciesConstant.push_back(species->getConstant());

  }

  Rcpp::DataFrame speciesTable = Rcpp::DataFrame::create(_("Number") = speciesNum,
                                                         _["Name"] = speciesName,
                                                         _["ID"] = speciesID,
                                                         _["InitialConcentration"] = speciesIC,
                                                         _["InitialAmount"] = speciesIA,
                                                         _["Compartment"] = speciesCmt,
                                                         _["CompartmentVol"] = speciesCmtVol,
                                                         _["BoundaryCondition"] = speciesBC,
                                                         _["Constant"] = speciesConstant);

  return speciesTable;
}
