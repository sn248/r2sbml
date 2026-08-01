/**
 * @file    convertReactions.cpp
 * @brief   Loads an SBML File and converts reactions to ODEs
 * @author  Sarah Keating
 * @author  Frank T. Bergmann
 * @author  Satyaprakash Nayak
 * https://stackoverflow.com/questions/41938120/how-can-i-translate-an-sbml-formula-to-program-code
 * https://groups.google.com/g/sbml-discuss/c/inS4Lzp3Ri8/m/XEL-aeiAAgAJ
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
#include <iomanip>
#include <cctype>
#include <vector>
#include <chrono>
#include <ctime>
#include <sbml/SBMLTypes.h>
#include <sbml/conversion/ConversionProperties.h>

#include "formulaToInfix.h"

using namespace std;
LIBSBML_CPP_NAMESPACE_USE

int writeFileR(SBMLDocument*, std::string);
int writeFileMrgsolve(SBMLDocument*, std::string);
int writeFileNlmixr2(SBMLDocument*, std::string);
int writeFileMatlab(SBMLDocument*, std::string);
int writeFileJulia(SBMLDocument*, std::string);

// Starting value for a species.
//
// A species carries initialAmount or initialConcentration, not both, and the
// unset one reads back as NaN.  Asking only for the amount therefore printed
// the literal `nan` for every concentration-based model -- which is not a
// number in R, C++ or rxode2 -- so take whichever attribute is actually set.
static double speciesInitialValue(const Species* species)
{
   if (species->isSetInitialAmount())        return species->getInitialAmount();
   if (species->isSetInitialConcentration()) return species->getInitialConcentration();
   return 0.0;
}

//'convertReactions
//'@param infile input file name
//'@param outfile output file name
//'@param format output code format, one of 'R'/'deSolve' (default), 'mrgsolve',
//' 'nlmixr2' (or 'rxode'), 'MATLAB' or 'Julia'.  An unrecognised format is an error.
//'@examples
//'sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
//'out_file <- tempfile(fileext = ".R")
//'convertReactions(sbml_file, out_file, format = "R")
//'out_file_mrg <- tempfile(fileext = ".cpp")
//'convertReactions(sbml_file, out_file_mrg, format = "mrgsolve")
//'out_file_rx <- tempfile(fileext = ".R")
//'convertReactions(sbml_file, out_file_rx, format = "nlmixr2")
//'out_file_m <- tempfile(fileext = ".m")
//'convertReactions(sbml_file, out_file_m, format = "MATLAB")
//'out_file_jl <- tempfile(fileext = ".jl")
//'convertReactions(sbml_file, out_file_jl, format = "Julia")
//'@return NULL invisibly
// [[Rcpp::export]]
 void convertReactions(SEXP infile, SEXP outfile, std::string format = "R"){

   if(!infile) Rcpp::stop("Input file is not present.\n");
   if(!outfile) Rcpp::stop("Output file is not present.\n");

   // read document and assign file for writing
   std::string inputFile = Rcpp::as<std::string>(infile);
   if (inputFile == "") Rcpp::stop("Input file path is empty.\n");
   std::string outputFile = Rcpp::as<std::string>(outfile);

   SBMLReader reader;
   SBMLDocument* document  = reader.readSBMLFromFile(inputFile);

   unsigned int  errors    = document->getNumErrors(LIBSBML_SEV_ERROR);

   // stop in case of errors
   if (errors > 0)
   {
     Rcpp::Rcerr << "Encountered the following SBML errors:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion skipped.  Please correct the problems above first.");
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
     Rcpp::stop("Conversion failed.");
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
     Rcpp::stop("Conversion failed.");
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
     Rcpp::stop("Conversion failed.");
   }

   // create conversion object that identifies the function definition expansion converter
   props.addOption("expandFunctionDefinitions", true,
                   "Replace reactions with rateRules");
   // convert
   success = document->convert(props);

   if (success != LIBSBML_OPERATION_SUCCESS)
   {
     Rcpp::Rcerr << "Unable to perform conversion due to the following:" << endl;
     document->printErrors(Rcpp::Rcerr);
     Rcpp::stop("Conversion failed.");
   }

   // make names equal to ID if name doesn't exist

   Rcpp::Rcout << "Conversion completed." << endl;
   Rcpp::Rcout << "Number of ODEs - " << document->getModel()->getNumRules() << endl;
   int numRules = document->getModel()->getNumRules();

   std::ofstream out(outputFile);
   out << "// Model equations generated from .xml file \n" << endl;

   if (format.compare("R") == 0 || format.compare("deSolve") == 0)  {
      writeFileR(document, outputFile);

      for(int i = 0; i < numRules; i++)
      {
        // mathML to infix
        // out << "ODE for " << document->getModel()->getRule(i)->getVariable()
        // << " is " << document->getModel()->getRule(i)->getFormula() << endl;
      }
      out.close();
   }
   else if (format.compare("mrgsolve") == 0)  {
      writeFileMrgsolve(document, outputFile);
   }
   else if (format.compare("nlmixr2") == 0 || format.compare("rxode") == 0)  {
      writeFileNlmixr2(document, outputFile);
   }
   else if (format.compare("MATLAB") == 0 || format.compare("matlab") == 0)  {
      writeFileMatlab(document, outputFile);
   }
   else if (format.compare("Julia") == 0 || format.compare("julia") == 0)  {
      writeFileJulia(document, outputFile);
   }
   else {
      // Without this an unrecognised format quietly leaves an empty file
      // behind, which looks like a successful conversion.
      Rcpp::stop("Unknown format '" + format + "'. Use one of 'R'/'deSolve', "
                 "'mrgsolve', 'nlmixr2'/'rxode', 'MATLAB' or 'Julia'.");
   }

   // SBMLWriter writer;
   // writer.writeSBMLToFile(document, outputFile);
   // libsbml::writeSBMLToFile(document, outputFile);
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
   out << "##        speciesTypes: " << model->getNumSpeciesTypes       () << endl;
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

   out << "## Units Definitions" << endl;
   int nUnitsDef = model->getNumUnitDefinitions();
   for (int i = 0; i<nUnitsDef; i++){
     int nListofUnits = model->getUnitDefinition(i)->getNumUnits();
     for (int j = 0; j<nListofUnits; j++){
       out << "## " << model->getUnitDefinition(i)->getId() << ": Kind=" << model->getUnitDefinition(i)->getUnit(j)->getName() << ", : Exponent=" << model->getUnitDefinition(i)->getUnit(j)->getExponent()  << endl;
     }
   }

   out << endl;
   out << "## Compartments " << endl;
   // out << "Comaparmtments <- c(" << endl;
   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        out <<  model->getCompartment(i)->getId() << " = " << model->getCompartment(i)->getVolume() << endl; //" # (" << model->getCompartment(i)->getUnits() << ")" << endl;
   }

   out << endl;

   out <<"## Initial Amounts" << endl;
   out << "InitialAmounts <- c(" << endl;
   int numIAs = model->getNumSpecies();
   for (int i = 0; i < numIAs; i++){
      if (i != numIAs-1){
        out << "         " << model->getSpecies(i)->getIdAttribute() << " = " << speciesInitialValue(model->getSpecies(i)) << "," << endl;
      }
      if (i == numIAs-1) {
        out << "         " << model->getSpecies(i)->getIdAttribute() << " = " << speciesInitialValue(model->getSpecies(i)) << endl;
      }
   }

  out << "                    )" << endl;

   out << endl;

   out << "## Parameters" << endl;
   out << "parameters <- c(" << endl;
   int numParams = model->getNumParameters();
   for (int i = 0; i < numParams; i++){
      if (i != numParams-1 ){
        out << "         " << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << "," << endl;
      }
      if (i == numParams-1) {
        out << "         " << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << endl;
      }
   }

   // for (int i = 0; i < numParams; i++) {
   //      out << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << endl; // "# (" << model->getParameter(i)->getUnits() << ")" << endl;
   // }

   out << "               )" << endl;
   out << endl;

   // for (int i = 0; i < numIAs-1; i++) {
   //      out << model->getSpecies(i)->getIdAttribute() << " = " << speciesInitialValue(model->getSpecies(i)) << endl; // " # (" <<  model->getSpecies(i)->getUnits() << ")" << endl;
   // }


   // Reactions (only comments)
   out << "## Reactions " << endl;
   int numReactions = model->getNumReactions();
   for (int i = 0; i < numReactions; i++){
        out <<  "## Reaction " << i << ": " << model->getReaction(i)->getKineticLaw() << endl; //" # (" << model->getCompartment(i)->getUnits() << ")" << endl;
   }

   out << endl;

   out << "## Mass-Balances (ODEs)" << endl;
   out << "massBalances <- function(time, states, params){" << endl << endl;

   out << "   ## Get States Names " << endl;
   for (int i = 0; i<numIAs; i++){
        out << "   " << model->getSpecies(i)->getIdAttribute() << " = states[[\"" << model->getSpecies(i)->getIdAttribute() << "\"]]" << endl;
   }

   out << endl;
   out << "   ## Get Parameter Names " << endl;
   for (int i = 0; i<numParams; i++){
        out << "   " << model->getParameter(i)->getIdAttribute() << " = params[[\"" << model->getParameter(i)->getIdAttribute() << "\"]]" << endl;
   }

   int numODEs = model->getNumRules();
   // for (int i = 0; i < numODEs ; i++){
   //    if(i != numODEs - 1){
   //    out << "                 " << model->getRule(i)->getFormula() << "," << endl;
   //    } else {
   //    out << "                 " << model->getRule(i)->getFormula() << endl;
   //    }
   // }
   // out << "                 )" << endl;

   out << endl;
   out << "   ## Mass Balances" << endl;
   for (int i = 0; i < numODEs; i++){
     out << "   d" << model->getRule(i)->getVariable() << "_dt = "
         << r2sbml::formulaToInfix(model->getRule(i)->getMath()) << endl;
   }

   out << endl;
   out << "   ## Make a list of Mass Balances" << endl;
   out << "   MassBalances <- c(" << endl;
   for (int i = 0; i<numIAs; i++){
     if (i != numIAs-1){
        out << "     d" << model->getSpecies(i)->getIdAttribute() << "_dt" << " ," <<endl;
     }
     if (i == numIAs-1){
        out << "     d" << model->getSpecies(i)->getIdAttribute() << "_dt" <<endl;
     }
   }
   out << "   )" << endl;
   out << "   return(list(MassBalances))" << endl;
   out << endl;


   out << "}" << endl;
   // out << "## Algebraic Rules" << endl;
   // out << "algebraicRules <- c(" << endl;
   // out << "                  )" << endl;
   out << endl;


   out.close();
   return 0;

}

// write output ODEs for mrgsolve
int writeFileMrgsolve(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   Model* model = document->getModel();

   out << "## Automatically generated mrgsolve model file by r2sbml\n" << endl;
   out << "$PROB\n" << endl;
   out << "$PARAM\n";

   int numParams = model->getNumParameters();
   for (int i = 0; i < numParams; i++){
        out << model->getParameter(i)->getIdAttribute() << " = " << model->getParameter(i)->getValue() << "\n";
   }

   out << "\n$CMT\n";
   int numIAs = model->getNumSpecies();
   for (int i = 0; i < numIAs; i++){
        out << model->getSpecies(i)->getIdAttribute() << "\n";
   }

   out << "\n$MAIN\n";
   for (int i = 0; i < numIAs; i++){
        out << model->getSpecies(i)->getIdAttribute() << "_0 = " << speciesInitialValue(model->getSpecies(i)) << ";\n";
   }

   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        out <<  model->getCompartment(i)->getId() << " = " << model->getCompartment(i)->getVolume() << ";\n";
   }

   out << "\n$ODE\n";
   int numODEs = model->getNumRules();
   for (int i = 0; i < numODEs; i++){
     // mrgsolve model blocks are C++, so powers have to be pow() calls.
     out << "dxdt_" << model->getRule(i)->getVariable() << " = "
         << r2sbml::formulaToInfixC(model->getRule(i)->getMath()) << ";\n";
   }

   out.close();
   return 0;
}

// write output ODEs for nlmixr2 / rxode
int writeFileNlmixr2(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   Model* model = document->getModel();

   out << "## Automatically generated nlmixr2/rxode model file by r2sbml\n" << endl;
   out << "model <- function() {\n";
   out << "  ini({\n";

   int numParams = model->getNumParameters();
   for (int i = 0; i < numParams; i++){
        out << "    " << model->getParameter(i)->getIdAttribute() << " <- " << model->getParameter(i)->getValue() << "\n";
   }

   out << "  })\n";
   out << "  model({\n";

   int numCmt = model->getNumCompartments();
   for (int i = 0; i < numCmt; i++){
        out << "    " << model->getCompartment(i)->getId() << " <- " << model->getCompartment(i)->getVolume() << "\n";
   }

   int numIAs = model->getNumSpecies();
   for (int i = 0; i < numIAs; i++){
        out << "    " << model->getSpecies(i)->getIdAttribute() << "(0) <- " << speciesInitialValue(model->getSpecies(i)) << "\n";
   }

   out << "\n";
   int numODEs = model->getNumRules();
   for (int i = 0; i < numODEs; i++){
     out << "    d/dt(" << model->getRule(i)->getVariable() << ") <- "
         << r2sbml::formulaToInfix(model->getRule(i)->getMath()) << "\n";
   }

   out << "  })\n";
   out << "}\n";

   out.close();
   return 0;
}


// Index of the rate rule governing species @p speciesId, or -1 when the
// species has none.  After the replaceReactions conversion every reaction has
// become a rate rule, so a species without one is held constant -- a boundary
// condition, or a species taking part in no reaction.
static int rateRuleForSpecies(Model* model, const std::string& speciesId)
{
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     const Rule* r = model->getRule(i);
     if (r->isRate() && r->getVariable() == speciesId) return (int)i;
   }
   return -1;
}

// True when @p speciesId is the variable of an assignment rule.  Such a
// species is not integrated: its value follows from the rule at every time
// point, so it is emitted as a local inside the RHS rather than given a slot
// in the state vector.  Giving it one would leave the solver carrying a stale
// copy that never updates.
static bool hasAssignmentRule(Model* model, const std::string& speciesId)
{
   for (unsigned int i = 0; i < model->getNumRules(); i++)
   {
     const Rule* r = model->getRule(i);
     if (r->isAssignment() && r->getVariable() == speciesId) return true;
   }
   return false;
}

// Indices of the species that form the state vector, in model order.
static std::vector<int> integratedSpecies(Model* model)
{
   std::vector<int> states;
   for (unsigned int i = 0; i < model->getNumSpecies(); i++)
   {
     if (!hasAssignmentRule(model, model->getSpecies(i)->getIdAttribute()))
     {
       states.push_back((int)i);
     }
   }
   return states;
}

// MATLAB resolves a function by file name, so the leading function in a file
// has to be named after it.  Derive that name from the output path and force
// it into a legal identifier: letters, digits and underscores, starting with
// a letter.
static std::string matlabFunctionName(const std::string& path)
{
   std::string base = path;

   std::string::size_type slash = base.find_last_of("/\\");
   if (slash != std::string::npos) base = base.substr(slash + 1);

   std::string::size_type dot = base.find_last_of('.');
   if (dot != std::string::npos && dot > 0) base = base.substr(0, dot);

   for (std::string::size_type i = 0; i < base.size(); i++)
   {
     if (!isalnum((unsigned char)base[i])) base[i] = '_';
   }
   if (base.empty() || !isalpha((unsigned char)base[0])) base = "sbml_" + base;

   return base;
}

// write output ODEs for MATLAB
int writeFileMatlab(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   out << std::setprecision(15);
   Model* model = document->getModel();

   const std::string fname = matlabFunctionName(outfilename);
   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();

   int numParams = model->getNumParameters();
   int numCmt    = model->getNumCompartments();
   int numRules  = model->getNumRules();

   out << "% Automatically generated MATLAB model file by r2sbml\n";
   out << "%\n";
   out << "% Save as " << fname << ".m, then solve with:  [t, y] = " << fname << "();\n";
   out << "% Columns of y are, in order: ";
   for (int i = 0; i < numStates; i++){
     out << model->getSpecies(states[i])->getIdAttribute()
         << (i + 1 < numStates ? ", " : "\n");
   }
   out << "%\n";
   out << "% Model Summary\n";
   out << "%        compartments: " << numCmt << "\n";
   out << "%             species: " << model->getNumSpecies() << "\n";
   out << "%          parameters: " << numParams << "\n";
   out << "%               rules: " << numRules << "\n";
   out << "%              events: " << model->getNumEvents() << "\n\n";

   out << "function [t, y] = " << fname << "(tspan, y0)\n\n";

   out << "    if nargin < 1 || isempty(tspan)\n";
   out << "        tspan = [0 10];\n";
   out << "    end\n\n";

   out << "    % Compartments\n";
   for (int i = 0; i < numCmt; i++){
     out << "    " << model->getCompartment(i)->getId() << " = "
         << model->getCompartment(i)->getVolume() << ";\n";
   }
   out << "\n";

   out << "    % Parameters\n";
   for (int i = 0; i < numParams; i++){
     out << "    " << model->getParameter(i)->getIdAttribute() << " = "
         << model->getParameter(i)->getValue() << ";\n";
   }
   out << "\n";

   out << "    % Initial amounts\n";
   out << "    if nargin < 2 || isempty(y0)\n";
   out << "        y0 = [ ...\n";
   for (int i = 0; i < numStates; i++){
     out << "            " << speciesInitialValue(model->getSpecies(states[i]))
         << "; % " << model->getSpecies(states[i])->getIdAttribute() << "\n";
   }
   out << "        ];\n";
   out << "    end\n\n";

   out << "    [t, y] = ode15s(@massBalances, tspan, y0);\n\n";

   // Nested, so the compartment and parameter values above stay in scope.
   out << "    function dydt = massBalances(time, states)\n\n";

   out << "        % States\n";
   for (int i = 0; i < numStates; i++){
     out << "        " << model->getSpecies(states[i])->getIdAttribute()
         << " = states(" << i + 1 << ");\n";
   }
   out << "\n";

   // Assignment rules come after the state unpacking, since they may read
   // states, and before the derivatives, which may read them in turn.
   bool anyAssignment = false;
   for (int i = 0; i < numRules; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       if (!anyAssignment){ out << "        % Assignment rules\n"; anyAssignment = true; }
       out << "        " << r->getVariable() << " = "
           << r2sbml::formulaToInfixMatlab(r->getMath()) << ";\n";
     }
   }
   if (anyAssignment) out << "\n";

   out << "        % Mass balances\n";
   out << "        dydt = zeros(" << numStates << ", 1);\n";
   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     if (rule >= 0){
       out << "        dydt(" << i + 1 << ") = "
           << r2sbml::formulaToInfixMatlab(model->getRule(rule)->getMath())
           << "; % " << id << "\n";
     } else {
       out << "        % dydt(" << i + 1 << ") stays 0: " << id
           << " has no rate rule and is held constant\n";
     }
   }

   for (int i = 0; i < numRules; i++){
     const Rule* r = model->getRule(i);
     if (r->isAlgebraic()){
       out << "        % Algebraic rule, not expressible as an explicit ODE: 0 = "
           << r2sbml::formulaToInfixMatlab(r->getMath()) << "\n";
     }
   }

   out << "\n";
   out << "    end\n";
   out << "end\n";

   out.close();
   return 0;
}

// write output ODEs for Julia
int writeFileJulia(SBMLDocument* document, std::string outfilename)
{
   std::ofstream out(outfilename);
   out << std::setprecision(15);
   Model* model = document->getModel();

   const std::vector<int> states = integratedSpecies(model);
   const int numStates = (int)states.size();

   int numParams = model->getNumParameters();
   int numCmt    = model->getNumCompartments();
   int numRules  = model->getNumRules();

   out << "# Automatically generated Julia model file by r2sbml\n";
   out << "#\n";
   out << "# Solve with:  using DifferentialEquations; sol = solve(prob)\n";
   out << "# Elements of u are, in order: ";
   for (int i = 0; i < numStates; i++){
     out << model->getSpecies(states[i])->getIdAttribute()
         << (i + 1 < numStates ? ", " : "\n");
   }
   out << "#\n";
   out << "# Model Summary\n";
   out << "#        compartments: " << numCmt << "\n";
   out << "#             species: " << model->getNumSpecies() << "\n";
   out << "#          parameters: " << numParams << "\n";
   out << "#               rules: " << numRules << "\n";
   out << "#              events: " << model->getNumEvents() << "\n\n";

   out << "using DifferentialEquations\n\n";

   out << "function massbalances!(du, u, p, t)\n\n";

   // SBML's csymbol for time serialises as `time`; the solver passes `t`.
   out << "    time = t\n\n";

   out << "    # Compartments\n";
   for (int i = 0; i < numCmt; i++){
     out << "    " << model->getCompartment(i)->getId() << " = "
         << model->getCompartment(i)->getVolume() << "\n";
   }
   out << "\n";

   out << "    # Parameters\n";
   for (int i = 0; i < numParams; i++){
     out << "    " << model->getParameter(i)->getIdAttribute()
         << " = p[" << i + 1 << "]\n";
   }
   out << "\n";

   out << "    # States\n";
   for (int i = 0; i < numStates; i++){
     out << "    " << model->getSpecies(states[i])->getIdAttribute()
         << " = u[" << i + 1 << "]\n";
   }
   out << "\n";

   bool anyAssignment = false;
   for (int i = 0; i < numRules; i++){
     const Rule* r = model->getRule(i);
     if (r->isAssignment()){
       if (!anyAssignment){ out << "    # Assignment rules\n"; anyAssignment = true; }
       out << "    " << r->getVariable() << " = "
           << r2sbml::formulaToInfix(r->getMath()) << "\n";
     }
   }
   if (anyAssignment) out << "\n";

   out << "    # Mass balances\n";
   for (int i = 0; i < numStates; i++){
     const std::string id = model->getSpecies(states[i])->getIdAttribute();
     int rule = rateRuleForSpecies(model, id);
     if (rule >= 0){
       out << "    du[" << i + 1 << "] = "
           << r2sbml::formulaToInfix(model->getRule(rule)->getMath())
           << "  # " << id << "\n";
     } else {
       out << "    du[" << i + 1 << "] = 0.0  # " << id
           << " has no rate rule and is held constant\n";
     }
   }

   for (int i = 0; i < numRules; i++){
     const Rule* r = model->getRule(i);
     if (r->isAlgebraic()){
       out << "    # Algebraic rule, not expressible as an explicit ODE: 0 = "
           << r2sbml::formulaToInfix(r->getMath()) << "\n";
     }
   }

   out << "\n";
   out << "    return nothing\n";
   out << "end\n\n";

   // Float64[] rather than [], so a model whose initial amounts happen to be
   // whole numbers still gets a floating point state vector.
   out << "u0 = Float64[";
   for (int i = 0; i < numStates; i++){
     out << speciesInitialValue(model->getSpecies(states[i]))
         << (i + 1 < numStates ? ", " : "");
   }
   out << "]\n";

   out << "p = Float64[";
   for (int i = 0; i < numParams; i++){
     out << model->getParameter(i)->getValue() << (i + 1 < numParams ? ", " : "");
   }
   out << "]\n";

   out << "tspan = (0.0, 10.0)\n";
   out << "prob = ODEProblem(massbalances!, u0, tspan, p)\n";

   out.close();
   return 0;
}
