# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

`r2sbml` is an R package that wraps the `libsbml` C++ library via Rcpp. It reads SBML (Systems Biology Markup Language) XML files and lets users query model components (species, parameters, compartments, reactions, rules, events, function definitions) from R.

## Build & Development Commands

```sh
# Full build. On Unix/macOS `configure` runs tools/cmake_call.sh, which downloads
# and builds libsbml from source — the first build takes several minutes.
R CMD INSTALL .

# Regenerate src/Makevars from src/Makevars.in without rebuilding libsbml
./config.status src/Makevars
```

```r
devtools::install()
devtools::test()                                  # all tests
testthat::test_file("tests/testthat/test-convert.R")
devtools::document()                              # man/*.Rd from roxygen in src/*.cpp
Rcpp::compileAttributes()                         # after adding/changing any [[Rcpp::export]] signature
```

`R/RcppExports.R` and `src/RcppExports.cpp` are generated — never hand-edit them. Roxygen comments live in the `.cpp` files (as `//'`), get copied into `R/RcppExports.R` by `compileAttributes()`, and only then does `document()` produce `man/*.Rd`. So the order is always **compileAttributes() → document()**; skipping the first leaves the docs describing a stale signature.

CI (`.github/workflows/R-CMD-check.yaml`) runs `R CMD check --as-cran` on Linux/macOS/Windows with `error-on: warning`, so CRAN NOTEs about the bundled libsbml sources are build failures.

## Architecture

### Two-phase API

1. **`getModel(filename)`** — reads an SBML file, keeps the `SBMLDocument` alive, and returns an R `externalptr` (`Rcpp::XPtr<Model>`).
2. **Query functions** — every other function takes that pointer, does `Model* model = Rcpp::XPtr<Model>(input_model)`, and either returns an R object or prints.

The two return styles matter when writing tests:

- **Return data**: `getSpeciesTable`, `getParameterTable`, `getReactionTable` (`DataFrame`); `getSpeciesNames`, `getCmtNames` (`StringVector`); `getSpeciesIC`, `getCmtSizes` (`NumericVector`); `getNumSpecies` (`int`).
- **Print to `Rcout`, return `0`**: `getRuleMath`, `getReactionMath`, `getFunctionDefinition`, `getEventMath`, `printSBML`, `echoSBML`. Their output must be captured (`capture.output`), not compared to a return value.

Query functions `Rcpp::stop()` on an empty model *or an empty component list* — e.g. `getRuleMath` on a model with no rules errors rather than returning empty. Any test looping over `inst/examples/` must account for that.

### C++ layer (`src/`)

One `.cpp` file per topic, each function exported with `// [[Rcpp::export]]`:
`getModel.cpp`, `speciesInfo.cpp`, `paramInfo.cpp`, `compartmentInfo.cpp`, `reactionInfo.cpp`, `getMath.cpp` (rules/reactions/function definitions/events), `getSpeciesNum.cpp`, `printSBML.cpp`, `echoSBML.cpp`, `convertReactions.cpp`.

### libsbml is built from source, not linked system-wide

This is the part most likely to surprise you. `configure.ac` unconditionally sets `need_to_build=yes` and calls `tools/cmake_call.sh`, which:

1. downloads libsbml 5.21.0 from GitHub into `src/libsbml-src` (`src/scripts/libsbml_download.sh`),
2. applies a block of **CRAN compliance patches** with `perl -pi -e` — removing `srand`/`rand`, `cout`/`fprintf` to stderr, and rewriting `sprintf`→`snprintf` in libsbml sources (Writing R Extensions §1.6.4 forbids these entry points),
3. cmake-builds a static `libsbml-static.a` into `src/libsbml-install/` (the only libsbml artefact that outlives the build; it is gitignored and `.Rbuildignore`d, so every clone and every tarball rebuilds it from scratch),
4. normalizes CRLF and missing trailing newlines in the installed headers,
5. deletes `libsbml-src`/`libsbml-build` via an `EXIT` trap so `R CMD check` never sees them.

Consequences: the perl patches match **exact source lines of libsbml 5.21.0** — bumping `LIBSBML_VERSION` in `src/scripts/libsbml_download.sh` will silently no-op some patches and reintroduce CRAN check failures. `configure` is generated from `configure.ac` by autoconf; edit the `.ac`. `src/Makevars` is generated from `src/Makevars.in` and contains absolute paths, so it is machine-specific and not committed.

### Compiler flags and artefact size

`src/scripts/r_config.sh` exports R's compiler and flags for the cmake build. Use the plain `CC`/`CXX`/`CXXFLAGS` config variables: the `CXX11*` family was deprecated in R 4.3 and is **defunct** in R 4.6, where it returns nothing and silently leaves the flags empty. `R CMD config CXX` returns compiler *and* standard flag together (`g++ -std=gnu++20`), so the script splits the first word off for cmake and carries the rest in `CXXFLAGS`.

`tools/cmake_call.sh` deliberately removes `-g` from the flags it hands to libsbml (`strip_g_flag`). Debug info for a vendored library nobody debugs from R dominates everything: `libsbml-static.a` is ~154MB with it and ~13.5MB without, and it follows the linker into `r2sbml.so`. r2sbml's own sources still get R's flags verbatim. `src/Makevars.in` then runs `strip --strip-debug` on the built shared object via a `strippedLib` target, taking the installed `.so` from ~65MB to ~6.6MB.

There is nothing to gain by disabling libsbml docs/tests/examples: `WITH_EXAMPLES`, `WITH_CHECK`, `WITH_DOXYGEN`, every language binding, and `WITH_STABLE_PACKAGES`/`WITH_ALL_PACKAGES` all default to `OFF` in libsbml's `CMakeLists.txt`, and `BUILD_TESTING=OFF` is passed explicitly. The install tree's `share/` is 232KB of licence and README text.

Windows uses `configure.win` (writes `src/Makevars.win` directly, no autoconf) and links against the Rtools libxml2/bz2/lzma/iconv. `tools/winlibs.R` (rwinlib prebuilt download) is a leftover from the old approach and is no longer on the build path.

### Example SBML files (`inst/examples/`)

Ten `.xml` files covering simple reactions, assignment rules, algebraic rules, function definitions, delay, events, multi-compartment, boundary conditions, and conversion factors. Both test files iterate over them. `inst/examples` is in `.Rbuildignore`, so tests fall back from `system.file("examples", ..., package = "r2sbml")` to `../../inst/examples/` when the package is not installed — keep both paths in any new test.

## `convertReactions`

The one function that does not take a model pointer. `convertReactions(infile, outfile, format = "R")` reads the SBML file itself and writes ready-to-simulate code, returning `NULL` invisibly (tests assert this with `expect_invisible`). Formats: `"R"`/`"deSolve"`, `"mrgsolve"`, `"nlmixr2"`/`"rxode"`. An unrecognised format is silently a no-op rather than an error.

It applies four libsbml converters in sequence before emitting anything — `replaceReactions`, `promoteLocalParameters`, `expandInitialAssignments`, `expandFunctionDefinitions` — so the model reaching the writers is always rate-rule form with no local parameters or function definitions left. That is why the writers only ever walk rules and parameters. Each format has its own `writeFile*` helper (`writeFileR`, `writeFileMrgsolve`, `writeFileNlmixr2`), forward-declared at the top of the file.

Note that `convertReactions` opens `outputFile` and writes a header line, then each `writeFile*` helper reopens the same path with `std::ofstream`, truncating it. The header is therefore discarded and only the helper's output survives. Harmless today, but don't be surprised by the dead write.

Do not namespace-qualify libsbml types here (`libsbml::SBMLWriter`). libsbml's `WITH_CPP_NAMESPACE` defaults to `OFF` and the build does not enable it, so no `libsbml` namespace exists — `LIBSBML_CPP_NAMESPACE_USE` expands to nothing and qualified names fail to compile. A previous revert of this file to the stock libSBML sample broke the build exactly this way.
