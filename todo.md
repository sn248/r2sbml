# todo

Known gaps and follow-up work. Each item notes how it was found, so it can be
confirmed before being acted on.

## Unsupported SBML constructs

### Algebraic rules in the three ODE-only targets

deSolve, MATLAB and Julia now emit a DAE and enforce the constraint. mrgsolve,
rxode2 and ubiquity integrate ODEs only, so they still write the rule out as a
comment, leave the species it constrains at a zero derivative, and warn. There
is no fix short of solving the constraint symbolically for its variable and
emitting that as an assignment, which only works for the cases where such a
solution exists and is unique.

Two related gaps in what *is* implemented:

- The DAE is only emitted when the number of algebraic rules equals the number
  of undetermined states (`daeIsSquare()`). SBML records no matching between
  rules and the variables they determine, and libSBML exposes none, so a model
  where the counts differ falls back to comments and a warning. A real
  bipartite matching over the variables each rule references would widen this.
- Only species are candidates for what an algebraic rule determines
  (`algebraicStates()`). A rule that fixes a non-constant parameter or
  compartment is never matched, so the counts disagree and the model falls
  back to comments and a warning. SBML Test Suite cases 00182, 00184, 00573
  and 00576 are all of this shape: parameters only, with rate rules and one
  algebraic rule. Another 19 (e.g. 00533, 00539, 00547) declare the
  compartment or parameter with no size or value at all, leaving the
  algebraic rule as the only thing that defines it; those are written as
  `NaN` with a warning naming the variable (`warnUndefinedValues()`).
- Consistent initial conditions are assumed, not checked.
  `inst/examples/sbmlalgebraicrules.xml` happens to start on the constraint
  manifold. A model that does not will have `daspk` fail at t = 0, and
  `ode15s` reject the initial state; neither error mentions the SBML model.
  `daspk`'s `estini` can estimate them, but it requires the algebraic
  equations to come last in the residual vector, which the current row order
  does not guarantee.

### Dilution for an assignment-rule compartment

A compartment with a *rate* rule is now integrated as a state and the species
inside it get the dilution term `-[S]*(dV/dt)/V`. A compartment with an
*assignment* rule gets the right volume but no dilution: forming the term
needs dV/dt, i.e. a symbolic time derivative of the assignment expression,
chain rule and all. All six writers warn in that case.

Doing it properly means differentiating an AST with respect to time and the
states it reads. That is a real piece of work, and worth it only if such
models turn up — no example has one. The SBML Test Suite has 18 (e.g.
00310-00318). Three of them (00310-00312) passed until assignment-rule
variables became solution columns: the comparison then used the true volume
rather than the declared one, which had hidden the missing dilution.

The warning also fires for a rule that does not vary with time (`c = 1`),
where no dilution is needed. Silencing it means changing
`growing_volume_model(assignment = TRUE)` in `test-convert.R` to a
time-dependent rule first, since that test relies on the warning.

### `delay`

SBML's `delay` csymbol passes through unchanged, so
`inst/examples/sbmldelay.xml` generates code that runs in no target. deSolve
would need `lagvalue()`, and the equivalent per target. Only the ubiquity
writer currently warns about it; the others emit it silently.

### Events

Not exported at all — they appear only as a count in the generated header
comment. `getEventMath()` can inspect them from R.
`inst/examples/sbmlevent.xml` has two.

### Miscellaneous maths with no target spelling

None of these is exercised by the example models, so all are untested:

- Two-argument `log(b, x)`. The Level 3 writer emits `log(b, x)`, which is
  correct in Julia but reversed in R (`log(x, base)`) and invalid in MATLAB.
  `formulaToUbiquity()` already rewrites it as a ratio of `SIMINT_LOGN`.
- `root(n, x)`, `exponentiale`, `factorial` in C++, and the `%` modulo
  operator, whose spelling differs across R, C++ and MATLAB. (`piecewise`,
  `ceiling`, infinity and booleans are now spelled per target by
  `rewriteForDialect()`.)
- n-ary `xor`: R's `xor()` takes exactly two arguments (00201, 00279).
- The Level 3 Version 2 MathML functions -- `implies`, `quotient`, `rem`,
  `sec`, `arccoth` and friends, n-ary `plus` written as a call -- and the
  `avogadro` csymbol pass through unchanged and are not functions or names in
  R (about 18 suite cases, e.g. 01277, 01278, 01279, 00957, 00959, 00961).
- The MATLAB `!` -> `~` and `!=` -> `~=` rewrite in `formulaToInfixMatlab()`
  is reasoned-correct but unexercised: no example model contains a logical
  operator.
- `ubiquityUnsupported()` lists the constructs named in the ubiquity
  documentation. Anything else — the trigonometric functions in particular —
  falls into the generic named-call path and is reported by name rather than
  being in the explicit list. ubiquity does have `SIMINT_OR`, `SIMINT_NOT` and
  `SIMINT_NE`, so `or`, `not` and `!=` could be translated rather than flagged.

### Smaller target-specific gaps

- rxode2's `ini()` block rejects an infinite parameter value; no spelling
  avoids that.
- ubiquity maps a `-inf` lower bound to `.Machine$double.xmin`, the smallest
  *positive* double. Harmless for simulation; wrong for estimation.
- MATLAB: a species named `y` or `t` overwrites the generated function's own
  outputs. The parent function's other locals are prefixed `r2sbml_` for
  this reason, but `t` and `y` are its documented outputs.

## Found by running the SBML Test Suite

Found 2026-10-17 by converting the semantic cases of the SBML Test Suite
(https://github.com/sbmlteam/sbml-test-suite, `cases/semantic/`) to deSolve,
simulating them and comparing against each case's `results.csv` within its
tolerances. That run is now part of the tests: 75 passing cases ship in
`tests/testthat/sbml-test-suite/`, and the whole suite runs when
`R2SBML_SBML_TEST_SUITE` points at `cases/semantic`, and in the
`sbml-test-suite.yaml` CI job (pinned to suite commit d6dcd2c). Both fail
below `sts_pass_floor` in `test-sbml-test-suite.R`; raise it whenever a fix
passes more, and ship a case or two from that fix.

1011 cases are in scope (no events, `delay`, `rateOf`, fast reactions,
`stoichiometryMath` or SBML packages). 647 passed at 4bc2da9 and 875 after
fixing piecewise/ceiling/infinity spellings, three silently wrong conversions
(assignment-rule compartment volumes, rule-set stoichiometry, conversion
factors) and returning assignment-rule variables as outputs. The first
harness put the scope at 766 because it read an empty `packagesPresent:`
line as a package; the counts recorded with it in earlier commits (494, 526,
581) are low for that reason, though each before/after comparison stands.

The remaining 136 failures, by cause. The numbers are suite case ids, so each
can be reproduced from the case directory of the same name; run the full
suite through the helper in `tests/testthat/helper-sbml-test-suite.R` to see
the current detail for each.

- Algebraic rules (about 30): see "Algebraic rules" above. 18 of them stop
  the solver on a `NaN` written by `warnUndefinedValues()`.
- Assignment-rule compartment dilution (18): see above.
- A species reference used in math, or stoichiometry that varies in time
  (13, e.g. 00974, 01107, 01449).
- Level 3 Version 2 MathML functions and `avogadro` (about 18): see
  "Miscellaneous maths".
- A reaction id used in math as its rate (6, e.g. 01225, 01226, 01231,
  01301): `J0` is not defined in the generated code.
- `Conversion failed.` (5, e.g. 00950, 01234, 01554, 01557). 00950
  is `expandInitialAssignments` rejecting an initial assignment to
  `inf`/`-inf`/`nan`; the others are not yet diagnosed.
- Conversion factors in the remaining shapes (4, e.g. 01484, 01499-01501).
- Generated R that does not parse (about 12, "unexpected '>'" or "'='"); not
  yet diagnosed.
- About 25 numeric mismatches not yet diagnosed individually.

## Not action items, recorded so they are not rediscovered

- **libSBML 5.21.1** exists but is tagged prerelease and its only change is a
  fix to the Python bindings in the pip package. It touches none of the eight
  files patched by `tools/cmake_call.sh`, so upgrading is safe but pointless.
  Bumping the version means regenerating the bundled archive with
  `tools/strip_libsbml_tarball.sh` *and* re-checking every patch, since one
  that no longer matches fails silently.
- **`R CMD check --as-cran` reports 2 NOTEs**, both benign: "New submission",
  and a non-portable `-mno-omit-leaf-frame-pointer` flag that comes from
  Debian/Ubuntu R's own default `CFLAGS` rather than from this package. The
  second does not appear on the CI machines.
- **A local check may add up to 4 more findings that are all environmental**,
  not package problems. A missing `inconsolata.sty` (Debian/Ubuntu:
  `texlive-fonts-extra`) fails the PDF manual, which produces one WARNING, one
  ERROR ("PDF version of manual without index") *and* a NOTE about the
  `r2sbml-manual.tex` it leaves behind; a missing `tidy` binary produces the
  HTML-manual NOTE. Install those two or ignore the four findings. The vignette
  checks — `files in 'vignettes'`, `unstated dependencies in vignettes`,
  `package vignettes`, `re-building of vignette outputs` — all pass.
