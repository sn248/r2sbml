# getSpeciesNames Outputs the Names of Compartments

getSpeciesNames Outputs the Names of Compartments

## Usage

``` r
getSpeciesNames(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

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
getSpeciesNames(model)
#> 
#> 
#> 
#> 
#> [1] "" "" "" ""
```
