# getReactionTable Outputs the Information Table for Reactions

getReactionTable Outputs the Information Table for Reactions

## Usage

``` r
getReactionTable(input_model)
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
getReactionTable(model)
#>   Number   ID Name Reversible                          RateLaw
#> 1      1  veq            TRUE comp * (kon * E * S - koff * ES)
#> 2      2 vcat           FALSE                 comp * kcat * ES
```
