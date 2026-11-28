# Copy a model

Edits made with
[`setParameterValue()`](https://sn248.github.io/r2sbml/reference/editModel.md),
[`setCompartmentSize()`](https://sn248.github.io/r2sbml/reference/editModel.md)
and
[`setSpeciesIC()`](https://sn248.github.io/r2sbml/reference/editModel.md)
change a model in place, and every R variable that holds the same
pointer sees them: `model2 <- model` copies the pointer, not the model.
`copyModel()` makes an independent copy of the whole SBML document, so
the two can be edited separately.

## Usage

``` r
copyModel(input_model)
```

## Arguments

- input_model:

  a model pointer from
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)

## Value

an external pointer to the copy

## Examples

``` r
sbml_file <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlmutlicompartment.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlmutlicompartment.xml (Level 3, version 2)
edited <- copyModel(model)
setParameterValue(edited, "k1", 1000)
getParameterTable(model)$Value[1]
#> [1] 2500
getParameterTable(edited)$Value[1]
#> [1] 1000
```
