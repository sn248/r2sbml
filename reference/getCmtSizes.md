# Sizes of the compartments in a model

Reports each compartment's declared size, as the file states it.

## Usage

``` r
getCmtSizes(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

a numeric vector, one entry per compartment, in model order

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
getCmtSizes(model)
#> [1] 1e-14
```
