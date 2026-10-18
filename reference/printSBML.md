# Summarise an SBML file

Prints the file's SBML level and version and how many of each component
its model has.

## Usage

``` r
printSBML(filename)
```

## Arguments

- filename:

  filename should be the name of the xml file

## Value

0 on success and 1 when the file has errors or no model; called for the
printed summary

## Examples

``` r
sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
printSBML(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml (Level 3, version 2)
#>                  id: (empty)
#> functionDefinitions: 0
#>     unitDefinitions: 2
#>    compartmentTypes: 0
#>         specieTypes: 0
#>        compartments: 1
#>             species: 4
#>          parameters: 0
#>  initialAssignments: 0
#>               rules: 0
#>         constraints: 0
#>           reactions: 2
#>              events: 0
#> 
#> [1] 0
```
