/* Function to get the Parameter Information i.e., Parameter Number,
 * Parameter Name, Parameter ID, Parameter Value, Local of Global Status
 * Parameter Units, Used in Reaction Number or ID
 */

#include <Rcpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getParameterTable
//'Outputs the Information Table for Paremeters
//'@param SBMLmodel input should be an SBML Model
// [[Rcpp::export]]
Rcpp::DataFrame getParameterTable (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::stop("Stopping, no model present!");
  }

  int numParams = model->getNumParameters();
  if (numParams == 0){
    Rcpp::stop("Stopping, no parameters present in the model!");
  }

  NumericVector paramNum;
  StringVector paramName;
  StringVector paramID;
  NumericVector paramValue;
  StringVector paramUnits;

  for(int i = 0; i<numParams; i++){

    Parameter *param = model->getParameter(i);
    paramNum.push_back(i+1);
    paramName.push_back(param->getName());
    paramID.push_back(param->getIdAttribute());
    paramValue.push_back(param->getValue());
    paramUnits.push_back(param->getUnits());

  }

  DataFrame paramTable = DataFrame::create(_("Number") = paramNum,
                                             _("ID") = paramID,
                                             _("Name") = paramName,
                                             _("Value") = paramValue,
                                             _("Units") = paramUnits);

  return paramTable;
}


