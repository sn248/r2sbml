#include <Rcpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getSpeciesNames
//'Names of the species in a model
//'
//'SBML separates `id`, which every species must have, from `name`, which is an
//'optional human-readable label. Most models in the wild set only `id`, so
//'returning `name` alone gave a vector of empty strings. This returns the
//'`name` where one is set and falls back to the `id` otherwise, which is how
//'SBML tools conventionally display a species. `getCmtNames()` follows the
//'same rule. Use `getSpeciesTable()` when you need the two columns separately.
//'@param input_model input should be an SBML Model
//'@return a character vector, one entry per species, in model order
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getSpeciesNames(model)
// [[Rcpp::export]]
Rcpp::StringVector getSpeciesNames (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  Rcpp::StringVector speciesNames;

  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  int numSpecies = model->getNumSpecies();
  if (numSpecies == 0)
  {
    Rcpp::stop("No species present.");
  }

  for(int i = 0; i < numSpecies; i++){

    const Species* s = model->getSpecies(i);
    speciesNames.push_back(s->isSetName() ? s->getName() : s->getIdAttribute());
  }

  return speciesNames;
}


//'getSpeciesIC
//'Initial values of the species in a model
//'
//'A species carries `initialAmount` *or* `initialConcentration`, never both,
//'and the unset one reads back as NaN. Asking only for the concentration
//'therefore returned NaN for every species in an amount-based model. This
//'returns whichever attribute the model actually sets.
//'
//'The value is reported **as the file states it**, with no unit conversion, so
//'a model that mixes amount-valued and concentration-valued species yields a
//'vector that mixes units too -- `getSpeciesTable()` shows which column each
//'species used. This deliberately differs from `convertReactions()`, which
//'must divide an amount by the compartment volume because the ODE it generates
//'integrates concentrations.
//'
//'A species that sets neither attribute -- its value comes from an initial
//'assignment or a rule -- is reported as `NA`, not 0.
//'@param input_model input should be an SBML Model
//'@return a named numeric vector, one entry per species, named by species id
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getSpeciesIC(model)
// [[Rcpp::export]]
Rcpp::NumericVector getSpeciesIC (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  Rcpp::NumericVector speciesIC;
  Rcpp::StringVector  speciesIds;

  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  int numSpecies = model->getNumSpecies();
  if (numSpecies == 0)
  {
    Rcpp::stop("No species present.");
  }

  for(int i = 0; i < numSpecies; i++){

    const Species* s = model->getSpecies(i);

    if (s->isSetInitialAmount())             speciesIC.push_back(s->getInitialAmount());
    else if (s->isSetInitialConcentration()) speciesIC.push_back(s->getInitialConcentration());
    else                                     speciesIC.push_back(NA_REAL);

    speciesIds.push_back(s->getIdAttribute());
  }

  speciesIC.attr("names") = speciesIds;
  return speciesIC;
}

//'getspeciesTable
//'Outputs the Information Table for Species
//'@param input_model input should be an SBML Model
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getSpeciesTable(model)
// [[Rcpp::export]]
Rcpp::DataFrame getSpeciesTable (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  int numSpecies = model->getNumSpecies();
  if (numSpecies == 0)
  {
    Rcpp::stop("No species present.");
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
