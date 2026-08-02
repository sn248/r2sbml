# getspeciesIC Outputs the Initial Concentrations of Species

getspeciesIC Outputs the Initial Concentrations of Species

## Usage

``` r
getSpeciesIC(input_model)
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
getSpeciesIC(model)
#> [1] NaN NaN NaN NaN
```
