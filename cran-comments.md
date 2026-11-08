## Submission

This update responds to the check issues on 0.1.0, which CRAN asked to have
fixed before 2026-11-24. That is why it follows 0.1.0 so closely.

It fixes the installation failure on r-devel-linux-x86_64-fedora-clang and
clang-ASAN. The bundled 'libSBML' sources use `new(nothrow)` without including
`<new>`, which libc++ 23 no longer provides through other headers. `configure`
now adds the include before building, and stops with an error if the 'libSBML'
build fails.

It also changes the code behind the test failures on the four macOS flavours,
though I could not reproduce them, so this fix is not confirmed. When a rule
set a species reference's stoichiometry, `convertReactions()` stopped with
"Conversion failed": the reaction was removed, but the parameter 'libSBML'
should have created for that stoichiometry was missing. The same tests passed
before this change under Apple clang 17 and 21 on GitHub Actions and on M1-SAN,
so I do not know the root cause. r2sbml now creates the parameter itself instead
of relying on that 'libSBML' code path.
TODO before submitting: add the mac-builder result, or say it was unavailable
(it was failing on 2026-11-06).

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

The first submission of 0.1.1 failed to install on the clang-san additional
check: `r2sbml.so` could not be loaded because of the undefined symbol
`unzGoToFirstFile`. That symbol comes from the minizip code bundled in
'libSBML' for reading compressed SBML files, which r2sbml does not use.
'libSBML' is now built without zlib and bzip2 support, so that code is no
longer compiled or linked, and libbz2 is no longer a system requirement. I
could not run CRAN's clang-san setup itself; R-hub's clang-asan container
(clang 23.1.6, libc++, ASan and UBSan) installs, loads and checks the package
with Status: OK and no sanitizer reports.

## Test environments

* Debian testing, R 4.6.1, clang 23.1.6 with libc++ (install and tests)
* Debian testing, R 4.6.1, GCC 16.2 (install and tests)
* Local Ubuntu 26.04, R 4.6.1, GCC 15.2 (`R CMD check --as-cran`)
* GitHub Actions (`R CMD check --as-cran`):
  * Ubuntu 24.04: R-devel (4.7.0), R-release (4.6.2), R-oldrel (4.5.3)
  * macOS 26 with Apple clang 21, R-release (4.6.2)
  * macOS 15 with Apple clang 17, R-release (4.6.2)
  * Windows Server 2022, R-release (4.6.2)
* R-hub clang-asan: clang 23.1.6 with libc++, AddressSanitizer and
  UndefinedBehaviorSanitizer (`R CMD check --as-cran`, including tests and
  vignettes)
* win-builder, R-devel
* TODO before submitting: mac-builder, or remove this line if unavailable

## R CMD check results

0 errors | 0 warnings | 3 NOTEs, on the local Ubuntu run:

* "Days since last update: 5": see the first paragraph above.
* "non-portable flag(s): -mno-omit-leaf-frame-pointer": this comes from the
  CXXFLAGS in Ubuntu's R configuration, not from the package.
* "Skipping checking HTML validation: no command 'tidy' found": HTML Tidy is
  not installed on that machine.

The GitHub Actions and R-hub clang-asan runs all gave Status: OK. The
win-builder R-devel check gave one NOTE, the same "Days since last update: 5".
TODO before submitting: confirm this against the new win-builder run.

## Bundled library

The package bundles the source of 'libSBML' 5.21.0 (LGPL 2.1) as
`src/libsbml-mod-5.21.0.tar.gz`, an official release with the language
bindings, documentation, tests and unused SBML Level 3 packages removed; the
files kept are unchanged upstream sources. `configure` builds it as a static
library with cmake, so installing needs no network access. Before building, it
patches the 'libSBML' sources to remove writes to stdout/stderr and
`sprintf`, as Writing R Extensions requires, and to include
`<new>` where `nothrow` is used and to fix the casts listed above. Its authors and
copyright holders are listed in `Authors@R` and in `inst/COPYRIGHTS`.

'libSBML' is built from source rather than linked from the system because it
is not packaged for most platforms, and where it is the version varies.
Building it takes a few minutes, which accounts for most of the install time.
