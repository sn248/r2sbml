# getCmtNames Names of the compartments in a model

Returns the `name` where one is set and falls back to the `id`
otherwise, the same rule
[`getSpeciesNames()`](https://sn248.github.io/r2sbml/reference/getSpeciesNames.md)
uses. Previously this returned the `id` unconditionally while
[`getSpeciesNames()`](https://sn248.github.io/r2sbml/reference/getSpeciesNames.md)
returned the `name`, so the two disagreed about what a "name" was. For a
model whose compartments set no `name` – which is all ten of the bundled
examples – the result is unchanged.

## Usage

``` r
getCmtNames(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

a character vector, one entry per compartment, in model order

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
getCmtNames(model)
#> [1] "comp"
```
