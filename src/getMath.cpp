// Get the Rule, Reaction, and Event formulas in a given SBML Document
// Derived form PrintMath.cpp in SBML examples

#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getRuleMath
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
// [[Rcpp::export]]
int getRuleMath (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  // Printing Rule Math
  int numRules = model->getNumRules();
  if (numRules == 0)
  {
    Rcpp::Rcout << "No Rules present in the model." << std::endl;
    stop("Stopping!");
  }

  for(int n = 0; n < numRules; n++)
  {
    char* formula;
    //Rcpp::StringVector formula;
    const Rule* r = model->getRule(n);
    if ( r->isSetMath() )
    {
      formula = SBML_formulaToString( r->getMath() );

      if (r->getVariable().length() > 0)
      {
        Rcout << "Rule " << n << ", formula: "
             << r->getVariable() << " = " << formula << endl;
      }
      else
      {
        Rcout << "Rule " << n << ", formula: "
             << formula << " = 0" << endl;
      }
      free(formula);
    }
  }

  return 0;

}


//'getReactionMath
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
// [[Rcpp::export]]
int getReactionMath (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numReactions = model->getNumReactions();
  if (numReactions == 0)
  {
    Rcpp::Rcout << "No Reactions present in the model." << std::endl;
    stop("Stopping!");
  }

  // Print Reaction Math
  for(int n = 0; n < numReactions; n++){

    char* formula;
    const KineticLaw* kl;
    Reaction* r = model->getReaction(n);

    if (r->isSetKineticLaw())
    {
      kl = r->getKineticLaw();

      if ( kl->isSetMath() )
      {
        formula = SBML_formulaToString( kl->getMath() );
        Rcout << "Reaction " << n << ", formula: " << formula << endl;
        free(formula);
      }
    }
  }
 return 0;
}


//'getFunctionDefinition
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
// [[Rcpp::export]]
int getFunctionDefinition (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  const ASTNode* math;
  char* formula;
  int numFunctionDefinitions = model->getNumFunctionDefinitions();
  if (numFunctionDefinitions == 0)
  {
    Rcpp::Rcout << "No Function definitions present in the model." << std::endl;
    stop("Stopping!");
  }

  for(int n = 0; n < numFunctionDefinitions; n++)
  {
    const FunctionDefinition* fd;
    if ( fd->isSetMath() )
    {
      Rcout << "FunctionDefinition " << n << ", " << fd->getId();

      math = fd->getMath();

      /* Print function arguments. */
      if (math->getNumChildren() > 1)
      {
        Rcout << "(" << ( math->getLeftChild() )->getName();

        for (n = 1; n < math->getNumChildren() - 1; ++n)
        {
          Rcout <<", " << ( math->getChild(n) )->getName();
        }
      }

      Rcout <<") := ";

      /* Print function body. */
      if (math->getNumChildren() == 0)
      {
        Rcout << "(no body defined)";
      }
      else
      {
        math    = math->getChild(math->getNumChildren() - 1);
        formula = SBML_formulaToString(math);
        Rcout << formula << endl;
        free(formula);
      }
    }
  }
  return 0;
}

//'printEventAssignmentMath
//'Outputs Event Assignment Math
void printEventAssignmentMath (unsigned int n, const EventAssignment* ea)
{
  std::string variable;
  char* formula;


  if ( ea->isSetMath() )
  {
    variable = ea->getVariable();
    formula  = SBML_formulaToString( ea->getMath() );

    cout <<"  EventAssignment " << n
         << ", trigger: " << variable << " = " << formula << endl;

    free(formula);
  }
}

//'getEventMath
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
// [[Rcpp::export]]
int getEventMath (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  if (model == 0)
  {
    Rcpp::Rcout << "No model present." << std::endl;
    stop("Stopping!");
  }

  int numEvents = model->getNumEvents();

  if (numEvents == 0)
  {
    Rcpp::Rcout << "No Events present in the model." << std::endl;
    stop("Stopping!");
  }

  for(int n = 0; n < numEvents; n++)
  {
    char* formula;
    unsigned int i;
    Event* e = model->getEvent(n);

    if ( e->isSetDelay() )
    {
      formula = SBML_formulaToString( e->getDelay()->getMath() );
      cout << "Event " << n << " delay: " << formula << endl;
      free(formula);
    }

    if ( e->isSetTrigger() )
    {
      formula = SBML_formulaToString( e->getTrigger()->getMath() );
      cout << "Event " << n << " trigger: " << formula << endl;
      free(formula);
    }

    for (i = 0; i < e->getNumEventAssignments(); ++i)
    {
      printEventAssignmentMath(i + 1, e->getEventAssignment(i));
    }
  }
return 0;
}
