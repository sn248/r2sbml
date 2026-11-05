## Submission

This update fixes the installation failure on
r-devel-linux-x86_64-fedora-clang. The bundled 'libSBML' sources use
`new(nothrow)` without including `<new>`, which libc++ 23 no longer provides
through other headers. `configure` now adds the include before building, and
stops with an error if the 'libSBML' build fails.

It also addresses the additional-check reports from gcc-UBSAN and M1-SAN:

* Fixed: the out-of-range cast in r2sbml's own `formulaToInfix.h`, and in the
  bundled 'libSBML' sources the `(int)(NAN)` casts in `ExpressionAnalyser.cpp`
  and the invalid casts in `Reaction.cpp` and `LocalParameter.cpp`.
* Not fixed, and suppressed: `Validator.cpp:340`, `Validator.cpp:93` and
  `ConsistencyConstraints.cpp:1830`. 'libSBML' deliberately applies its
  `SpeciesReference` constraints to `ModifierSpeciesReference` objects by
  casting between these sibling classes, which is undefined behaviour. Fixing
  it would mean rewriting those constraints upstream. `configure` compiles the
  sources under `sbml/validator/` with `-fno-sanitize=vptr`, so the sanitizer
  no longer reports it, but the behaviour itself is unchanged. No other
  sanitizer check is disabled, and AddressSanitizer reported no errors from
  the test suite.

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
`<new>` where `nothrow` is used and to fix the casts listed above. Its authors and
copyright holders are listed in `Authors@R` and in `inst/COPYRIGHTS`.

'libSBML' is built from source rather than linked from the system because it
is not packaged for most platforms, and where it is the version varies.
Building it takes a few minutes, which accounts for most of the install time.
