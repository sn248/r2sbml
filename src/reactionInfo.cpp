/* Function to get the Reaction Information i.e., Reaction Number,
 * Reaction Name, Reaction ID, Reaction Reversible status, Reaction Rate Law
 * Reaction Rate law is a string, but can be used to make the model equations
 */

#include <Rcpp.h>
#include <iostream>
#include <vector>
#include <string>
#include <sbml/SBMLTypes.h>

#include "formulaToInfix.h"

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE


//'getReactionTable
//'Outputs the Information Table for Reactions
//'@param input_model input should be an SBML Model
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getReactionTable(model)
// [[Rcpp::export]]
Rcpp::DataFrame getReactionTable (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  int numReactions = model->getNumReactions();
  if (numReactions == 0){
    Rcpp::stop("No reactions present in the model.");
  }

  const KineticLaw* kl;
  Rcpp::NumericVector reactionNum;
  Rcpp::StringVector reactionID;
  Rcpp::StringVector reactionName;
  Rcpp::LogicalVector isReversible;
  Rcpp::StringVector rateLaw;

  for(int i = 0; i<numReactions; i++){

    Reaction* reaction = model->getReaction(i);

    reactionNum.push_back(i+1);
    reactionID.push_back(reaction->getIdAttribute());
    reactionName.push_back(reaction->getName());
    isReversible.push_back(reaction->getReversible());

    if (reaction->isSetKineticLaw())
    {
      kl = reaction->getKineticLaw();
      if ( kl->isSetMath() )
      {
        rateLaw.push_back(r2sbml::formulaToInfix( kl->getMath() ));
      }
    }

  }

  Rcpp::DataFrame reactionTable = Rcpp::DataFrame::create(_("Number") = reactionNum,
                                                          _("ID") = reactionID,
                                                          _("Name") = reactionName,
                                                          _("Reversible") = isReversible,
                                                          _("RateLaw") = rateLaw);

  return reactionTable;


}
