// #include <RcppCommon.h>

#include <iostream>
#include <sbml/SBMLDocument.h>
#include <sbml/Model.h>

// namespace Rcpp {
//
// namespace traits {
//
//    template <typename T> SEXP wrap( const libsbml::Model& ) {
//
//    }
//
//   }
// }


#include <Rcpp.h>

// [[Rcpp::export]]
SEXP getModel (std::string fname) {

  libsbml::SBMLReader reader;
  libsbml::SBMLDocument* document = reader.readSBMLFromFile(fname);

  libsbml::Model* model = document->getModel();

  // Rcpp::XPtr<libsbml::Model> modelPtr(model);
  // return Rcpp::wrap(modePtr);

  return Rcpp::wrap(*model);

}

RCPP_MODULE(sbModel){

  using namespace Rcpp ;

  // we expose the class libsbml::Model as "Model" on the R side
  class_<libsbml::Model>("Model")

  // exposing the default constructor
  .constructor<unsigned int, unsigned int>("Creates a new Model using the given SBML level and version values.")

  // exposing member functions -- taken directly from libsbml::Model
  .method( "getListOfParameters", &libsbml::Model::getListOfParameters(),"Get the list of Parameters of the model")




}
