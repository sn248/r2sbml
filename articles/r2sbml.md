# Getting started with r2sbml

`r2sbml` is a thin R interface to
[libSBML](https://sbml.org/software/libsbml/), built with Rcpp. It does
two things:

1.  **Reads** an [SBML](https://sbml.org) model and lets you query its
    components from R — species, compartments, parameters, reactions,
    rules, events and function definitions.
2.  **Exports** the model’s mass balances as ready-to-simulate code for
    six ODE targets: deSolve, mrgsolve, rxode2/nlmixr2, MATLAB, Julia
    and ubiquity.

It does *not* let you modify a model. There is no setter API and no way
to write SBML back out; `r2sbml` reads and exports.

libSBML itself is bundled with the package and compiled from source
during installation, so nothing needs to be installed separately and the
build downloads nothing. See the README for the compiler and header
prerequisites.

``` r

library(r2sbml)
```

Ten small example models ship with the package and are used throughout
this vignette:

``` r

basename(list.files(system.file("examples", package = "r2sbml"), pattern = "[.]xml$"))
#>  [1] "sbmlalgebraicrules.xml"              
#>  [2] "sbmlassignmentrules.xml"             
#>  [3] "sbmlboundaryconditions.xml"          
#>  [4] "sbmlconversionfactor-alternative.xml"
#>  [5] "sbmlconversionfactor.xml"            
#>  [6] "sbmldelay.xml"                       
#>  [7] "sbmlevent.xml"                       
#>  [8] "sbmlfunctiondefinition.xml"          
#>  [9] "sbmlmutlicompartment.xml"            
#> [10] "sbmlsimple.xml"
```

## Reading a model

Everything except
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
works in two phases. First
[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
reads the file, keeps the underlying `SBMLDocument` alive, and hands
back an external pointer:

``` r

sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
model <- getModel(sbml_file)
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlsimple.xml (Level 3, version 2)
class(model)
#> [1] "externalptr"
```

[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
prints a short header as a side effect — the file it read, the number of
diagnostics libSBML reported, and the Level/Version of the document. If
there were any, it prints them and stops with `"Stopping!"` rather than
handing back a pointer you cannot trust.

Reading is not validation. libSBML reports parse-level and structural
problems as it reads, but
[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
does not run a consistency check, so a file that loads cleanly can still
be semantically questionable — inconsistent units, say. `r2sbml` never
validates a model.

Every query function then takes that pointer as its only argument.

### Functions that return data

``` r

getSpeciesTable(model)
#>   Number Name ID InitialConcentration InitialAmount Compartment CompartmentVol
#> 1      1       E                  NaN         5e-21        comp          1e-14
#> 2      2       S                  NaN         1e-20        comp          1e-14
#> 3      3       P                  NaN         0e+00        comp          1e-14
#> 4      4      ES                  NaN         0e+00        comp          1e-14
#>   BoundaryCondition Constant
#> 1                 0    FALSE
#> 2                 0    FALSE
#> 3                 0    FALSE
#> 4                 0    FALSE

getReactionTable(model)
#>   Number   ID Name Reversible                          RateLaw
#> 1      1  veq            TRUE comp * (kon * E * S - koff * ES)
#> 2      2 vcat           FALSE                 comp * kcat * ES
```

[`getSpeciesTable()`](https://sn248.github.io/r2sbml/reference/getSpeciesTable.md)
reports both `InitialConcentration` and `InitialAmount` columns. An SBML
species carries one or the other, never both, and the unset one reads
back as `NaN` — so the `NaN` column above is telling you this model is
amount-based, not that anything is wrong.

Simpler accessors return plain vectors:

``` r

getCmtNames(model)
#> [1] "comp"
getCmtSizes(model)
#> 1e-14
#> [1] 1e-14
getNumSpecies(model)
#> [1] 4
```

The full set of data-returning functions:

| Function | Returns |
|----|----|
| [`getSpeciesTable()`](https://sn248.github.io/r2sbml/reference/getSpeciesTable.md), [`getParameterTable()`](https://sn248.github.io/r2sbml/reference/getParameterTable.md), [`getReactionTable()`](https://sn248.github.io/r2sbml/reference/getReactionTable.md) | a `data.frame` |
| [`getSpeciesNames()`](https://sn248.github.io/r2sbml/reference/getSpeciesNames.md), [`getCmtNames()`](https://sn248.github.io/r2sbml/reference/getCmtNames.md) | a character vector |
| [`getSpeciesIC()`](https://sn248.github.io/r2sbml/reference/getSpeciesIC.md), [`getCmtSizes()`](https://sn248.github.io/r2sbml/reference/getCmtSizes.md) | a numeric vector |
| [`getNumSpecies()`](https://sn248.github.io/r2sbml/reference/getNumSpecies.md) | an integer |

[`getSpeciesIC()`](https://sn248.github.io/r2sbml/reference/getSpeciesIC.md)
reports whichever of `initialAmount` or `initialConcentration` the model
sets, named by species id, and applies **no** unit conversion — unlike
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md),
which must divide an amount by the compartment volume because the ODE it
writes integrates concentrations.
[`getSpeciesNames()`](https://sn248.github.io/r2sbml/reference/getSpeciesNames.md)
and
[`getCmtNames()`](https://sn248.github.io/r2sbml/reference/getCmtNames.md)
return the SBML `name` where one is set and fall back to the `id`
otherwise. See
[`vignette("limitations", package = "r2sbml")`](https://sn248.github.io/r2sbml/articles/limitations.md)
for both.

### Functions that print

A second family writes to the console and returns `0` invisibly.
Assigning them gets you the `0`, not the text, so capture them instead:

``` r

rules_model <- getModel(system.file("examples", "sbmlassignmentrules.xml",
                                    package = "r2sbml"))
#> 
#>   filename: /home/runner/work/_temp/Library/r2sbml/examples/sbmlassignmentrules.xml
#>   error(s): 0
#> 
#> 
#> File: /home/runner/work/_temp/Library/r2sbml/examples/sbmlassignmentrules.xml (Level 3, version 2)
getRuleMath(rules_model)
#> Rule 0, formula: S1 = T / (1 + Keq)
#> Rule 1, formula: S2 = Keq * S1
#> [1] 0
```

``` r

txt <- capture.output(getReactionMath(model))
txt
#> [1] "Reaction 0, formula: comp * (kon * E * S - koff * ES)"
#> [2] "Reaction 1, formula: comp * kcat * ES"                
#> [3] "[1] 0"
```

| Function | Prints |
|----|----|
| [`getRuleMath()`](https://sn248.github.io/r2sbml/reference/getRuleMath.md) | assignment, rate and algebraic rules |
| [`getReactionMath()`](https://sn248.github.io/r2sbml/reference/getReactionMath.md) | kinetic laws |
| [`getFunctionDefinition()`](https://sn248.github.io/r2sbml/reference/getFunctionDefinition.md) | function definitions |
| [`getEventMath()`](https://sn248.github.io/r2sbml/reference/getEventMath.md) | event triggers, delays and assignments |
| [`printSBML()`](https://sn248.github.io/r2sbml/reference/printSBML.md), [`echoSBML()`](https://sn248.github.io/r2sbml/reference/echoSBML.md) | the document itself |

[`printSBML()`](https://sn248.github.io/r2sbml/reference/printSBML.md)
and [`echoSBML()`](https://sn248.github.io/r2sbml/reference/echoSBML.md)
are the exception to the two-phase rule: they take a *filename*, not a
model pointer.

### Absent components are an error, not an empty result

A query for a component the model does not have raises an error. It does
not return a zero-row data frame or a zero-length vector:

``` r

# sbmlsimple.xml has no rules at all
getRuleMath(model)
#> No Rules present in the model.
#> Error:
#> ! Stopping!
```

Note where the information is: the explanatory line goes to the console,
while the condition message itself is only `"Stopping!"`. If you are
looping over many models, wrap the call:

``` r

safely <- function(expr) tryCatch(expr, error = function(e) NULL)
files  <- list.files(system.file("examples", package = "r2sbml"),
                     pattern = "[.]xml$", full.names = TRUE)

has_rules <- vapply(files, function(f) {
  !is.null(safely(capture.output(getRuleMath(getModel(f)))))
}, logical(1))
data.frame(model = basename(files), has_rules = unname(has_rules))
#>                                   model has_rules
#> 1                sbmlalgebraicrules.xml      TRUE
#> 2               sbmlassignmentrules.xml      TRUE
#> 3            sbmlboundaryconditions.xml      TRUE
#> 4  sbmlconversionfactor-alternative.xml     FALSE
#> 5              sbmlconversionfactor.xml     FALSE
#> 6                         sbmldelay.xml      TRUE
#> 7                         sbmlevent.xml      TRUE
#> 8            sbmlfunctiondefinition.xml     FALSE
#> 9              sbmlmutlicompartment.xml     FALSE
#> 10                       sbmlsimple.xml     FALSE
```

The same applies to
[`getParameterTable()`](https://sn248.github.io/r2sbml/reference/getParameterTable.md)
on a model whose parameters are all local to its kinetic laws —
`sbmlsimple.xml` is exactly that case.

## Exporting mass balances

[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
is the other half of the package, and the one function that does not
take a model pointer. It reads the SBML file itself and writes a code
file, returning `NULL` invisibly:

``` r

out_r <- tempfile(fileext = ".R")
convertReactions(sbml_file, out_r, format = "R")
#> Conversion completed.
#> Number of ODEs - 4
```

| `format` | Target |
|----|----|
| `"R"` or `"deSolve"` (default) | [deSolve](https://cran.r-project.org/package=deSolve) |
| `"mrgsolve"` | [mrgsolve](https://mrgsolve.org) |
| `"nlmixr2"` or `"rxode"` | [rxode2](https://nlmixr2.github.io/rxode2/) / nlmixr2 |
| `"MATLAB"` or `"matlab"` | a function file for `ode15s` |
| `"Julia"` or `"julia"` | an `ODEProblem` for DifferentialEquations.jl |
| `"ubiquity"` or `"Ubiquity"` | a [ubiquity](https://r.ubiquity.tools) system file |

An unrecognised format is an error, so a typo cannot leave you with an
empty file that looks like a successful conversion:

``` r

convertReactions(sbml_file, tempfile(), format = "fortran")
#> Conversion completed.
#> Number of ODEs - 4
#> Error:
#> ! Unknown format 'fortran'. Use one of 'R'/'deSolve', 'mrgsolve', 'nlmixr2'/'rxode', 'MATLAB', 'Julia' or 'ubiquity'.
```

### What happens before anything is written

[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
applies four libSBML converters in sequence:

1.  `replaceReactions` — every reaction becomes a rate rule
2.  `promoteLocalParameters` — parameters local to a kinetic law become
    global
3.  `expandInitialAssignments` — initial assignments are folded into
    values
4.  `expandFunctionDefinitions` — user-defined functions are inlined

By the time a writer runs, the model is in rate-rule form with no local
parameters and no function definitions left. That is why the generated
code never contains a function definition, and why parameters that were
buried inside a kinetic law appear as top-level constants.

### The deSolve output

``` r

cat(readLines(out_r), sep = "\n")
#> ## Automatically generated model file by r2sbml at 
#> ## Writing the model ODEs for solving in R
#> ## Solving the model requires desolve package 
#> 
#> ## Model Summary 
#> ## functionDefinitions: 0
#> ##     unitDefinitions: 2
#> ##    compartmentTypes: 0
#> ##        speciesTypes: 0
#> ##        compartments: 1
#> ##             species: 4
#> ##          parameters: 3
#> ##  initialAssignments: 0
#> ##               rules: 4
#> ##         constraints: 0
#> ##           reactions: 0
#> ##              events: 0
#> 
#> ## Load required packages
#> library(deSolve)
#> 
#> ## Units Definitions
#> ## per_second: Kind=, : Exponent=-1
#> ## litre_per_mole_second: Kind=, : Exponent=-1
#> ## litre_per_mole_second: Kind=, : Exponent=1
#> ## litre_per_mole_second: Kind=, : Exponent=-1
#> 
#> ## Compartments 
#> comp = 1e-14
#> 
#> ## Initial Amounts
#> InitialAmounts <- c(
#>          E = 5e-07,
#>          S = 1e-06,
#>          P = 0,
#>          ES = 0
#>                     )
#> 
#> ## Parameters
#> parameters <- c(
#>          veq_koff = 0.2,
#>          veq_kon = 1e+06,
#>          vcat_kcat = 0.1
#>                )
#> 
#> ## Reactions 
#> 
#> ## Mass-Balances (ODEs)
#> massBalances <- function(time, states, params){
#> 
#>    ## Get States Names 
#>    E = states[["E"]]
#>    S = states[["S"]]
#>    P = states[["P"]]
#>    ES = states[["ES"]]
#> 
#>    ## Get Parameter Names 
#>    veq_koff = params[["veq_koff"]]
#>    veq_kon = params[["veq_kon"]]
#>    vcat_kcat = params[["vcat_kcat"]]
#> 
#>    ## Mass Balances
#>    dE_dt = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp) + comp * vcat_kcat * ES / comp
#>    dS_dt = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp)
#>    dP_dt = comp * vcat_kcat * ES / comp
#>    dES_dt = comp * (veq_kon * E * S - veq_koff * ES) / comp + -1 * (comp * vcat_kcat * ES / comp)
#> 
#>    ## Make a list of Mass Balances
#>    MassBalances <- c(
#>      dE_dt ,
#>      dS_dt ,
#>      dP_dt ,
#>      dES_dt
#>    )
#>    return(list(MassBalances))
#> 
#> }
```

Three things in that file are worth pointing out.

**The state vector is decided deliberately.** It is not simply “one
equation per rule”. A species set by an *assignment rule* is computed
inside the right-hand side and given no slot, because its value follows
from the rule at every time point. A species with *no* rate rule — a
boundary condition, or one in no reaction — keeps its slot with a zero
derivative so it still appears in the solution.

**Initial conditions are converted into the units the ODE integrates.**
An SBML species symbol is a concentration unless `hasOnlySubstanceUnits`
is set, and the rate rules are divided by the compartment volume to
match. `sbmlsimple.xml` declares `initialAmount = 5e-21` in a `1e-14`
compartment, so the generated initial condition is `5e-07`, not `5e-21`.
The volume used is the species’ own compartment’s, so a
multi-compartment model rescales species individually.

**Compartment volumes are emitted as plain constants** when they are
constant. If a compartment carries a rate rule instead, it joins the
state vector and is integrated — see “Changing compartment volumes”
below.

You can run the result directly:

``` r

library(deSolve)

env <- new.env()
source(out_r, local = env)

solution <- ode(y     = env$InitialAmounts,
                times = seq(0, 5, by = 1),
                func  = env$massBalances,
                parms = env$parameters)
round(solution, 9)
#>   time        E        S        P       ES
#> 1    0 5.00e-07 1.00e-06 0.00e+00 0.00e+00
#> 2    1 2.51e-07 7.36e-07 1.60e-08 2.49e-07
#> 3    2 1.90e-07 6.46e-07 4.40e-08 3.10e-07
#> 4    3 1.74e-07 5.98e-07 7.60e-08 3.26e-07
#> 5    4 1.72e-07 5.63e-07 1.09e-07 3.28e-07
#> 6    5 1.75e-07 5.34e-07 1.42e-07 3.25e-07
```

``` r

# free + bound enzyme is conserved, and so is total substrate
d <- as.data.frame(solution)
range(d$E + d$ES)
#> [1] 5e-07 5e-07
range(d$S + d$ES + d$P)
#> [1] 1e-06 1e-06
```

### The other five targets

Each target gets its own spelling of the same mathematics.

``` r

out_mrg <- tempfile(fileext = ".cpp")
convertReactions(sbml_file, out_mrg, format = "mrgsolve")
#> Conversion completed.
#> Number of ODEs - 4
cat(readLines(out_mrg), sep = "\n")
#> ## Automatically generated mrgsolve model file by r2sbml
#> 
#> $PROB
#> 
#> $PARAM
#> veq_koff = 0.2
#> veq_kon = 1e+06
#> vcat_kcat = 0.1
#> 
#> $CMT
#> E
#> S
#> P
#> ES
#> 
#> $MAIN
#> E_0 = 5e-07;
#> S_0 = 1e-06;
#> P_0 = 0;
#> ES_0 = 0;
#> comp = 1e-14;
#> 
#> $ODE
#> dxdt_E = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp) + comp * vcat_kcat * ES / comp;
#> dxdt_S = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp);
#> dxdt_P = comp * vcat_kcat * ES / comp;
#> dxdt_ES = comp * (veq_kon * E * S - veq_koff * ES) / comp + -1 * (comp * vcat_kcat * ES / comp);
```

mrgsolve model blocks are C++, where `^` is bitwise XOR and does not
compile for doubles, so powers are written `pow(a, b)` for this target
alone.

``` r

out_rx <- tempfile(fileext = ".R")
convertReactions(sbml_file, out_rx, format = "nlmixr2")
#> Conversion completed.
#> Number of ODEs - 4
cat(readLines(out_rx), sep = "\n")
#> ## Automatically generated nlmixr2/rxode model file by r2sbml
#> 
#> model <- function() {
#>   ini({
#>     veq_koff <- 0.2
#>     veq_kon <- 1e+06
#>     vcat_kcat <- 0.1
#>   })
#>   model({
#>     comp <- 1e-14
#>     E(0) <- 5e-07
#>     S(0) <- 1e-06
#>     P(0) <- 0
#>     ES(0) <- 0
#> 
#>     d/dt(E) <- -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp) + comp * vcat_kcat * ES / comp
#>     d/dt(S) <- -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp)
#>     d/dt(P) <- comp * vcat_kcat * ES / comp
#>     d/dt(ES) <- comp * (veq_kon * E * S - veq_koff * ES) / comp + -1 * (comp * vcat_kcat * ES / comp)
#>   })
#> }
```

``` r

out_m <- tempfile(fileext = ".m")
convertReactions(sbml_file, out_m, format = "MATLAB")
#> Conversion completed.
#> Number of ODEs - 4
cat(readLines(out_m), sep = "\n")
#> % Automatically generated MATLAB model file by r2sbml
#> %
#> % Save as file2a498a75f9a.m, then solve with:  [t, y] = file2a498a75f9a();
#> % Columns of y are, in order: E, S, P, ES
#> %
#> % Model Summary
#> %        compartments: 1
#> %             species: 4
#> %          parameters: 3
#> %               rules: 4
#> %              events: 0
#> 
#> function [t, y] = file2a498a75f9a(tspan, y0)
#> 
#>     if nargin < 1 || isempty(tspan)
#>         tspan = [0 10];
#>     end
#> 
#>     % Compartments
#>     comp = 1e-14;
#> 
#>     % Parameters
#>     veq_koff = 0.2;
#>     veq_kon = 1000000;
#>     vcat_kcat = 0.1;
#> 
#>     % Initial amounts
#>     if nargin < 2 || isempty(y0)
#>         y0 = [ ...
#>             5e-07; % E
#>             1e-06; % S
#>             0; % P
#>             0; % ES
#>         ];
#>     end
#> 
#>     [t, y] = ode15s(@massBalances, tspan, y0);
#> 
#>     function dydt = massBalances(time, states)
#> 
#>         % States
#>         E = states(1);
#>         S = states(2);
#>         P = states(3);
#>         ES = states(4);
#> 
#>         % Mass balances
#>         dydt = zeros(4, 1);
#>         dydt(1) = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp) + comp * vcat_kcat * ES / comp; % E
#>         dydt(2) = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp); % S
#>         dydt(3) = comp * vcat_kcat * ES / comp; % P
#>         dydt(4) = comp * (veq_kon * E * S - veq_koff * ES) / comp + -1 * (comp * vcat_kcat * ES / comp); % ES
#> 
#>     end
#> end
```

MATLAB resolves a function by file name, so the generated function is
named after the output path you gave. **Renaming the `.m` file breaks
it** — regenerate instead of renaming.

``` r

out_jl <- tempfile(fileext = ".jl")
convertReactions(sbml_file, out_jl, format = "Julia")
#> Conversion completed.
#> Number of ODEs - 4
cat(readLines(out_jl), sep = "\n")
#> # Automatically generated Julia model file by r2sbml
#> #
#> # Solve with:  using DifferentialEquations; sol = solve(prob)
#> # Elements of u are, in order: E, S, P, ES
#> #
#> # Model Summary
#> #        compartments: 1
#> #             species: 4
#> #          parameters: 3
#> #               rules: 4
#> #              events: 0
#> 
#> using DifferentialEquations
#> 
#> function massbalances!(du, u, p, t)
#> 
#>     time = t
#> 
#>     # Compartments
#>     comp = 1e-14
#> 
#>     # Parameters
#>     veq_koff = p[1]
#>     veq_kon = p[2]
#>     vcat_kcat = p[3]
#> 
#>     # States
#>     E = u[1]
#>     S = u[2]
#>     P = u[3]
#>     ES = u[4]
#> 
#>     # Mass balances
#>     du[1] = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp) + comp * vcat_kcat * ES / comp  # E
#>     du[2] = -1 * (comp * (veq_kon * E * S - veq_koff * ES) / comp)  # S
#>     du[3] = comp * vcat_kcat * ES / comp  # P
#>     du[4] = comp * (veq_kon * E * S - veq_koff * ES) / comp + -1 * (comp * vcat_kcat * ES / comp)  # ES
#> 
#>     return nothing
#> end
#> 
#> u0 = Float64[5e-07, 1e-06, 0, 0]
#> p = Float64[0.2, 1000000, 0.1]
#> tspan = (0.0, 10.0)
#> prob = ODEProblem(massbalances!, u0, tspan, p)
```

``` r

out_ub <- tempfile(fileext = ".txt")
convertReactions(sbml_file, out_ub, format = "ubiquity")
#> Conversion completed.
#> Number of ODEs - 4
cat(readLines(out_ub), sep = "\n")
#> # Automatically generated ubiquity system file by r2sbml
#> #
#> # Build with:  cfg <- build_system(system_file = "<this file>")
#> #
#> # Model Summary
#> #        compartments: 1
#> #             species: 4
#> #          parameters: 3
#> #               rules: 4
#> #              events: 0
#> 
#> # Compartment volumes
#> <P> comp 1e-14 -inf inf litre yes System
#> 
#> # Parameters
#> <P> veq_koff 0.2 -inf inf per_second yes System
#> <P> veq_kon 1000000 -inf inf litre_per_mole_second yes System
#> <P> vcat_kcat 0.1 -inf inf per_second yes System
#> 
#> # Initial conditions
#> <I> E = 5e-07
#> <I> S = 1e-06
#> <I> P = 0
#> <I> ES = 0
#> 
#> # Mass balances
#> <ODE:E> (((-1)*((comp*(((veq_kon*E)*S)-(veq_koff*ES)))/comp))+(((comp*vcat_kcat)*ES)/comp))
#> <ODE:S> ((-1)*((comp*(((veq_kon*E)*S)-(veq_koff*ES)))/comp))
#> <ODE:P> (((comp*vcat_kcat)*ES)/comp)
#> <ODE:ES> (((comp*(((veq_kon*E)*S)-(veq_koff*ES)))/comp)+((-1)*(((comp*vcat_kcat)*ES)/comp)))
#> 
#> # Outputs
#> <O> E_out = E
#> <O> S_out = S
#> <O> P_out = P
#> <O> ES_out = ES
```

ubiquity has no compartment concept of its own, so a compartment volume
becomes an ordinary `<P>` constant that the rate expressions divide by.
It also spells exponentiation, the transcendental functions and every
comparison as bracketed prefix calls — `SIMINT_POWER[a][b]`,
`SIMINT_LOGN[a]`, `SIMINT_GT[a][b]` — which is why its output looks so
different from the rest.

Parameter bounds are written `-inf inf` rather than the `eps inf` you
may have seen in hand-written ubiquity systems: an SBML value may
legitimately be zero or negative, and a lower bound above the value
would be inconsistent. They matter only for estimation.

## Model features that change the shape of the output

### Assignment rules

A species driven by an assignment rule is not integrated. It is
recomputed inside the right-hand side, before the derivatives that read
it:

``` r

assign_file <- system.file("examples", "sbmlassignmentrules.xml", package = "r2sbml")
out_assign  <- tempfile(fileext = ".R")
convertReactions(assign_file, out_assign, format = "R")
#> Conversion completed.
#> Number of ODEs - 5

lines <- readLines(out_assign)
cat(lines[grep("Assignment Rules", lines):length(lines)], sep = "\n")
#>    ## Assignment Rules
#>    S1 = T / (1 + Keq)
#>    S2 = Keq * S1
#> 
#>    ## Mass Balances
#>    dX0_dt = -1 * (in_k1 * X0 * cell / cell)
#>    dX1_dt = out_k2 * S2 * cell / cell
#>    dT_dt = in_k1 * X0 * cell / cell + -1 * (out_k2 * S2 * cell / cell)
#> 
#>    ## Make a list of Mass Balances
#>    MassBalances <- c(
#>      dX0_dt ,
#>      dX1_dt ,
#>      dT_dt
#>    )
#>    return(list(MassBalances))
#> 
#> }
```

`S1` and `S2` have no slot in the state vector and no derivative; they
are locals. Giving them slots would leave the solver carrying copies
that never update while the rules recompute different values beside
them.

### Algebraic rules make the model a DAE

An algebraic rule is a constraint, `0 = f(...)`, not a derivative. Where
the target can solve a differential-algebraic system, `r2sbml` emits
one.

For deSolve that changes the shape of the generated function entirely:
it becomes a **residual** function for `daspk` of
`(time, states, derivs, params)`, not a derivative function for `ode`:

``` r

alg_file <- system.file("examples", "sbmlalgebraicrules.xml", package = "r2sbml")
out_alg  <- tempfile(fileext = ".R")
convertReactions(alg_file, out_alg, format = "R")
#> Conversion completed.
#> Number of ODEs - 4

lines <- readLines(out_alg)
cat(lines[grep("Residuals:", lines)[1]:(grep("return\\(list\\(Residuals", lines))], sep = "\n")
#>    ## Residuals: 0 = d<state>/dt - f() for an integrated state,
#>    ##            0 = g()               for one fixed by an algebraic rule
#>    res_E = k1_on * E * S - (k1_off + k2) * ES  ## algebraic rule fixing E
#>    res_S = derivs[["S"]] - (-1 * (cell * (k1_on * E * S - k1_off * ES) / cell))
#>    res_ES = E + ES - E_total  ## algebraic rule fixing ES
#>    res_P = derivs[["P"]] - (cell * k2 * ES / cell)
#>    res_E_total = derivs[["E_total"]]  ## E_total is constant
#> 
#>    ## Make a list of Residuals
#>    Residuals <- c(
#>      res_E ,
#>      res_S ,
#>      res_ES ,
#>      res_P ,
#>      res_E_total
#>    )
#>    return(list(Residuals))
```

The file also defines `InitialDerivatives`, because `daspk` needs `y'`
at `t = 0` consistent with the residuals:

``` r

env <- new.env()
source(out_alg, local = env)

sol <- deSolve::daspk(y     = env$InitialAmounts,
                      dy    = env$InitialDerivatives,
                      times = seq(0, 10, by = 2),
                      res   = env$massBalances,
                      parms = env$parameters)
round(sol, 6)
#>   time        E        S       ES        P E_total
#> 1    0 0.500000 1.000000 0.500000 0.000000       1
#> 2    2 0.638103 0.567146 0.361897 0.432854       1
#> 3    4 0.782187 0.278465 0.217813 0.721535       1
#> 4    6 0.892834 0.120029 0.107166 0.879971       1
#> 5    8 0.954673 0.047479 0.045327 0.952521       1
#> 6   10 0.982329 0.017989 0.017671 0.982011       1
```

``` r

d <- as.data.frame(sol)
# both algebraic rules hold along the whole trajectory
max(abs(d$E + d$ES - d$E_total))
#> [1] 1.511014e-13
max(abs(1 * d$E * d$S - (0.5 + 0.5) * d$ES))
#> [1] 4.561434e-07
```

MATLAB and Julia keep the ordinary derivative shape and use a **singular
mass matrix** instead: `M * y' = f`, with a zero row wherever a
constraint sits, so that row reads `0 = f_i`.

``` r

out_alg_m <- tempfile(fileext = ".m")
convertReactions(alg_file, out_alg_m, format = "MATLAB")
#> Conversion completed.
#> Number of ODEs - 4
lines <- readLines(out_alg_m)
cat(grep("M = eye|M\\(|MassSingular|ode15s", lines, value = TRUE), sep = "\n")
#>     M = eye(5);
#>     M(1, 1) = 0; % E
#>     M(3, 3) = 0; % ES
#>     opts = odeset('Mass', M, 'MassSingular', 'yes');
#>     [t, y] = ode15s(@massBalances, tspan, y0, opts);
```

Julia needs both the mass matrix *and* a solver that accepts one, so the
generated header names `Rodas5()` rather than a bare `solve(prob)`:

``` r

out_alg_jl <- tempfile(fileext = ".jl")
convertReactions(alg_file, out_alg_jl, format = "Julia")
#> Conversion completed.
#> Number of ODEs - 4
lines <- readLines(out_alg_jl)
cat(grep("Solve with|^M |^M\\[|mass_matrix|ODEProblem", lines, value = TRUE), sep = "\n")
#> # Solve with:  using DifferentialEquations; sol = solve(prob, Rodas5())
#> M = zeros(5, 5)
#> M[2, 2] = 1.0  # S
#> M[4, 4] = 1.0  # P
#> M[5, 5] = 1.0  # E_total
#> massbalances = ODEFunction(massbalances!; mass_matrix = M)
#> prob = ODEProblem(massbalances, u0, tspan, p)
```

mrgsolve, rxode2 and ubiquity integrate ODEs only. For those, the rule
is written out as a comment and
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
warns — see the limitations vignette.

### Changing compartment volumes

If a compartment carries a rate rule, its volume is integrated alongside
the species, and every species inside it picks up a dilution term. Since
a species symbol is a concentration, `[S] = n/V`, the derivative is

``` math
\frac{d[S]}{dt} \;=\; \frac{1}{V}\frac{dn}{dt} \;-\; \frac{[S]}{V}\frac{dV}{dt}
```

and libSBML’s converter supplies only the first term. `r2sbml` adds the
second.

The example models all have constant compartments, so here is a small
model written out by hand — a species sitting in a compartment that
grows at a constant rate, with no reactions at all:

``` r

growing <- tempfile(fileext = ".xml")
writeLines('<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>
<compartment id="c" size="1" spatialDimensions="3" constant="false"/>
</listOfCompartments>
<listOfSpecies>
<species id="A" compartment="c" initialConcentration="2" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="g" value="1" constant="true"/>
</listOfParameters>
<listOfRules>
<rateRule variable="c">
<math xmlns="http://www.w3.org/1998/Math/MathML"><ci>g</ci></math>
</rateRule>
</listOfRules>
</model>
</sbml>', growing)

out_grow <- tempfile(fileext = ".R")
convertReactions(growing, out_grow, format = "R")
#> Conversion completed.
#> Number of ODEs - 1
lines <- readLines(out_grow)
cat(grep("InitialAmounts|^ +[Ac] =|_dt|states\\[", lines, value = TRUE), sep = "\n")
#> InitialAmounts <- c(
#>          A = 2,
#>          c = 1   ## compartment volume, integrated
#>    A = states[["A"]]
#>    c = states[["c"]]
#>    dA_dt = -A * (g) / c  ## includes dilution by the changing volume
#>    dc_dt = g  ## compartment volume
#>      dA_dt ,
#>      dc_dt
```

`c` is a state, not a constant, and `A` has a non-zero derivative
despite taking part in no reaction — its concentration falls because its
container is growing. The exact solution is `[A](t) = 2 / (1 + t)`, with
the amount `A * c` fixed at 2:

``` r

env <- new.env()
source(out_grow, local = env)

times <- c(0, 1, 3, 7)
d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                func = env$massBalances, parms = env$parameters,
                                rtol = 1e-10, atol = 1e-10))
data.frame(time = times, A = d$A, exact = 2 / (1 + times), amount = d$A * d$c)
#>   time    A exact amount
#> 1    0 2.00  2.00      2
#> 2    1 1.00  1.00      2
#> 3    3 0.50  0.50      2
#> 4    7 0.25  0.25      2
```

## Practical notes

**Write to [`tempfile()`](https://rdrr.io/r/base/tempfile.html) in
examples and tests.**
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
overwrites its output path without asking.

**Expect console output.**
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
prints `Conversion completed.` and the number of ODEs;
[`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
prints its header. Wrap calls in `invisible(capture.output(...))` if
that noise is unwelcome.

**Watch for warnings.** Several model features cannot be expressed in
every target. Where that is true,
[`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
raises an R warning rather than silently emitting code that runs but
computes the wrong thing. The next vignette lists every such case, and —
just as importantly — the cases where no warning is raised.

## Where to go next

``` r

vignette("limitations", package = "r2sbml")
```

covers what `r2sbml` does not do: unsupported SBML constructs, the
per-target capability matrix, which limitations are announced and which
are silent, and the known quirks of the query functions.
