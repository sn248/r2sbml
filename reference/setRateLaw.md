# Replace a reaction's rate law

Parse an infix formula and make it the kinetic law of a reaction, in
place.

## Usage

``` r
setRateLaw(input_model, id, formula)
```

## Arguments

- input_model:

  a model pointer from
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)

- id:

  the id of the reaction

- formula:

  the new rate law, a single string

## Value

the model pointer, invisibly, so calls can be chained

## Details

`formula` uses libSBML's Level 3 infix syntax, the same syntax
[`getReactionMath()`](https://sn248.github.io/r2sbml/reference/getReactionMath.md)
prints: `k1 * S`, `Vmax * S / (Km + S)`, `S^2`, `exp(-k * time)`, and
calls to the model's function definitions. Every name in it must be a
compartment, species, parameter, reaction or species reference of the
model, one of the reaction's local parameters, or a function definition;
anything else is an error and the model is left unchanged.

The reaction's local parameters are kept, whether or not the new formula
uses them. A reaction without a kinetic law gets one. As with the other
setters the model changes in place; see
[editModel](https://sn248.github.io/r2sbml/reference/editModel.md).
[`writeSBML()`](https://sn248.github.io/r2sbml/reference/writeSBML.md)
runs the full consistency checks, so it reports units that do not add up
as warnings.

## Examples

``` r
sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml (Level 3, version 2)
# vcat's local parameter kcat is kept, so the new law can still use it
setRateLaw(model, "vcat", "comp * kcat * ES / (1 + P)")
getReactionMath(model)
#> Reaction 0, formula: comp * (kon * E * S - koff * ES)
#> Reaction 1, formula: comp * kcat * ES / (1 + P)
#> [1] 0
```
