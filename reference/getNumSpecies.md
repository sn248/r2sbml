# getSpeciesNum Outputs the number of Species in the model

getSpeciesNum Outputs the number of Species in the model

## Usage

``` r
getNumSpecies(input_model)
```

## Arguments

- input_model:

  input should be an SBML model

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
