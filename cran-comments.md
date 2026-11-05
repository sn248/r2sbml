## Submission

This update fixes the installation failure on
r-devel-linux-x86_64-fedora-clang. The bundled 'libSBML' sources use
`new(nothrow)` without including `<new>`, which libc++ 23 no longer provides
through other headers. `configure` now adds the include before building, and
stops with an error if the 'libSBML' build fails.

## Test environments

* Debian testing, R 4.6.1, clang 23.1.6 with libc++ (install and tests)
* Debian testing, R 4.6.1, GCC 16.2 (install and tests)

TODO before submitting: add the R CMD check --as-cran runs for this version
(local, GitHub Actions, win-builder, mac-builder).

## R CMD check results

TODO before submitting: not yet run for 0.1.1; fill in from the runs above.

## Bundled library

The package bundles the source of 'libSBML' 5.21.0 (LGPL 2.1) as
`src/libsbml-mod-5.21.0.tar.gz`, an official release with the language
bindings, documentation, tests and unused SBML Level 3 packages removed; the
files kept are unchanged upstream sources. `configure` builds it as a static
library with cmake, so installing needs no network access. Before building, it
patches the 'libSBML' sources to remove `rand`/`srand`, writes to
stdout/stderr and `sprintf`, as Writing R Extensions requires, and to include
`<new>` where `nothrow` is used. Its authors and
copyright holders are listed in `Authors@R` and in `inst/COPYRIGHTS`.

'libSBML' is built from source rather than linked from the system because it
is not packaged for most platforms, and where it is the version varies.
Building it takes a few minutes, which accounts for most of the install time.
