# getFunctionDefinition Outputs Model which can be simulated

getFunctionDefinition Outputs Model which can be simulated

## Usage

``` r
getFunctionDefinition(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Examples

``` r
sbml_file <- system.file("examples", "sbmlfunctiondefinition.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlfunctiondefinition.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlfunctiondefinition.xml (Level 3, version 2)
getFunctionDefinition(model)
#> FunctionDefinition 0, f(x) := x * 2
#> [1] 0
```
