# Editing a loaded model and writing it back to SBML

Scope for r2sbml's third qspfit request (`dev/qspfit-suggestions.md` §3:
"Write edited models back to SBML"). Load a model, mutate it in place
(parameter values, compartment sizes, species initial values, rate laws,
reactions removed or switched off), then serialise it to an SBML file.

Decisions taken 2026-11-27: `setSpeciesIC` requires the caller to name the
attribute; all three phases below are in scope; this note is the record.

## Why this is cheaper than it looks

- **The pointer is already mutable.** `getModel()` returns
  `XPtr<Model, …, finalizeModel, true>` over a non-const `Model*`, and
  `finalizeModel` frees the owning `SBMLDocument`. libSBML's
  `getSpecies(i)`, `getParameter(id)`, `getCompartment(id)` return non-const
  pointers off a non-const model, so edits mutate the very document the XPtr
  already owns — no new ownership machinery, and the finalizer is unchanged.
- **The writer and the Level 3 parser already ship in `libsbml-static.a`.**
  No libsbml rebuild, no new `perl -pi` patch, no archive regeneration, no
  `configure`/Makevars change — no new linked symbols.

## Layer 1 — the writer (`src/writeSBML.cpp`)

```cpp
// [[Rcpp::export]]
int writeSBML(SEXP input_model, SEXP filename);   // returns NULL invisibly
```

- `Model* model = XPtr<Model>(input_model)`; guard `model == 0` → `Rcpp::stop`.
- `SBMLDocument* doc = model->getSBMLDocument();` (set, because the model was
  read via `SBMLReader`). **Never delete it** — the XPtr finalizer owns it;
  the writer only serialises.
- **Re-validate before writing, with the shared severity rule.** CLAUDE.md:
  the error+fatal count "is shared between `getModel()` and
  `convertReactions()` and must stay that way." A write is a third call site
  and uses the same two-count idiom:
  `doc->checkConsistency(); n = getNumErrors(LIBSBML_SEV_ERROR) +
  getNumErrors(LIBSBML_SEV_FATAL)` (each severity is counted *exactly*, so
  FATAL must be added separately). `n > 0` → refuse, print diagnostics, do not
  emit an invalid file. Lesser diagnostics → R warning, file still written,
  exactly as `getModel()` downgrades them.
- `SBMLWriter().writeSBMLToFile(doc, fname)`; false → `stop` naming the path.
  Level/version are preserved (the writer serialises the doc's own L/V).
- Return `invisible(NULL)`, matching `convertReactions`; test `expect_invisible`.

## Layer 2 — the mutators (`src/editModel.cpp`)

Each takes the model pointer, mutates in place, returns it invisibly so calls
chain. Unknown id → `Rcpp::stop` naming the component, e.g.
`"No parameter 'id' in the model."`, matching the existing message style.
Empty model / empty component list already `stop` in this package.

| Function | libSBML | Notes |
|---|---|---|
| `setParameterValue(model, id, value)` | `getParameter(id)->setValue(v)` | `NA` → `unsetValue()` |
| `setCompartmentSize(model, id, value)` | `Compartment::setSize` | `NA` → `unsetSize()` (NaN convention) |
| `setSpeciesIC(model, id, value, as)` | amount **or** concentration | `as = c("amount","concentration")`, caller-specified (no guessing) |
| `setRateLaw(model, id, formula)` | parse infix → `KineticLaw::setMath` | Level 3 parser, below |
| `removeReaction(model, id)` | `model->removeReaction(id)` | returns the removed object to `delete`; heaviest |

- **`setSpeciesIC` attribute choice.** A species carries `initialAmount`
  *xor* `initialConcentration`. The caller names which via `as`; the setter
  sets that one and `unset`s the other, so the species never ends up carrying
  both. (Decided over the `hasOnlySubstanceUnits`-follows rule to avoid a
  silent guess; the rest of the package still *reads* whichever is set.)
- **Formula parsing uses Level 3**, to stay consistent with
  `formulaToInfix.h` — the whole package standardises on L3 because L1 writes
  `gt(b,1)`/`and(a,b)`/`pow(a,b)`, none of which is a function in the targets.
  `SBML_parseL3FormulaWithModel(formula.c_str(), model)`; NULL →
  `stop(SBML_getLastParseL3Error())`. The `ASTNode*` ownership transfers to
  `setMath` (deep-copies), so `delete` the local node.
- **`removeReaction` is phase 3** because removing a reaction can leave
  dangling species references and may need the bookkeeping `convertReactions`
  already does (`exposeReferencedStoichiometry` and friends); build it after
  the scalar setters and `setRateLaw` are solid. A soft
  `setReactionActive(model, id, FALSE)` (zero the kinetic law, keep the
  structure) is a lighter alternative if a hard remove proves fiddly.

## Mandatory downstream wiring (CLAUDE.md hard rules)

Every new `[[Rcpp::export]]` triggers a fixed chain; skipping any step is a
build/CI failure (`R CMD check --as-cran`, `error-on: warning`):

1. `Rcpp::compileAttributes()` → regenerates `R/RcppExports.R` +
   `src/RcppExports.cpp` (never hand-edit).
2. `devtools::document()` → `man/*.Rd` from the `//'` roxygen in the new
   `.cpp` files. Order is always compileAttributes **then** document.
3. **`_pkgdown.yml`**: every exported function must appear in exactly one
   reference group or pkgdown errors on an unindexed topic. Add two groups:
   **"Editing a model"** (`setParameterValue`, `setCompartmentSize`,
   `setSpeciesIC`, `setRateLaw`, `removeReaction`) and **"Writing a model"**
   (`writeSBML`).
4. Full roxygen per function, with `@param`/`@return` and runnable
   `@examples` (they execute under `R CMD check`).

No `formulaToInfix.h` edit, so no `touch`-the-`.cpp` header-dependency trap.

## Tests (`tests/testthat/test-edit.R`)

The drift-proof test is a **round-trip**: load an example → mutate →
`writeSBML` to `tempfile(fileext=".xml")` → `getModel` the output → assert the
change survived and untouched components are unchanged. Plus the error paths,
in the package's established style:

- Unknown id → `expect_error` matching the named message.
- Empty model / empty component list → `stop`.
- Bad formula → `expect_error` carrying the L3 parse error.
- `writeSBML` returns invisibly → `expect_invisible`.
- Live-pointer mutation visibility: `setParameterValue` then
  `getParameterTable` reflects the new value *without* reloading — proves the
  in-place mutation on the shared XPtr.
- A deliberately-inconsistent edit → `writeSBML` refuses (the shared severity
  gate).
- `setSpeciesIC(..., as = "amount")` vs `as = "concentration")`: the named
  attribute is set and the other is unset.

## Known gap (flag, out of scope for the first cut)

`convertReactions(infile, outfile, …)` reads the **file**, not a model
pointer. So "edit the loaded model, then simulate" goes edit → `writeSBML` to
a temp file → `convertReactions(tempfile, …)`. That works and is minimal. A
`convertReactions` overload taking a model pointer would remove the disk
round-trip but is a separate, larger change (all six writers reach for the
filename/document today).

## Phasing

1. `writeSBML` + scalar setters (`setParameterValue`, `setCompartmentSize`,
   `setSpeciesIC`) + round-trip tests + docs/pkgdown wiring.
2. `setRateLaw` (L3 parser → KineticLaw) — the qspfit rate-law editor.
3. `removeReaction` / `setReactionActive` — species-reference bookkeeping.
