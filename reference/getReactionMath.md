# Print the rate laws of a model

Prints each reaction's kinetic law as infix math.

## Usage

``` r
getReactionMath(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

0; called for the printed output

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
getReactionMath(model)
#> Reaction 0, formula: comp * (kon * E * S - koff * ES)
#> Reaction 1, formula: comp * kcat * ES
#> [1] 0
```
