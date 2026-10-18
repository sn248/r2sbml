# Print the events of a model

Prints each event's trigger and assignments as infix math.

## Usage

``` r
getEventMath(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

0; called for the printed output

## Examples

``` r
sbml_file <- system.file("examples", "sbmlevent.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlevent.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlevent.xml (Level 3, version 2)
getEventMath(model)
#> Event 0 trigger: 1 * (P1 > tau)
#>   EventAssignment 1, trigger: G2 = 1
#> Event 1 trigger: 1 * (P1 <= tau)
#>   EventAssignment 1, trigger: G2 = 0
#> [1] 0
```
