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
  algebraic rule.
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
models turn up — no example has one.

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
- `root(n, x)`, `piecewise(...)`, `exponentiale`, `factorial` in C++, and the
  `%` modulo operator, whose spelling differs across R, C++ and MATLAB.
- The MATLAB `!` -> `~` and `!=` -> `~=` rewrite in `formulaToInfixMatlab()`
  is reasoned-correct but unexercised: no example model contains a logical
  operator.
- `ubiquityUnsupported()` lists the constructs named in the ubiquity
  documentation. Anything else — the trigonometric functions in particular —
  falls into the generic named-call path and is reported by name rather than
  being in the explicit list.

## Found by running the SBML Test Suite

Found 2026-10-17 by converting the semantic cases of the SBML Test Suite
(https://github.com/sbmlteam/sbml-test-suite, `cases/semantic/`) to deSolve
with `convertReactions()`, simulating them with `ode()` (or `daspk()` for a
DAE) and comparing against each case's `results.csv` within the tolerances in
its `settings.txt`. Cases using events, `delay`, `rateOf`, fast reactions,
`stoichiometryMath` or any SBML package were skipped, leaving 766; 494 passed.
Making rate-rule parameters into states (they were frozen at their initial
value) raised that to 526, with no case that passed before failing after.
The numbers below are suite case ids, so each item can be reproduced from the
case directory of the same name.

### `nan` written as a value

86 cases fail to source with `object 'nan' not found`. Two causes:

- A compartment with no `size` (40 cases, e.g. 00048) is emitted as
  `compartment = nan`. `speciesInitialValue()` already treats an unset size as
  a volume of 1, but the compartment constant itself is written straight from
  `getVolume()`.
- A parameter with no `value` that an assignment rule sets (e.g. 00147,
  00181) is written into the parameter list as `nan`, even though the RHS then
  recomputes it. It should not be in the parameter list at all, any more than
  an assignment-rule species is in the state vector.

### Assignment-rule species are not observable

An assignment-rule species is computed as a local inside the RHS and never
returned, so its trajectory cannot be read from the solution (78 cases, e.g.
00038). deSolve accepts extra outputs as further elements of the list
`massBalances` returns, and the other targets have equivalents.

### A model with no integrated species

When every species is set by an assignment rule, or there are none,
`InitialAmounts` is empty and `ode()` fails with "`y' must be numeric" (43
cases, e.g. 00029). A model with parameters only makes the conversion itself
fail (00950).

### Initial values ignore initial assignments to the compartment

Species starting values are converted through the compartment size the file
declares, not the size an initial assignment or a constant assignment rule
gives it (about 10 cases, e.g. 00143, 00471).

### Rule-assigned stoichiometry is ignored

A `speciesReference` whose stoichiometry is set by a rule keeps its declared
value (4 cases, e.g. 00971).

### Conversion factors

Three model-level conversion-factor cases (00975 to 00977) integrate to the
wrong values. Not yet diagnosed.

### `ceil`

The Level 3 writer spells the ceiling function `ceil`, which R does not have;
R's is `ceiling` (3 cases, e.g. 00028). The other targets need checking too.
`piecewise`, listed above, accounts for another 18 failures.

### Running the suite as a test

The harness that produced all of the above is not in the repo. A curated set
of cases (each is three small files: the `-sbml-l3v2.xml`, `-results.csv` and
`-settings.txt`) could ship under `tests/testthat/`, or a `skip_on_cran()` test
or CI job could fetch the suite and fail on a drop in the pass count. The
whole semantic set is about 10.8MB, too large to ship. The suite is LGPL 2.1,
so shipping cases needs an entry in `inst/COPYRIGHTS`.

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
