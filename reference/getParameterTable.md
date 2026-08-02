# getParameterTable Outputs the Information Table for Paremeters

getParameterTable Outputs the Information Table for Paremeters

## Usage

``` r
getParameterTable(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

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
getParameterTable(model)
#>   Number  ID Name Value         Units
#> 1      1 Keq        2.5 dimensionless
```
