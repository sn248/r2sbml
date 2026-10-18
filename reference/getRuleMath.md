# Print the rules of a model

Prints each rule as infix math; an algebraic rule is printed as `f = 0`.

## Usage

``` r
getRuleMath(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

0; called for the printed output

## Examples

``` r
sbml_file <- system.file("examples", "sbmlassignmentrules.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlassignmentrules.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlassignmentrules.xml (Level 3, version 2)
getRuleMath(model)
#> Rule 0, formula: S1 = T / (1 + Keq)
#> Rule 1, formula: S2 = Keq * S1
#> [1] 0
```
