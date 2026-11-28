# Write a model to an SBML file

Writes the model, with any edits made by
[`setParameterValue()`](https://sn248.github.io/r2sbml/reference/editModel.md),
[`setCompartmentSize()`](https://sn248.github.io/r2sbml/reference/editModel.md)
or
[`setSpeciesIC()`](https://sn248.github.io/r2sbml/reference/editModel.md),
to an SBML file.

## Usage

``` r
writeSBML(input_model, filename)
```

## Arguments

- input_model:

  a model pointer from
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)

- filename:

  output file name, conventionally ending in `.xml`

## Value

NULL invisibly

## Details

The model is first run through libSBML's consistency checks. A model
with error- or fatal-severity diagnostics is not written, so an edit can
never leave an invalid file behind; lesser diagnostics are raised as a
warning and the file is written anyway, the rule
[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
applies when reading. Because these checks are more thorough than the
ones made on reading, a model
[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
accepted can still be refused here.

libSBML writes the whole document in its own layout, so a diff against
the original file shows changes of indentation and attribute order
besides the edits. The SBML level and version are kept.

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
out_file <- tempfile(fileext = ".xml")
writeSBML(model, out_file)
getParameterTable(getModel(out_file))
#> 
#>   filename: /tmp/RtmpksNvu1/file265c65d77e68.xml
#>   error(s): 0
#> 
#> 
#> File: /tmp/RtmpksNvu1/file265c65d77e68.xml (Level 3, version 2)
#>   Number ID Name Value                     Units
#> 1      1 k1       1000 litre_per_mole_per_second
#> 2      2 k2       2500 litre_per_mole_per_second
#> 3      3 KT      25000                per_second
#> 4      4 k3       2500                per_second
```
