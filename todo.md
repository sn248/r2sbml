# todo

Known gaps and follow-up work. Each item notes how it was found, so it
can be confirmed before being acted on.

## Unsupported SBML constructs

### Algebraic rules in the three ODE-only targets

deSolve, MATLAB and Julia now emit a DAE and enforce the constraint.
mrgsolve, rxode2 and ubiquity integrate ODEs only, so they still write
the rule out as a comment, leave the species it constrains at a zero
derivative, and warn. There is no fix short of solving the constraint
symbolically for its variable and emitting that as an assignment, which
only works for the cases where such a solution exists and is unique.

Two related gaps in what *is* implemented:

- The DAE is only emitted when the number of algebraic rules equals the
  number of undetermined states (`daeIsSquare()`). SBML records no
  matching between rules and the variables they determine, and libSBML
  exposes none, so a model where the counts differ falls back to
  comments and a warning. A real bipartite matching over the variables
  each rule references would widen this.
- Consistent initial conditions are assumed, not checked.
  `inst/examples/sbmlalgebraicrules.xml` happens to start on the
  constraint manifold. A model that does not will have `daspk` fail at t
  = 0, and `ode15s` reject the initial state; neither error mentions the
  SBML model. `daspk`’s `estini` can estimate them, but it requires the
  algebraic equations to come last in the residual vector, which the
  current row order does not guarantee.

### Dilution for an assignment-rule compartment

A compartment with a *rate* rule is now integrated as a state and the
species inside it get the dilution term `-[S]*(dV/dt)/V`. A compartment
with an *assignment* rule gets the right volume but no dilution: forming
the term needs dV/dt, i.e. a symbolic time derivative of the assignment
expression, chain rule and all. All six writers warn in that case.

Doing it properly means differentiating an AST with respect to time and
the states it reads. That is a real piece of work, and worth it only if
such models turn up — no example has one.

### `delay`

SBML’s `delay` csymbol passes through unchanged, so
`inst/examples/sbmldelay.xml` generates code that runs in no target.
deSolve would need `lagvalue()`, and the equivalent per target. Only the
ubiquity writer currently warns about it; the others emit it silently.

### Events

Not exported at all — they appear only as a count in the generated
header comment.
[`getEventMath()`](https://sn248.github.io/r2sbml/reference/getEventMath.md)
can inspect them from R. `inst/examples/sbmlevent.xml` has two.

### Miscellaneous maths with no target spelling

None of these is exercised by the example models, so all are untested:

- Two-argument `log(b, x)`. The Level 3 writer emits `log(b, x)`, which
  is correct in Julia but reversed in R (`log(x, base)`) and invalid in
  MATLAB. `formulaToUbiquity()` already rewrites it as a ratio of
  `SIMINT_LOGN`.
- `root(n, x)`, `piecewise(...)`, `exponentiale`, `factorial` in C++,
  and the `%` modulo operator, whose spelling differs across R, C++ and
  MATLAB.
- The MATLAB `!` -\> `~` and `!=` -\> `~=` rewrite in
  `formulaToInfixMatlab()` is reasoned-correct but unexercised: no
  example model contains a logical operator.
- `ubiquityUnsupported()` lists the constructs named in the ubiquity
  documentation. Anything else — the trigonometric functions in
  particular — falls into the generic named-call path and is reported by
  name rather than being in the explicit list.

## Query-function quirks

- **Empty-component errors carry no useful message.** The explanatory
  line goes to `Rcout` and the condition message is only `"Stopping!"`,
  so a caller cannot tell “no rules” from “no parameters” without
  capturing stdout.
- **[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
  and
  [`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
  disagree on what counts as fatal.**
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
  stops when `getNumErrors()` is non-zero — every diagnostic, whatever
  its severity — while
  [`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
  stops only on `getNumErrors(LIBSBML_SEV_ERROR)`. After a bare
  `readSBMLFromFile()` the two usually coincide, since the reader mostly
  emits severity-error diagnostics, so no example distinguishes them. A
  file that read with only a warning would be rejected by
  [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
  and accepted by
  [`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md).

## Not action items, recorded so they are not rediscovered

- **libSBML 5.21.1** exists but is tagged prerelease and its only change
  is a fix to the Python bindings in the pip package. It touches none of
  the eight files patched by `tools/cmake_call.sh`, so upgrading is safe
  but pointless. Bumping the version means regenerating the bundled
  archive with `tools/strip_libsbml_tarball.sh` *and* re-checking every
  patch, since one that no longer matches fails silently.
- **`R CMD check --as-cran` reports 2 NOTEs**, both benign: “New
  submission”, and a non-portable `-mno-omit-leaf-frame-pointer` flag
  that comes from Debian/Ubuntu R’s own default `CFLAGS` rather than
  from this package. The second does not appear on the CI machines.
- **A local check may add up to 4 more findings that are all
  environmental**, not package problems. A missing `inconsolata.sty`
  (Debian/Ubuntu: `texlive-fonts-extra`) fails the PDF manual, which
  produces one WARNING, one ERROR (“PDF version of manual without
  index”) *and* a NOTE about the `r2sbml-manual.tex` it leaves behind; a
  missing `tidy` binary produces the HTML-manual NOTE. Install those two
  or ignore the four findings. The vignette checks —
  `files in 'vignettes'`, `unstated dependencies in vignettes`,
  `package vignettes`, `re-building of vignette outputs` — all pass.
