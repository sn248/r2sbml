#include <Rcpp.h>
#include <iostream>

#include <sbml/xml/XMLNode.h>
#include <sbml/xml/XMLAttributes.h>
#include <sbml/xml/XMLInputStream.h>
#include <sbml/xml/XMLOutputStream.h>

#include <sbml/annotation/RDFAnnotation.h>
#include <sbml/math/ASTNode.h>

#include <sbml/SBMLDocument.h>
#include <sbml/SBO.h>
#include <sbml/SBMLVisitor.h>
#include <sbml/SBMLError.h>
#include <sbml/KineticLaw.h>

#include <sbml/Model.h>
#include "./sbml/Model.cpp"

#include <sbml/ModifierSpeciesReference.h>
#include <sbml/SimpleSpeciesReference.h>

#include <sbml/AlgebraicRule.h>
#include <sbml/AssignmentRule.h>
#include <sbml/RateRule.h>

#include <sbml/util/IdentifierTransformer.h>
#include <sbml/util/ElementFilter.h>

#include <sbml/extension/SBMLExtensionRegistry.h>
#include <sbml/extension/SBasePlugin.h>


// [[Rcpp::export]]
SEXP getModel (SEXP filename) {

  std::string fname = Rcpp::as<std::string>(filename);
  libsbml::SBMLReader reader;
  libsbml::SBMLDocument* document = reader.readSBMLFromFile(fname);

  libsbml::Model* model = document->getModel();

  Rcpp::XPtr<libsbml::Model> modelPtr(model);

  return Rcpp::wrap(modelPtr);

}

RCPP_MODULE(sbModel){

  using namespace Rcpp ;

  // we expose the class libsbml::Model as "Model" on the R side
  class_<libsbml::Model>("Model")

  // exposing the default constructor
  .constructor<unsigned int, unsigned int>("Creates a new Model using the given SBML level and version values.")

  // exposing member functions -- taken directly from libsbml::Model
  .method( "getListofParameters", &libsbml::Model::getListOfParameters(),"Get the list of Parameters of the model")

}
