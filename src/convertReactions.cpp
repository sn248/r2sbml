/**
 * @file    convertReactions.cpp
 * @brief   Loads an SBML File and converts reactions to ODEs
 * @author  Sarah Keating
 * @author  Frank T. Bergmann
 * @author  Satyaprakash Nayak
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

#include <Rcpp.h>
#include <fstream>
#include <iostream>
#include <chrono>
#include <ctime>
#include <sbml/SBMLTypes.h>
#include <sbml/conversion/ConversionProperties.h>

using namespace std;
LIBSBML_CPP_NAMESPACE_USE

int writeFileR(SBMLDocument*, std::string);

//'convertReactions
//'@param infile input file name
//'@param outfile output file name
//'@param format output code format, should be 'MATLAB','mrgsolve',rxode' or 'R' (default)
// [[Rcpp::export]]
 int convertReactions(SEXP infile, SEXP outfile, std::string format = "R"){

   // read document
   std::string inputFile = Rcpp::as<std::string>(infile);
   std::string outputFile = Rcpp::as<std::string>(outfile);

   SBMLReader reader;
   SBMLDocument* document  = reader.readSBMLFromFile(inputFile);

   unsigned int  errors    = document->getNumErrors(LIBSBML_SEV_ERROR);

   // stop in case of errors
   if (errors > 0)
   {
     Rcpp::Rcerr << "Encountered the following SBML errors:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::Rcerr << "Conversion skipped.  Please correct the problems above first."
                 << endl;
     return errors;
   }

   // create conversion object that identifies the function definition converter
   ConversionProperties props;
   props.addOption("replaceReactions", true,
                   "Replace reactions with rateRules");
   // convert
   int success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     return errors;
   }

   // promote local parameters to global parameters
   props.addOption("promoteLocalParameters", true,
                   "Promote Local Parameters to Global");
   // convert
   success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     return errors;
   }

   // expand Initial Assignments
   props.addOption("expandInitialAssignments", true,
                   "Expanding Initial Assignments");
   // convert
   success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     return errors;
   }
   else
   {
     // make names equal to ID if name doesn't exist

     Rcpp::Rcout << "Conversion completed." << endl;
     Rcpp::Rcout << "Number of ODEs - " << document->getModel()->getNumRules() << endl;
     int numRules = document->getModel()->getNumRules();

     std::ofstream out(outputFile);
     out << "// Model equations generated from .xml file \n" << endl;

     // std::string outFormat = format;
     if (format.compare("R") == 0)  { writeFileR(document, outputFile); }

     for(int i = 0; i < numRules; i++)
     {
       // mathML to infix
       // out << "ODE for " << document->getModel()->getRule(i)->getVariable()
       // << " is " << document->getModel()->getRule(i)->getFormula() << endl;

     }

     out.close();

     // SBMLWriter writer;
     // writer.writeSBMLToFile(document, outputFile);
     // libsbml::writeSBMLToFile(document, outputFile);
   }

   return 0;
 }

// write output ODEs for R
int writeFileR(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   Model* model = document->getModel();

   // auto now = std::chrono::system_clock::now();
   out << "## Automatically generated model file by r2sbml at " << endl;

   out << "## Writing the model ODEs for solving in R" << endl;
   out << "## Solving the model requires desolve package \n" << endl;
   out << "## Model Summary " << endl;
   out << "## functionDefinitions: " << model->getNumFunctionDefinitions() << endl;
   out << "##     unitDefinitions: " << model->getNumUnitDefinitions    () << endl;
   out << "##    compartmentTypes: " << model->getNumCompartmentTypes   () << endl;
   out << "##         specieTypes: " << model->getNumSpeciesTypes       () << endl;
   out << "##        compartments: " << model->getNumCompartments       () << endl;
   out << "##             species: " << model->getNumSpecies            () << endl;
   out << "##          parameters: " << model->getNumParameters         () << endl;
   out << "##  initialAssignments: " << model->getNumInitialAssignments () << endl;
   out << "##               rules: " << model->getNumRules              () << endl;
   out << "##         constraints: " << model->getNumConstraints        () << endl;
   out << "##           reactions: " << model->getNumReactions          () << endl;
   out << "##              events: " << model->getNumEvents             () << endl;
   out << endl;

   out << "## Load required packages" << endl;
   out << "library(deSolve)" << endl << endl;
   out << "## Parameter list" << endl;
   // out << "parameters <- function(){" << endl << endl;
   out << "params <- c(" << endl;
   int numParams = model->getNumParameters();
   for (int i = 0; i < numParams ; i++ ){
      if (i != numParams-1 ){
        out << "            " << model->getParameter(i)->getName() << " = " << model->getParameter(i)->getValue() << "," << endl;
      } else {
        out << "            " << model->getParameter(i)->getName() << " = " << model->getParameter(i)->getValue() << endl;
      }
   }
   out << "           )" << endl;
   // out << "}" << endl;
   out << endl;
   out <<"## Initial Conditions list" << endl;
   out << "ICs <- c(" << endl;
   int numICs = model->getNumSpecies();
   for (int i = 0; i < numICs ; i++ ){
      if (i != numICs-1 ){
        out << "         " << model->getSpecies(i)->getName() << " = " << model->getSpecies(i)->getInitialConcentration() << "," << endl;
      } else {
        out << "         " << model->getSpecies(i)->getName() << " = " << model->getSpecies(i)->getInitialConcentration() << endl;
      }
   }
   out << "        )" << endl;

   out << endl;
   out << "## Compartment list " << endl;
   int numCmt = model->getNumCompartments();
   for(int i = 0; i < numCmt; i++){
        out <<  model->getCompartment(i)->getId() << " = " << model->getCompartment(i)->getVolume() << endl;
   }
   out << endl;

   out << "## Mass-Balances (ODEs)" << endl;
   out << "massBalances <- c(" << endl;
   int numODEs = model->getNumRules();
   for (int i = 0; i < numODEs ; i++){
      if(i != numODEs - 1){
      out << "                 " << model->getRule(i)->getFormula() << "," << endl;
      } else {
      out << "                 " << model->getRule(i)->getFormula() << endl;
      }
   }
   out << "                 )" << endl;

   out.close();
   return 0;

}
