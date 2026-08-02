# todo

Known gaps and follow-up work. Each item notes how it was found, so it can be
confirmed before being acted on.

## Correctness

### Retrofit `integratedSpecies()` to the three original writers

`writeFileMatlab`, `writeFileJulia` and `writeFileUbiquity` decide explicitly
which species form the state vector. `writeFileR`, `writeFileMrgsolve` and
`writeFileNlmixr2` do not, and get two cases wrong:

- **Species with no rate rule.** `writeFileR` emits one `d<var>_dt` per *rule*
  but builds its return vector from *species*. On `inst/examples/sbmlmutlicompartment.xml`
  (4 species, 3 rate rules) the generated R references `dX_dt`, which is never
  defined. X is a boundary species.
- **Algebraic rules.** They have no variable, so the same loop emits
  `d_dt = ...` — twice, on `inst/examples/sbmlalgebraicrules.xml` — and the
  return vector then references `dE_dt`, `dES_dt` and `dE_total_dt`, none of
  which exist.

Both files therefore generate code that does not run. The fix is the one the
newer writers already use: state = species minus assignment-rule variables,
zero derivative for species with no rate rule, algebraic rules as comments.

### Confirm the initial-value units are consistent with the rate rules

After `replaceReactions`, the rate rules divide by compartment volume
(`... / comp`), which implies the state variable is a *concentration*. But
`speciesInitialValue()` returns `initialAmount` when that is the attribute
set. For `sbmlsimple.xml`, `initialAmount = 5e-21` with `comp = 1e-14`, so the
concentration would be `5e-7` — a factor of `1e14` apart.

This is unverified, and it affects every writer equally, so it is a question
about the conversion as a whole rather than a bug in one target. Worth
settling with a model whose analytic solution is known before changing
anything: if the states really are concentrations, an amount-valued initial
condition needs dividing by the compartment volume.

## Unsupported SBML constructs

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
