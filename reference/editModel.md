# Edit a model's values

Set the value of a parameter, the size of a compartment, or the initial
value of a species, in place.

## Usage

``` r
setParameterValue(input_model, id, value)

setCompartmentSize(input_model, id, value)

setSpeciesIC(input_model, id, value, as)
```

## Arguments

- input_model:

  a model pointer from
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)

- id:

  the id of the parameter, compartment or species

- value:

  the new value, a single number; `NA` unsets it

- as:

  for `setSpeciesIC()`, whether `value` is an `"amount"` or a
  `"concentration"`

## Value

the model pointer, invisibly, so calls can be chained

## Details

The model is changed in place, unlike an ordinary R object: every
variable that holds the same pointer sees the edit. Use
[`copyModel()`](https://sn248.github.io/r2sbml/reference/copyModel.md)
first to keep the original. The query functions show the edit straight
away, and
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
and
[`writeSBML()`](https://sn248.github.io/r2sbml/reference/writeSBML.md)
use it.

A species carries an initial amount or an initial concentration, never
both, so `setSpeciesIC()` needs to be told which `value` is: it sets
that attribute and unsets the other.

An unknown `id` is an error. When an initial assignment or an assignment
rule sets the component, the new value is stored but has no effect, and
a warning says so; a rate rule starts from the new value, so it does
have one.

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
setParameterValue(model, "k1", 1000)
setCompartmentSize(model, "nucleus", 2)
setSpeciesIC(model, "X", 0.5, as = "concentration")
getParameterTable(model)
#>   Number ID Name Value                     Units
#> 1      1 k1       1000 litre_per_mole_per_second
#> 2      2 k2       2500 litre_per_mole_per_second
#> 3      3 KT      25000                per_second
#> 4      4 k3       2500                per_second
getSpeciesTable(model)
#>   Number Name  ID InitialConcentration InitialAmount Compartment CompartmentVol
#> 1      1        X                  0.5           NaN     nucleus              2
#> 2      2      Y1n                  NaN             1     nucleus              2
#> 3      3      Y1c                  NaN             0   cytoplasm              5
#> 4      4       Y2                  NaN             1   cytoplasm              5
#>   BoundaryCondition Constant
#> 1                 1    FALSE
#> 2                 0    FALSE
#> 3                 0    FALSE
#> 4                 0    FALSE
```
