# Check an SBML file for errors

Reads the file and prints the number of diagnostics libSBML reports,
then the diagnostics themselves.

## Usage

``` r
echoSBML(filename)
```

## Arguments

- filename:

  input file name

## Value

the number of diagnostics, as an integer

## Examples

``` r
sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
echoSBML(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml
#>   error(s): 0
#> 
#> [1] 0
```
