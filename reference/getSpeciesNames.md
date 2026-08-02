# getSpeciesNames Names of the species in a model

SBML separates `id`, which every species must have, from `name`, which
is an optional human-readable label. Most models in the wild set only
`id`, so returning `name` alone gave a vector of empty strings. This
returns the `name` where one is set and falls back to the `id`
otherwise, which is how SBML tools conventionally display a species.
[`getCmtNames()`](https://sn248.github.io/r2sbml/reference/getCmtNames.md)
follows the same rule. Use
[`getSpeciesTable()`](https://sn248.github.io/r2sbml/reference/getSpeciesTable.md)
when you need the two columns separately.

## Usage

``` r
getSpeciesNames(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

a character vector, one entry per species, in model order

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
getSpeciesNames(model)
#> [1] "E"  "S"  "P"  "ES"
```
