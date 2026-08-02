/**
 * @file    getMath.cpp
 * @brief   Loads an SBML File and converts reactions to ODEs
 * @author  Sarah Keating
 * @author  Frank T. Bergmann
 *
 * <!--------------------------------------------------------------------------
 * This sample program is distributed under a different license than the rest
 * of libSBML.  This program uses the open-source MIT license, as follows:
 *
 * Copyright (c) 2013-2016 by the California Institute of Technology
 * (California, USA), the European Bioinformatics Institute (EMBL-EBI, UK)
 * and the University of Heidelberg (Germany), with support from the National
 * Institutes of Health (USA) under grant R01GM070923.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Neither the name of the California Institute of Technology (Caltech), nor
 * of the European Bioinformatics Institute (EMBL-EBI), nor of the University
 * of Heidelberg, nor the names of any contributors, may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * ------------------------------------------------------------------------ -->
 */

// Get the Rule, Reaction, and Event formulas in a given SBML Document
// Derived form PrintMath.cpp in SBML examples

#include <Rcpp.h>
#include <iostream>
#include <sbml/SBMLTypes.h>

#include "formulaToInfix.h"

using namespace std;
using namespace Rcpp;

LIBSBML_CPP_NAMESPACE_USE

//'getRuleMath
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
//'@examples
//'sbml_file <- system.file("examples", "sbmlassignmentrules.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getRuleMath(model)
// [[Rcpp::export]]
int getRuleMath (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  // Printing Rule Math
  int numRules = model->getNumRules();
  if (numRules == 0)
  {
    Rcpp::stop("No Rules present in the model.");
  }

  for(int n = 0; n < numRules; n++)
  {
    //Rcpp::StringVector formula;
    const Rule* r = model->getRule(n);
    if ( r->isSetMath() )
    {
      std::string formula = r2sbml::formulaToInfix( r->getMath() );

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
    }
  }

  return 0;

}


//'getReactionMath
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getReactionMath(model)
// [[Rcpp::export]]
int getReactionMath (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  int numReactions = model->getNumReactions();
  if (numReactions == 0)
  {
    Rcpp::stop("No Reactions present in the model.");
  }

  // Print Reaction Math
  for(int n = 0; n < numReactions; n++){

    const KineticLaw* kl;
    Reaction* r = model->getReaction(n);

    if (r->isSetKineticLaw())
    {
      kl = r->getKineticLaw();

      if ( kl->isSetMath() )
      {
        Rcout << "Reaction " << n << ", formula: "
              << r2sbml::formulaToInfix( kl->getMath() ) << endl;
      }
    }
  }
 return 0;
}


//'getFunctionDefinition
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
//'@examples
//'sbml_file <- system.file("examples", "sbmlfunctiondefinition.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getFunctionDefinition(model)
// [[Rcpp::export]]
int getFunctionDefinition (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);

  const ASTNode* math;
  int numFunctionDefinitions = model->getNumFunctionDefinitions();
  if (numFunctionDefinitions == 0)
  {
    Rcpp::stop("No Function definitions present in the model.");
  }

  for(int n = 0; n < numFunctionDefinitions; n++)
  {
    const FunctionDefinition* fd = model->getFunctionDefinition(n);
    if ( fd->isSetMath() )
    {
      Rcout << "FunctionDefinition " << n << ", " << fd->getId();

      math = fd->getMath();
      int numChildren = 0;
      numChildren = math->getNumChildren();

      /* Print function arguments. */
      if (math->getNumChildren() > 1)
      {
        Rcout << "(" << ( math->getLeftChild() )->getName();

        for (int i = 1; i < numChildren - 1; ++i)
        {
          Rcout <<", " << ( math->getChild(i) )->getName();
        }
      }

      Rcout <<") := ";

      /* Print function body. */
      if (numChildren == 0)
      {
        Rcout << "No Function Body Defined)" << endl;
      }
      else
      {
        math    = math->getChild(math->getNumChildren() - 1);
        Rcout << r2sbml::formulaToInfix(math) << endl;
      }
    }
  }
  return 0;
}

// printEventAssignmentMath - internal helper, not exported to R
void printEventAssignmentMath (unsigned int n, const EventAssignment* ea)
{
  std::string variable;

  if ( ea->isSetMath() )
  {
    variable = ea->getVariable();

    Rcout <<"  EventAssignment " << n
          << ", trigger: " << variable << " = "
          << r2sbml::formulaToInfix( ea->getMath() ) << endl;
  }
}

//'getEventMath
//'Outputs Model which can be simulated
//'@param input_model input should be an SBML Model
//'@examples
//'sbml_file <- system.file("examples", "sbmlevent.xml", package = "r2sbml")
//'model <- getModel(sbml_file)
//'getEventMath(model)
// [[Rcpp::export]]
int getEventMath (SEXP input_model) {

  Model* model = Rcpp::XPtr<Model>(input_model);
  if (model == 0)
  {
    Rcpp::stop("No model present.");
  }

  int numEvents = model->getNumEvents();

  if (numEvents == 0)
  {
    Rcpp::stop("No Events present in the model.");
  }

  for(int n = 0; n < numEvents; n++)
  {
    unsigned int i;
    Event* e = model->getEvent(n);

    if ( e->isSetDelay() )
    {
      Rcout << "Event " << n << " delay: "
            << r2sbml::formulaToInfix( e->getDelay()->getMath() ) << endl;
    }

    if ( e->isSetTrigger() )
    {
      Rcout << "Event " << n << " trigger: "
            << r2sbml::formulaToInfix( e->getTrigger()->getMath() ) << endl;
    }

    for (i = 0; i < e->getNumEventAssignments(); ++i)
    {
      printEventAssignmentMath(i + 1, e->getEventAssignment(i));
    }
  }
return 0;
}
