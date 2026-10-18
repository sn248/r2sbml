# todo

Known gaps and follow-up work. Each item notes how it was found, so it can be
confirmed before being acted on.

## Unsupported SBML constructs

### Algebraic rules that are coupled or nonlinear

An algebraic rule that names one undetermined variable and is linear in it is
rewritten as an assignment rule (`solveLinearAlgebraicRules()`), so it works
in every target; that cleared all 36 algebraic-rule failures in the SBML Test
Suite. What is left reaches the DAE path: deSolve, MATLAB and Julia enforce
the constraint, while mrgsolve, rxode2 and ubiquity write the rule out as a
comment, leave the species at a zero derivative, and warn. Open items there:

- The DAE is only emitted when the number of algebraic rules equals the number
  of undetermined states (`daeIsSquare()`). A real bipartite matching over the
  variables each rule references would widen this.
- Only species are DAE candidates (`algebraicStates()`); a coupled rule fixing
  a parameter or compartment still falls back to comments and a warning.
- Consistent initial conditions are assumed, not checked.
  `inst/examples/sbmlalgebraicrules.xml` happens to start on the constraint
  manifold. A model that does not will have `daspk` fail at t = 0, and
  `ode15s` reject the initial state; neither error mentions the SBML model.
  `daspk`'s `estini` can estimate them, but it requires the algebraic
  equations to come last in the residual vector, which the current row order
  does not guarantee.
- A solved rule divides by its coefficient, so one that vanishes at run time
  (`(k3 + 1) * S1 = T` with `k3 = -1`) gives NaN.

### Dilution through an assignment-rule volume

Done through `differentiate()` (00310-00318 pass), but it falls back to a
warning when the volume rule reads a species (circular), or depends through
time or a state on piecewise, min, max, abs, the `arc*` functions or
factorial.

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

The Level 3 Version 2 MathML (sec/csc/cot and hyperbolics, every `arc*`,
`quotient`, `rem`, `implies`, `exponentiale`, `avogadro`, and
plus/times/and/or/xor with zero, one or many operands) is now rewritten by
`expandMathML()`. What is left:

- Two-argument `log(b, x)`. The Level 3 writer emits `log(b, x)`, which is
  correct in Julia but reversed in R (`log(x, base)`) and invalid in MATLAB.
  `formulaToUbiquity()` already rewrites it as a ratio of `SIMINT_LOGN`.
- `root(n, x)`, `factorial` in C++, and the `%` modulo operator, whose
  spelling differs across R, C++ and MATLAB.
- `quotient` and `rem` follow libSBML's floor definition; no suite case has a
  negative operand, so truncation towards zero cannot be ruled out.
- 01282: libSBML's `expandInitialAssignments` evaluates `piecewise(0, 5, 1)`
  as 1, treating the numeric condition 5 as false. That is libSBML's
  evaluator, not a spelling.
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
- The rxode2 list in `reservedNames()` holds only what was probed: rxode2
  exports no list of its reserved names.
- A renamed compartment shows its new name in deSolve too, since compartments
  are top-level variables there; every other deSolve name keeps the SBML id.
- An id that is not a valid name in the target at all, rather than a reserved
  one, is not renamed: 01121's promoted local parameter `_J0_k1` starts with
  an underscore, which R rejects.
- `warnUndefinedValues()` reports "No value is defined" even for a NaN the
  file assigns deliberately (00950).

## Found by running the SBML Test Suite

Found 2026-10-17 by converting the semantic cases of the SBML Test Suite
(https://github.com/sbmlteam/sbml-test-suite, `cases/semantic/`) to deSolve,
simulating them and comparing against each case's `results.csv` within its
tolerances. That run is now part of the tests: 103 passing cases ship in
`tests/testthat/sbml-test-suite/`, and the whole suite runs when
`R2SBML_SBML_TEST_SUITE` points at `cases/semantic`, and in the
`sbml-test-suite.yaml` CI job (pinned to suite commit d6dcd2c). Both fail
below `sts_pass_floor` in `test-sbml-test-suite.R`; raise it whenever a fix
passes more, and ship a case or two from that fix.

1011 cases are in scope (no events, `delay`, `rateOf`, fast reactions,
`stoichiometryMath` or SBML packages). 647 passed at 4bc2da9 and 875 after
fixing piecewise/ceiling/infinity spellings, three silently wrong conversions
(assignment-rule compartment volumes, rule-set stoichiometry, conversion
factors) and returning assignment-rule variables as outputs, then 883 once a
reaction id used in math denoted its rate and assignment rules were sorted
into dependency order, then 962 after the Level 3 Version 2 maths, renaming
reserved ids, chained relationals, and five more replaceReactions and harness
fixes, then 1007 after solving linear algebraic rules and diluting through
assignment-rule volumes. The first
harness put the scope at 766 because it read an empty `packagesPresent:`
line as a package; the counts recorded with it in earlier commits (494, 526,
581) are low for that reason, though each before/after comparison stands.

The remaining 4 failures, each its own cause:

- 01121: an id that is not a valid R name (see "Smaller target-specific
  gaps").
- 01282: libSBML's piecewise evaluation (see "Miscellaneous maths").
- 01323: a relative error of 3.5e-08 on `avogadro`-scaled values, just outside
  the case's tolerance; not yet diagnosed.
- 01486: `object 'y' not found`; not yet diagnosed.

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
