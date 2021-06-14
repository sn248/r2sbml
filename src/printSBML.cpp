#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;
LIBSBML_CPP_NAMESPACE_USE

//'printSBML
//'@param filename filename should be the name of the xml file
// [[Rcpp::export]]
int printSBML (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  SBMLReader reader;
  SBMLDocument* document = reader.readSBMLFromFile(fname);

  unsigned int errors = document->getNumErrors();
  Rcout << std::endl;
  Rcout << "  filename: " << fname << std::endl;
  Rcout << "  error(s): " << errors  << std::endl;
  Rcout << std::endl;

  if (document->getNumErrors() > 0)
  {
    Rcerr << "Encountered the following SBML errors:" << endl;
    document->printErrors(Rcerr);
    return 1;
  }

  unsigned int level   = document->getLevel  ();
  unsigned int version = document->getVersion();
  Rcout << endl
        << "File: " << fname
        << " (Level " << level << ", version " << version << ")" << endl;

  Model* model = document->getModel();
  if (model == 0)
  {
    Rcout << "No model present." << endl;
    return 1;
  }

  Rcout << "               "
        << (level == 1 ? "name: " : "  id: ")
        << (model->isSetId() ? model->getId() : std::string("(empty)")) << endl;

  if (model->isSetSBOTerm())
  {
    Rcout << "      model sboTerm: " << model->getSBOTerm() << endl;
  }

  Rcout << "functionDefinitions: " << model->getNumFunctionDefinitions() << endl;
  Rcout << "    unitDefinitions: " << model->getNumUnitDefinitions    () << endl;
  Rcout << "   compartmentTypes: " << model->getNumCompartmentTypes   () << endl;
  Rcout << "        specieTypes: " << model->getNumSpeciesTypes       () << endl;
  Rcout << "       compartments: " << model->getNumCompartments       () << endl;
  Rcout << "            species: " << model->getNumSpecies            () << endl;
  Rcout << "         parameters: " << model->getNumParameters         () << endl;
  Rcout << " initialAssignments: " << model->getNumInitialAssignments () << endl;
  Rcout << "              rules: " << model->getNumRules              () << endl;
  Rcout << "        constraints: " << model->getNumConstraints        () << endl;
  Rcout << "          reactions: " << model->getNumReactions          () << endl;
  Rcout << "             events: " << model->getNumEvents             () << endl;
  Rcout << endl;

  delete document;

  return 0;
}
