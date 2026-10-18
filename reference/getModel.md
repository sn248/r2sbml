# Read an SBML model

Reads an SBML file and keeps the parsed document alive for the other
query functions. A file with error- or fatal-severity diagnostics is an
error; lesser diagnostics are raised as warnings.

## Usage

``` r
getModel(filename)
```

## Arguments

- filename:

  input file name, should end with .xml

## Value

an external pointer to the model, to pass to the other functions

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
```
