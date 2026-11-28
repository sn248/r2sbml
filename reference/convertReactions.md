# Convert an SBML model to simulation code

Writes the model's mass balances, rules and parameters as code for the
chosen simulation tool, ready to simulate.

## Usage

``` r
convertReactions(infile, outfile, format = "R")
```

## Arguments

- infile:

  input SBML file name, or a model from
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)

- outfile:

  output file name

- format:

  output code format, one of 'R'/'deSolve' (default), 'mrgsolve',
  'nlmixr2' (or 'rxode2'), 'MATLAB', 'Julia' or 'ubiquity'. An
  unrecognised format is an error.

## Value

NULL invisibly

## Details

The first argument is either the path to an SBML file or a model already
read with
[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md).
Passing a loaded model converts a copy of it, so the model you keep in R
is left unchanged – an edited model can be converted directly, with no
round-trip through a temporary file.

## Examples

``` r
sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
out_file <- tempfile(fileext = ".R")
convertReactions(sbml_file, out_file, format = "R")
#> Conversion completed.
#> Number of ODEs - 4
out_file_mrg <- tempfile(fileext = ".cpp")
convertReactions(sbml_file, out_file_mrg, format = "mrgsolve")
#> Conversion completed.
#> Number of ODEs - 4
out_file_rx <- tempfile(fileext = ".R")
convertReactions(sbml_file, out_file_rx, format = "nlmixr2")
#> Conversion completed.
#> Number of ODEs - 4
out_file_m <- tempfile(fileext = ".m")
convertReactions(sbml_file, out_file_m, format = "MATLAB")
#> Conversion completed.
#> Number of ODEs - 4
out_file_jl <- tempfile(fileext = ".jl")
convertReactions(sbml_file, out_file_jl, format = "Julia")
#> Conversion completed.
#> Number of ODEs - 4
out_file_ub <- tempfile(fileext = ".txt")
convertReactions(sbml_file, out_file_ub, format = "ubiquity")
#> Conversion completed.
#> Number of ODEs - 4
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml (Level 3, version 2)
out_file_model <- tempfile(fileext = ".R")
convertReactions(model, out_file_model, format = "R")
#> Conversion completed.
#> Number of ODEs - 4
```
