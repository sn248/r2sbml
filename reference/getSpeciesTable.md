# getspeciesTable Outputs the Information Table for Species

getspeciesTable Outputs the Information Table for Species

## Usage

``` r
getSpeciesTable(input_model)
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
getSpeciesTable(model)
#>   Number Name ID InitialConcentration InitialAmount Compartment CompartmentVol
#> 1      1       E                  NaN         5e-21        comp          1e-14
#> 2      2       S                  NaN         1e-20        comp          1e-14
#> 3      3       P                  NaN         0e+00        comp          1e-14
#> 4      4      ES                  NaN         0e+00        comp          1e-14
#>   BoundaryCondition Constant
#> 1                 0    FALSE
#> 2                 0    FALSE
#> 3                 0    FALSE
#> 4                 0    FALSE
```
