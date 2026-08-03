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

### Vignettes

Two, in `vignettes/`: `r2sbml.Rmd` (how to use the package) and `limitations.Rmd` (what it does not do). Both evaluate their code live, so every output shown is generated at build time and cannot drift from the implementation — that is deliberate, and the reason to prefer a live chunk over a pasted transcript when adding to them.

Chunks needing deSolve are gated on `has_desolve <- requireNamespace("deSolve", quietly = TRUE)` via `eval = has_desolve`, since deSolve is only a `Suggests`. Verify that path by rendering with the user library redirected away from it — `R_LIBS_USER=<lib-without-deSolve>`, not `R_LIBS`, which prepends rather than replaces.

`R CMD build` installs the package before building vignettes, so a vignette build recompiles libsbml from scratch and takes several minutes.

### pkgdown site

`_pkgdown.yml` configures the site; `.github/workflows/pkgdown.yaml` builds it on every push to master/main and deploys `docs/` to the `gh-pages` branch. `docs/` is gitignored and `.Rbuildignore`d — it is a build artefact, rebuilt by CI, never committed. Rebuild locally with `pkgdown::build_site()`, which needs the same cmake/libxml2/libbz2 toolchain as an install.

The reference index in `_pkgdown.yml` lists functions in explicit groups. **Every exported function must appear in exactly one group** or pkgdown errors on an unindexed topic, so adding an `[[Rcpp::export]]` means adding it there too, after `compileAttributes()` → `document()`.

One trap worth knowing: **pkgdown publishes every root-level `*.md` as a site page.** `package_mds()` globs the package root and its exclusion list is hardcoded to README/LICENCE/NEWS/the issue templates/cran-comments — there is no config key for anything else. So `CLAUDE.md` gets rendered to `docs/CLAUDE.html` and indexed in `search.json` (one entry per heading, 17 of them) and `sitemap.xml`, unlinked from the navbar but findable through site search. `tools/pkgdown_clean.R` deletes it and scrubs both indexes; the workflow runs it between the build and the deploy. Keep that step if you change the workflow. `todo.md` is deliberately left published — it is a real known-issues list and the limitations vignette points readers at it.

The `path` values in `search.json` are absolute URLs when `url:` is set in `_pkgdown.yml` and root-relative when it is not, and some entries carry no `path` at all — hence the suffix match and the guard in the clean script.

## Architecture

### Two-phase API

1. **`getModel(filename)`** — reads an SBML file, keeps the `SBMLDocument` alive, and returns an R `externalptr` (`Rcpp::XPtr<Model>`).
2. **Query functions** — every other function takes that pointer, does `Model* model = Rcpp::XPtr<Model>(input_model)`, and either returns an R object or prints.

The two return styles matter when writing tests:

- **Return data**: `getSpeciesTable`, `getParameterTable`, `getReactionTable` (`DataFrame`); `getSpeciesNames`, `getCmtNames` (`StringVector`); `getSpeciesIC`, `getCmtSizes` (`NumericVector`); `getNumSpecies` (`int`).
- **Print to `Rcout`, return `0`**: `getRuleMath`, `getReactionMath`, `getFunctionDefinition`, `getEventMath`, `printSBML`, `echoSBML`. Their output must be captured (`capture.output`), not compared to a return value.

Query functions `Rcpp::stop()` on an empty model *or an empty component list* — e.g. `getRuleMath` on a model with no rules errors rather than returning empty. Any test looping over `inst/examples/` must account for that.

The condition message names the component (`"No Rules present in the model."`), so `expect_error()` can match on it and a caller can tell the cases apart. These used to print the explanation to `Rcout` and raise the bare message `"Stopping!"`; do not reintroduce that split — put the text in the `stop()` call, not a preceding `Rcout <<`.

**Severity handling is shared between `getModel()` and `convertReactions()`** and must stay that way: both refuse a file only for error- *and* fatal-severity diagnostics, and `getModel()` downgrades anything lesser to an R warning rather than refusing the file. The subtlety is that `getNumErrors(severity)` counts that severity **exactly**, not cumulatively, so `getNumErrors(LIBSBML_SEV_ERROR)` alone silently ignores `LIBSBML_SEV_FATAL` — both call sites add the two. In practice libSBML's reader reports even unparseable input at error severity, so the fatal count is usually 0; that is why the omission never showed up.

### C++ layer (`src/`)

One `.cpp` file per topic, each function exported with `// [[Rcpp::export]]`:
`getModel.cpp`, `speciesInfo.cpp`, `paramInfo.cpp`, `compartmentInfo.cpp`, `reactionInfo.cpp`, `getMath.cpp` (rules/reactions/function definitions/events), `getSpeciesNum.cpp`, `printSBML.cpp`, `echoSBML.cpp`, `convertReactions.cpp`.

### libsbml is built from source, not linked system-wide

This is the part most likely to surprise you. `configure.ac` unconditionally sets `need_to_build=yes` and calls `tools/cmake_call.sh`, which:

1. unpacks the bundled `src/libsbml-mod-5.21.0.tar.gz` into `src/libsbml-src` (`src/scripts/libsbml_download.sh` — the name is historical; it no longer downloads anything, so installing needs no network),
2. applies a block of **CRAN compliance patches** with `perl -pi -e` — removing `srand`/`rand`, `cout`/`fprintf` to stderr, and rewriting `sprintf`→`snprintf` in libsbml sources (Writing R Extensions §1.6.4 forbids these entry points),
3. cmake-builds a static `libsbml-static.a` into `src/libsbml-install/` (the only libsbml artefact that outlives the build; it is gitignored and `.Rbuildignore`d, so every clone and every tarball rebuilds it from scratch),
4. normalizes CRLF and missing trailing newlines in the installed headers,
5. deletes `libsbml-src`/`libsbml-build` via an `EXIT` trap so `R CMD check` never sees them.

Consequences: the perl patches match **exact source lines of libsbml 5.21.0**, so a version bump is a three-part change — regenerate the archive with `tools/strip_libsbml_tarball.sh <version>`, move it to `src/`, and update `LIBSBML_VERSION` in `src/scripts/libsbml_download.sh` — and then re-check every patch, because one that no longer matches fails silently and reintroduces CRAN check failures. `configure` is generated from `configure.ac` by autoconf; edit the `.ac`. `src/Makevars` is generated from `src/Makevars.in` and contains absolute paths, so it is machine-specific and not committed.

### The bundled libsbml archive

`src/libsbml-mod-5.21.0.tar.gz` (1.6MB) is an official libsbml release with whole directories removed; the files it keeps are byte-for-byte upstream. `tools/strip_libsbml_tarball.sh` regenerates it and is the single place recording what is dropped: language bindings, docs, examples, `dev/`, the autotools build system, every `test/` directory, and the SBML Level 3 package sources other than `l3v2extendedmath`.

Three things there are load-bearing and easy to break by pruning more:

- `src/bindings/` must survive, because `src/CMakeLists.txt` calls `add_subdirectory(bindings)` unconditionally. Only its `CMakeLists.txt` is kept; every language inside is behind a `WITH_<LANG>` guard that is off.
- `src/sbml/packages/*-register.{h,cxx}` are kept even for deleted packages, because `src/CMakeLists.txt` `file(GLOB)`s them into a generated header. Their bodies are `#ifdef USE_<PKG>`-guarded, so the deleted sources are never referenced.
- `COPYING.txt`, `FUNDING.txt`, `LICENSE.txt`, `NEWS.txt`, `README.md` and `VERSION.txt` are libsbml's `DOCUMENTATION_FILES`; `cmake --install` fails outright if any one is missing.

Unlike everything else under `src/libsbml-*`, this archive is tracked in git — `.gitignore` ignores `*.tar.gz` and then re-includes it with `!src/libsbml-mod-*.tar.gz` — and it is deliberately **not** in `.Rbuildignore`, since shipping it is the whole point.

### Compiler flags and artefact size

`src/scripts/r_config.sh` exports R's compiler and flags for the cmake build. Use the plain `CC`/`CXX`/`CXXFLAGS` config variables: the `CXX11*` family was deprecated in R 4.3 and is **defunct** in R 4.6, where it returns nothing and silently leaves the flags empty. `R CMD config CXX` returns compiler *and* standard flag together (`g++ -std=gnu++20`), so the script splits the first word off for cmake and carries the rest in `CXXFLAGS`.

`tools/cmake_call.sh` deliberately removes `-g` from the flags it hands to libsbml (`strip_g_flag`). Debug info for a vendored library nobody debugs from R dominates everything: `libsbml-static.a` is ~154MB with it and ~13.5MB without, and it follows the linker into `r2sbml.so`. r2sbml's own sources still get R's flags verbatim. `src/Makevars.in` then runs `strip --strip-debug` on the built shared object via a `strippedLib` target, taking the installed `.so` from ~65MB to ~6.6MB.

There is nothing to gain by disabling libsbml docs/tests/examples: `WITH_EXAMPLES`, `WITH_CHECK`, `WITH_DOXYGEN`, every language binding, and `WITH_STABLE_PACKAGES`/`WITH_ALL_PACKAGES` all default to `OFF` in libsbml's `CMakeLists.txt`, and `BUILD_TESTING=OFF` is passed explicitly. The install tree's `share/` is 232KB of licence and README text.

Windows uses `configure.win` (writes `src/Makevars.win` directly, no autoconf) and links against the Rtools libxml2/bz2/lzma/iconv.

### Example SBML files (`inst/examples/`)

Ten `.xml` files covering simple reactions, assignment rules, algebraic rules, function definitions, delay, events, multi-compartment, boundary conditions, and conversion factors. Both test files iterate over them.

`inst/examples` **does** ship: no `.Rbuildignore` pattern matches it (`^inst/.DS_S*` does not), and `tar tzf` on a built tarball lists all ten. An earlier version of this file claimed the opposite. The tests' fallback from `system.file("examples", ..., package = "r2sbml")` to `../../inst/examples/` is therefore belt-and-braces for running against an uninstalled source tree, not a necessity — but keep both paths in any new test, since that is how the existing ones are written. The vignettes rely on `system.file()` alone, which is only safe *because* the examples ship.

## `convertReactions`

The one function that does not take a model pointer. `convertReactions(infile, outfile, format = "R")` reads the SBML file itself and writes ready-to-simulate code, returning `NULL` invisibly (tests assert this with `expect_invisible`). Formats: `"R"`/`"deSolve"`, `"mrgsolve"`, `"nlmixr2"`/`"rxode2"`, `"MATLAB"`, `"Julia"` and `"ubiquity"` (the last three also accept the other case). An unrecognised format raises an error — it used to leave an empty file behind, which looked like success.

It applies four libsbml converters in sequence before emitting anything — `replaceReactions`, `promoteLocalParameters`, `expandInitialAssignments`, `expandFunctionDefinitions` — so the model reaching the writers is always rate-rule form with no local parameters or function definitions left. That is why the writers only ever walk rules and parameters. Each format has its own `writeFile*` helper (`writeFileR`, `writeFileMrgsolve`, `writeFileNlmixr2`, `writeFileMatlab`, `writeFileJulia`, `writeFileUbiquity`), forward-declared at the top of the file.

Use `speciesInitialValue()` rather than `Species::getInitialAmount()` for a starting value. It handles two separate traps.

First, a species carries `initialAmount` *or* `initialConcentration`, never both, and the unset one reads back as NaN — which `operator<<` prints as the literal `nan`, a number in none of the six target languages. Six of the ten example models are concentration-based, so this affected most output.

Second, and less visible: **the attribute that is set is not necessarily in the units the ODE integrates.** Unless `hasOnlySubstanceUnits` is true, an SBML species symbol denotes a concentration, and `replaceReactions` divides the rate rule by the compartment volume to match — you can see the `/ comp` in any generated RHS. So the state is a concentration, and an `initialAmount` has to be divided by the volume before it can be used as the initial condition. `sbmlsimple.xml` sets `initialAmount = 5e-21` in a `1e-14` compartment: the right starting value is `5e-07`, a factor of 1e14 away. The conversion runs the other way too — a `hasOnlySubstanceUnits` species given an `initialConcentration` needs multiplying by the volume, and libSBML correctly leaves *its* rate rule undivided.

The volume comes from the species' own compartment, so a multi-compartment model rescales per species: in `sbmlmutlicompartment.xml` only `Y2` (cytoplasm, size 5) moves, while `Y1n` (nucleus, size 1) does not. Compartments with `spatialDimensions="0"`, an unset size, or a size of 0 convert through a volume of 1 — there is nothing meaningful to divide by, and 0 would only produce an inf.

### Compartments whose volume is not constant

`replaceReactions` leaves out **two** things when a compartment varies, and both had to be put back:

- **Its rate rule never reaches the writers.** `integratedSpecies()` walks species, and `rateRuleFor()` was only ever asked about species ids, so a `rateRule` on a compartment was silently discarded and the volume emitted as a fixed number. `integratedCompartments()` now collects them and they join the state vector *after* the species, which keeps every existing state index — and so the MATLAB/Julia mass-matrix rows — where it was.
- **The species rate rules carry only the reaction term.** Since `[S] = n/V`, the true derivative is `d[S]/dt = (dn/dt)/V - [S]*(dV/dt)/V`, and libSBML supplies only the first half. `speciesDerivative()` appends the dilution term. Verify this on a model with no reactions at all: the concentration must still fall as the volume grows, so a species with *no* rate rule can still have a non-zero derivative — which is why the writers test `rule >= 0 || diluted` rather than `rule >= 0`.

`speciesDerivative()` takes the target's formula writer as a function pointer, since composing `(reaction) - S * (dV/dt) / V` is the only target-specific part and all six spell `+ - * /` infix — including ubiquity, whose prefix `SIMINT_*` spellings are confined to powers and transcendentals.

Dilution is only derivable when the compartment has a **rate** rule, because `dV/dt` is then given outright. An **assignment**-rule compartment gets the right volume — the existing assignment-rule loop is variable-agnostic and already emits it — but its dilution would need a symbolic time derivative of the rule, so `warnAssignmentRuleCompartment()` warns instead of emitting a plausible wrong answer. `constant` and `hasOnlySubstanceUnits` species are never diluted: the first does not move, the second integrates an amount.

None of the ten example models has a non-constant compartment, so the tests build their own (`growing_volume_model()` in `test-convert.R`) and check `[A] = 2/(1+t)` against the closed form.

This class of bug is silent: the generated code runs, and simply integrates from the wrong place. The regression test therefore checks a purpose-built decay model against its closed-form solution (`decay_model()` in `test-convert.R`) rather than against a golden output file.

### What the state vector is: `integratedSpecies()`

**Every** writer decides what it integrates through `integratedSpecies()`, and none of them walks the rule list to do it. The three rules it encodes:

- A species that is the variable of an **assignment rule** is not integrated. Its value follows from the rule at every time point, so it is emitted as a local inside the RHS and given no slot. Handing it one leaves the solver carrying a copy that never updates while the rule recomputes a different value beside it.
- A species with **no rate rule** — a boundary condition, or one in no reaction — keeps its slot with a zero derivative, so it still appears in the solution.
- A species an **algebraic rule** fixes keeps its slot too, but the row becomes a constraint instead of a derivative. See below.

Iterating over rules instead is the bug this avoids, and `writeFileR`, `writeFileMrgsolve` and `writeFileNlmixr2` all had it: they emitted one derivative per *rule* while listing states per *species*. On `sbmlmutlicompartment.xml` (4 species, 3 rate rules) the generated R referenced an undefined `dX_dt`; on `sbmlalgebraicrules.xml` an algebraic rule has no variable at all, so the same loop produced `d_dt = ...`. Both files are now covered by tests that evaluate the generated RHS, so a regression fails the suite rather than the user's solver.

### Algebraic rules become a DAE

An algebraic rule is a constraint `0 = f(...)`, not a derivative, so a model carrying one is a DAE. deSolve, MATLAB and Julia can each solve one; mrgsolve, rxode2 and ubiquity integrate ODEs only and emit the rule as a comment plus an R warning — the warning matters because the alternative is a solver that runs and quietly returns a trajectory ignoring the constraint.

Which states are constrained is the awkward part. **SBML does not record which variable a given algebraic rule determines, and libSBML exposes no matching for it.** What `algebraicStates()` can work out is the candidate set: a species that is not constant and has neither a rate rule nor an assignment rule is undetermined, and the specification requires the algebraic rules to determine exactly those. `daeIsSquare()` then insists the two counts agree — one rule per undetermined state — and anything else falls back to comments and a warning rather than guessing. On `sbmlalgebraicrules.xml` that gives `E` and `ES` (both `boundaryCondition="true"`, `constant="false"`) against two rules; `E_total` is `constant="true"` and so is not a candidate.

Which rule lands in which constrained row does **not** matter. The residuals are one simultaneous system, so permuting its components leaves the solution unchanged — only the Jacobian's sparsity shifts. What does matter is *which states* get a constraint row, and that is what the candidate set decides.

The three DAE targets spell it differently:

- **deSolve** switches shape entirely: `massBalances` becomes a residual function of `(time, states, derivs, params)` for `daspk`, not a derivative function of `(time, states, params)` for `ode`. Integrated rows read `derivs[["S"]] - (f)`, constrained rows the rule itself. `daspk` also needs y' at t=0 consistent with the residuals, so the writer emits `InitialDerivatives` by evaluating the same rate-rule expressions inside `with(as.list(c(InitialAmounts, parameters)), ...)` — compartment volumes are file-level globals and resolve through `with`'s enclosing frame.
- **MATLAB and Julia** keep the derivative-function shape and use a singular mass matrix instead: `M*y' = f`, with a zero row wherever a constraint sits, so that row reads `0 = f_i`. MATLAB needs `odeset('Mass', M, 'MassSingular', 'yes')`; Julia needs `ODEFunction(...; mass_matrix = M)` *and* a solver that accepts one, so the header says `solve(prob, Rodas5())` rather than the bare `solve(prob)` — the default solver choice will not take a mass matrix.

`sbmlalgebraicrules.xml` is a quasi-steady-state enzyme model, and it has a property worth keeping in the test: `S + P` is conserved *only* when `k1_on*E*S == (k1_off + k2)*ES` holds, because that is exactly what makes `dS/dt + dP/dt` cancel. So checking `S + P == 1` along the trajectory tests that the constraint is genuinely being enforced, not merely written down. Note that `S + ES + P` is *not* conserved here — `E` and `ES` are boundary species, exempt from reaction-driven mass balance.

### The MATLAB and Julia writers

Two smaller target constraints apply only to these two, which index into the state vector rather than naming derivatives.

MATLAB resolves a function by file name, so `matlabFunctionName()` derives the function's name from the output path — renaming a generated `.m` file breaks it. And SBML's csymbol for time serialises as `time`, which the MATLAB RHS gets for free by naming its argument `time`, while the Julia RHS receives `t` and needs the `time = t` alias the writer emits.

### The ubiquity writer

`writeFileUbiquity` emits a ubiquity system file: `<P>` for parameters *and* compartment volumes (ubiquity has no compartment concept, so a volume is just a constant the rate expressions divide by), `<I>` for initial conditions, `<Ad>` for assignment rules — dynamic rather than static secondary parameters, since they read states — `<ODE:name>` for the mass balances, and `<O>` for outputs. It shares `integratedSpecies()` with the MATLAB and Julia writers.

Bounds are written `-inf inf`, not the `eps inf` you see in hand-written ubiquity systems: an SBML value may legitimately be zero or negative, and a lower bound above the value is inconsistent. They only matter for estimation.

ubiquity is the one target that **cannot** reuse the Level 3 writer. It spells exponentiation, the transcendental functions and every comparison as bracketed prefix calls — `SIMINT_POWER[a][b]`, `SIMINT_LOGN[a]`, `SIMINT_GT[a][b]` — so there is no infix text to patch up, and delegating a subtree to Level 3 does not compose, because a power nested anywhere inside comes back out as `a^b`. `formulaToUbiquity()` is therefore a full recursive walk over the AST. It parenthesises every arithmetic operand rather than tracking precedence: verbose output, but it cannot get the grouping wrong.

Constructs with no ubiquity spelling (`root`, `piecewise`, `factorial`, `delay`, `or`, `not`, `xor`) pass through as plain calls and are collected by `ubiquityUnsupported()`. The writer then puts a `# WARNING` block at the top of the file *and* raises an R warning. This matters because `build_system()` accepts such a file and the failure only surfaces later as a C compile error naming a shared object, with nothing pointing back at the model.

`convertReactions` itself never opens `outputFile`; each `writeFile*` helper opens the path with `std::ofstream` and owns the whole file. It used to write a header line first, which every helper then truncated away.

### Serialising math: always use `src/formulaToInfix.h`

Never call `SBML_formulaToString()` or `Rule::getFormula()` — both emit the SBML **Level 1** infix syntax, which has no relational or logical operators. It writes `b > 1` as `gt(b, 1)`, `a && b` as `and(a, b)`, and `a^b` as `pow(a, b)`. None of `gt`, `and` or `pow` is a function in R or rxode2, so anything serialised that way emitted code that could not run.

`r2sbml::formulaToInfix()` uses the Level 3 writer instead. Two things about it are non-obvious:

- It appends unit annotations to literal numbers — `3 mole` serialises as `3 mole`, not `3` — which is right for round-tripping SBML and wrong for generating code. The helper suppresses this with `setParseUnits(L3P_NO_UNITS)`. Do not call `SBML_formulaToL3String()` directly; it has no settings argument and will leak units into the output.
- It writes exponentiation as `a^b`, which suits R and rxode2 but not mrgsolve, whose model blocks are C++ where `^` is bitwise XOR and does not compile for doubles.

`r2sbml::formulaToInfixC()` is the mrgsolve variant: it deep-copies the AST, retypes every power node, and serialises that. The retype targets `AST_FUNCTION` (a plain named call) rather than `AST_FUNCTION_POWER`, because the Level 3 writer spells *both* `AST_POWER` and `AST_FUNCTION_POWER` as `a^b` — only a named call comes back as `pow(a, b)`. The rest of the Level 3 syntax is an improvement for mrgsolve too, since `b > 1` and `a && b` are valid C++ where the Level 1 spellings were not.

Unrelated to the serialiser: SBML's `delay` csymbol still passes through as `delay(...)`, which is not a function in base R. `sbmldelay.xml` is the one example whose generated deSolve/rxode2 code will not run; deSolve would need `lagvalue()`.

Do not namespace-qualify libsbml types here (`libsbml::SBMLWriter`). libsbml's `WITH_CPP_NAMESPACE` defaults to `OFF` and the build does not enable it, so no `libsbml` namespace exists — `LIBSBML_CPP_NAMESPACE_USE` expands to nothing and qualified names fail to compile. A previous revert of this file to the stock libSBML sample broke the build exactly this way.
