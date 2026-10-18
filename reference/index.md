# Package index

## Reading a model

Everything except
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
works in two phases: read the file once, then query the pointer that
comes back.

- [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md) :
  Read an SBML model

## Querying components

These return R objects. Note that a query for a component the model does
not have raises an error rather than returning an empty result.

- [`getSpeciesTable()`](https://sn248.github.io/r2sbml/reference/getSpeciesTable.md)
  : Table of the species in a model
- [`getParameterTable()`](https://sn248.github.io/r2sbml/reference/getParameterTable.md)
  : Table of the parameters in a model
- [`getReactionTable()`](https://sn248.github.io/r2sbml/reference/getReactionTable.md)
  : Table of the reactions in a model
- [`getSpeciesNames()`](https://sn248.github.io/r2sbml/reference/getSpeciesNames.md)
  : Names of the species in a model
- [`getSpeciesIC()`](https://sn248.github.io/r2sbml/reference/getSpeciesIC.md)
  : Initial values of the species in a model
- [`getNumSpecies()`](https://sn248.github.io/r2sbml/reference/getNumSpecies.md)
  : Number of species in a model
- [`getCmtNames()`](https://sn248.github.io/r2sbml/reference/getCmtNames.md)
  : Names of the compartments in a model
- [`getCmtSizes()`](https://sn248.github.io/r2sbml/reference/getCmtSizes.md)
  : Sizes of the compartments in a model

## Printing the mathematics

These write to the console and return `0` invisibly, so capture them
with [`capture.output()`](https://rdrr.io/r/utils/capture.output.html)
rather than assigning them.

- [`getRuleMath()`](https://sn248.github.io/r2sbml/reference/getRuleMath.md)
  : Print the rules of a model
- [`getReactionMath()`](https://sn248.github.io/r2sbml/reference/getReactionMath.md)
  : Print the rate laws of a model
- [`getFunctionDefinition()`](https://sn248.github.io/r2sbml/reference/getFunctionDefinition.md)
  : Print the function definitions of a model
- [`getEventMath()`](https://sn248.github.io/r2sbml/reference/getEventMath.md)
  : Print the events of a model

## Printing the document

The exception to the two-phase rule: these take a filename, not a model
pointer.

- [`printSBML()`](https://sn248.github.io/r2sbml/reference/printSBML.md)
  : Summarise an SBML file
- [`echoSBML()`](https://sn248.github.io/r2sbml/reference/echoSBML.md) :
  Check an SBML file for errors

## Exporting mass balances

Writes ready-to-simulate code for one of six ODE targets. Reads the SBML
file itself, so it takes no model pointer.

- [`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
  : Convert an SBML model to simulation code
