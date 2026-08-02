# Package index

## Reading a model

Everything except
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
works in two phases: read the file once, then query the pointer that
comes back.

- [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md) :
  getModel Outputs an external pointer to the Model from SBML file

## Querying components

These return R objects. Note that a query for a component the model does
not have raises an error rather than returning an empty result.

- [`getSpeciesTable()`](https://sn248.github.io/r2sbml/reference/getSpeciesTable.md)
  : getspeciesTable Outputs the Information Table for Species
- [`getParameterTable()`](https://sn248.github.io/r2sbml/reference/getParameterTable.md)
  : getParameterTable Outputs the Information Table for Paremeters
- [`getReactionTable()`](https://sn248.github.io/r2sbml/reference/getReactionTable.md)
  : getReactionTable Outputs the Information Table for Reactions
- [`getSpeciesNames()`](https://sn248.github.io/r2sbml/reference/getSpeciesNames.md)
  : getSpeciesNames Names of the species in a model
- [`getSpeciesIC()`](https://sn248.github.io/r2sbml/reference/getSpeciesIC.md)
  : getSpeciesIC Initial values of the species in a model
- [`getNumSpecies()`](https://sn248.github.io/r2sbml/reference/getNumSpecies.md)
  : getSpeciesNum Outputs the number of Species in the model
- [`getCmtNames()`](https://sn248.github.io/r2sbml/reference/getCmtNames.md)
  : getCmtNames Names of the compartments in a model
- [`getCmtSizes()`](https://sn248.github.io/r2sbml/reference/getCmtSizes.md)
  : getCmtSizes Outputs the Sizes of Compartments

## Printing the mathematics

These write to the console and return `0` invisibly, so capture them
with [`capture.output()`](https://rdrr.io/r/utils/capture.output.html)
rather than assigning them.

- [`getRuleMath()`](https://sn248.github.io/r2sbml/reference/getRuleMath.md)
  : getRuleMath Outputs Model which can be simulated
- [`getReactionMath()`](https://sn248.github.io/r2sbml/reference/getReactionMath.md)
  : getReactionMath Outputs Model which can be simulated
- [`getFunctionDefinition()`](https://sn248.github.io/r2sbml/reference/getFunctionDefinition.md)
  : getFunctionDefinition Outputs Model which can be simulated
- [`getEventMath()`](https://sn248.github.io/r2sbml/reference/getEventMath.md)
  : getEventMath Outputs Model which can be simulated

## Printing the document

The exception to the two-phase rule: these take a filename, not a model
pointer.

- [`printSBML()`](https://sn248.github.io/r2sbml/reference/printSBML.md)
  : printSBML
- [`echoSBML()`](https://sn248.github.io/r2sbml/reference/echoSBML.md) :
  echoSBML

## Exporting mass balances

Writes ready-to-simulate code for one of six ODE targets. Reads the SBML
file itself, so it takes no model pointer.

- [`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
  : convertReactions
