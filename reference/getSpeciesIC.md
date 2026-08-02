# getSpeciesIC Initial values of the species in a model

A species carries `initialAmount` *or* `initialConcentration`, never
both, and the unset one reads back as NaN. Asking only for the
concentration therefore returned NaN for every species in an
amount-based model. This returns whichever attribute the model actually
sets.

## Usage

``` r
getSpeciesIC(input_model)
```

## Arguments

- input_model:

  input should be an SBML Model

## Value

a named numeric vector, one entry per species, named by species id

## Details

The value is reported **as the file states it**, with no unit
conversion, so a model that mixes amount-valued and concentration-valued
species yields a vector that mixes units too –
[`getSpeciesTable()`](https://sn248.github.io/r2sbml/reference/getSpeciesTable.md)
shows which column each species used. This deliberately differs from
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md),
which must divide an amount by the compartment volume because the ODE it
generates integrates concentrations.

A species that sets neither attribute – its value comes from an initial
assignment or a rule – is reported as `NA`, not 0.

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
getSpeciesIC(model)
#>     E     S     P    ES 
#> 5e-21 1e-20 0e+00 0e+00 
```
