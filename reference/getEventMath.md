# getEventMath Outputs Model which can be simulated

getEventMath Outputs Model which can be simulated

## Usage

``` r
getEventMath(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Examples

``` r
sbml_file <- system.file("examples", "sbmlevent.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> Warning: Namespace prefix sbml for units on cn is not defined [201]
#> Warning: Namespace prefix sbml for units on cn is not defined [201]
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlevent.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlevent.xml (Level 3, version 2)
getEventMath(model)
#> Event 0 trigger: P1 > tau
#>   EventAssignment 1, trigger: G2 = 1
#> Event 1 trigger: P1 <= tau
#>   EventAssignment 1, trigger: G2 = 0
#> [1] 0
```
