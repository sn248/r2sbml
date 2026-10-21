## Submission

This is a new submission.

## Test environments

* local Ubuntu, R 4.6 (R CMD check --as-cran)
* GitHub Actions: ubuntu-latest (devel, release, oldrel-1), macos-latest
  (release), windows-latest (release)
* win-builder (devel)
* mac-builder (release)

## R CMD check results

0 errors | 0 warnings | 1 note

* checking CRAN incoming feasibility ... NOTE
  New submission

  Possibly misspelled words in DESCRIPTION: Hucka, Bornstein, et, al

  These are the author names and "et al." of the two references cited in
  the Description field (Hucka et al. 2003; Bornstein et al. 2008), and are
  spelled correctly.

## Bundled library

The package bundles the source of 'libSBML' 5.21.0 (LGPL 2.1) as
`src/libsbml-mod-5.21.0.tar.gz`, an official release with the language
bindings, documentation, tests and unused SBML Level 3 packages removed; the
files kept are unchanged upstream sources. `configure` builds it as a static
library with cmake, so installing needs no network access. Before building, it
patches the 'libSBML' sources to remove `rand`/`srand`, writes to
stdout/stderr and `sprintf`, as Writing R Extensions requires. Its authors and
copyright holders are listed in `Authors@R` and in `inst/COPYRIGHTS`.

'libSBML' is built from source rather than linked from the system because it
is not packaged for most platforms, and where it is the version varies.
Building it takes a few minutes, which accounts for most of the install time.
