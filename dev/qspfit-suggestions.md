# r2sbml suggestions (from qspfit)

Found 2026-11-22 while planning a reactions view for the qspfit.org web
interface (see "Reactions as data in the model" in phase 0 and "Reactions
view" in phase 2 of `qspfit/todo.md`). qspfit wants to show each reaction
with its equation, rate law and parameters, let the user edit rate laws and
parameter values or switch reactions off, and rebuild the model from that.
This needs the reactions as data, not only as ODE code.

## What r2sbml does now

- `getReactionTable(getModel(f))` returns each reaction's `Number`, `ID`,
  `Name`, `Reversible` and `RateLaw`; for `sbmlmutlicompartment.xml`, for
  example, `transport`, `TRUE`, `cytoplasm * KT * (Y1n - Y1c)`. It does
  not return reactants, products or stoichiometry.
  `src/convertReactions.cpp` already reads the species references
  (`getReactant()`, `getProduct()`, `exposeRuleStoichiometry()`), so the
  information is available in C++, just not returned to R.
- `convertReactions(format = "R")` writes an empty `## Reactions` section
  and copies every rate law into each ODE that uses it. In `sbmlsimple`,
  `veq_kon * E * S - veq_koff * ES` appears three times in `massBalances`.
  A reader of the `.R` file can't recover the reactions from it.
- Some models have no reactions: `sbmlsimple.xml` reports `reactions: 0`
  and is built from rules. Nothing below should change how those convert.

## Requests, in priority order

1. **Return stoichiometry from `getReactionTable()`.** Add the reactants
   and products with their stoichiometries: a list column, or a separate
   long table with reaction, species, role (reactant, product, modifier)
   and stoichiometry. Also return each reaction's compartment and its
   local parameters (with the prefixed names that `convertReactions()`
   uses, such as `veq_kon`, so they match the generated `parameters`).
2. **Write reactions as data in `convertReactions(format = "R")`.**
   Instead of copying rate laws into the ODEs, write them once, e.g.

   ```r
   reactions <- list(
     binding = list(eq = "E + S <-> ES",
                    rate = quote(kon * E * S - koff * ES)),
     ...
   )
   ```

   plus a stoichiometry matrix (states × reactions), and write
   `massBalances` as `S %*% rates`. qspfit then reads `reactions` and the
   matrix and can rebuild the right-hand side after an edit. Keep the
   generated code to the constructs qspfit's model validator allows
   (assignments, `function`, `c()`, `list()`, `quote()`, arithmetic and
   the math functions from `getMath.cpp`). Files in the current format,
   with rates copied into the ODEs, must still load in qspfit, so this
   can be a new layout rather than a change to an existing one.
3. **Write edited models back to SBML.** A function that takes a model (or
   the SBML file) plus changes (new rate laws, parameter values, reactions
   removed or added) and writes SBML, so the website can export what the
   user edited.

## Why it matters to qspfit

- The web interface's Model step opens on the reaction table: equation,
  rate law with editable parameters, an on/off switch, and fluxes.
- In plain R, it enables `reactions(model)`, `set_rate(model, id, expr)`,
  `qsp_simulate(model, knockout = "degradation")` and reaction fluxes as
  simulation outputs.
