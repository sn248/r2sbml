#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;
LIBSBML_CPP_NAMESPACE_USE

// [[Rcpp::export]]
int printSBML (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  SBMLReader reader;
  SBMLDocument* document = reader.readSBMLFromFile(fname);

  unsigned int errors = document->getNumErrors();
  std::cout << std::endl;
  std::cout << "  filename: " << fname << std::endl;
  std::cout << "  error(s): " << errors  << std::endl;
  std::cout << std::endl;

  if (document->getNumErrors() > 0)
  {
    cerr << "Encountered the following SBML errors:" << endl;
    document->printErrors(cerr);
    return 1;
  }

  unsigned int level   = document->getLevel  ();
  unsigned int version = document->getVersion();
  cout << endl
       << "File: " << fname
       << " (Level " << level << ", version " << version << ")" << endl;

  Model* model = document->getModel();
  if (model == 0)
  {
    cout << "No model present." << endl;
    return 1;
  }

  cout << "               "
       << (level == 1 ? "name: " : "  id: ")
       << (model->isSetId() ? model->getId() : std::string("(empty)")) << endl;

  if (model->isSetSBOTerm())
    cout << "      model sboTerm: " << model->getSBOTerm() << endl;
  cout << "functionDefinitions: " << model->getNumFunctionDefinitions() << endl;
  cout << "    unitDefinitions: " << model->getNumUnitDefinitions    () << endl;
  cout << "   compartmentTypes: " << model->getNumCompartmentTypes   () << endl;
  cout << "        specieTypes: " << model->getNumSpeciesTypes       () << endl;
  cout << "       compartments: " << model->getNumCompartments       () << endl;
  cout << "            species: " << model->getNumSpecies            () << endl;
  cout << "         parameters: " << model->getNumParameters         () << endl;
  cout << " initialAssignments: " << model->getNumInitialAssignments () << endl;
  cout << "              rules: " << model->getNumRules              () << endl;
  cout << "        constraints: " << model->getNumConstraints        () << endl;
  cout << "          reactions: " << model->getNumReactions          () << endl;
  cout << "             events: " << model->getNumEvents             () << endl;
  cout << endl;

  delete document;

  return 0;
}
