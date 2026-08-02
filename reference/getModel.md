# getModel Outputs an external pointer to the Model from SBML file

getModel Outputs an external pointer to the Model from SBML file

## Usage

``` r
getModel(filename)
```

## Arguments

- filename:

  input file name, should end with .xml

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
