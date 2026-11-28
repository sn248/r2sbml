# Remove a reaction

Delete a reaction from a model, in place, with its species references
and kinetic law (local parameters included).

## Usage

``` r
removeReaction(input_model, id)
```

## Arguments

- input_model:

  a model pointer from
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)

- id:

  the id of the reaction

## Value

the model pointer, invisibly, so calls can be chained

## Details

Removal is refused, and the model left unchanged, when something that
stays behind still uses the reaction: math that reads its id (in Level 3
that is the reaction's rate) or the id of one of its species references
(that is a stoichiometry), or a rule, initial assignment or event
assignment that sets such a stoichiometry. The error lists what uses it.

The species stay. A species no other reaction or rule changes keeps its
initial value from then on. As with the other setters the model changes
in place; use
[`copyModel()`](https://sn248.github.io/r2sbml/reference/copyModel.md)
first to keep the original. To switch a reaction off but keep it, set
its rate law to zero with `setRateLaw(model, id, "0")`.

## Examples

``` r
sbml_file <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlmutlicompartment.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlmutlicompartment.xml (Level 3, version 2)
removeReaction(model, "degradation")
getReactionMath(model)
#> Reaction 0, formula: nucleus * k1 * X * Y1n
#> Reaction 1, formula: cytoplasm * KT * (Y1n - Y1c)
#> Reaction 2, formula: cytoplasm * k2 * Y1c * Y2
#> [1] 0
```
