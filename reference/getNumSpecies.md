# Number of species in a model

Counts the species the model declares.

## Usage

``` r
getNumSpecies(input_model)
```

## Arguments

- input_model:

  input should be an SBML model

## Value

an integer, the number of species

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
getNumSpecies(model)
#> [1] 4
```
