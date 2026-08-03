# convertReactions

convertReactions

## Usage

``` r
convertReactions(infile, outfile, format = "R")
```

## Arguments

- infile:

  input file name

- outfile:

  output file name

- format:

  output code format, one of 'R'/'deSolve' (default), 'mrgsolve',
  'nlmixr2' (or 'rxode2'), 'MATLAB', 'Julia' or 'ubiquity'. An
  unrecognised format is an error.

## Value

NULL invisibly

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
```
