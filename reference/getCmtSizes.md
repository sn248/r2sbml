# getCmtSizes Outputs the Sizes of Compartments

getCmtSizes Outputs the Sizes of Compartments

## Usage

``` r
getCmtSizes(input_model)
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
getCmtSizes(model)
#> 1e-14
#> [1] 1e-14
```
