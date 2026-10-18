# Table of the reactions in a model

Lists each reaction with its rate law written as infix math.

## Usage

``` r
getReactionTable(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

a data frame with one row per reaction and columns `Number`, `ID`,
`Name`, `Reversible` and `RateLaw`

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
getReactionTable(model)
#>   Number   ID Name Reversible                          RateLaw
#> 1      1  veq            TRUE comp * (kon * E * S - koff * ES)
#> 2      2 vcat           FALSE                 comp * kcat * ES
```
