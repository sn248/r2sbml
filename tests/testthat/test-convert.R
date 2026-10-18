test_that("convertReactions formats work", {
    sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlsimple.xml"

    # R (deSolve)
    out_file <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_file, format = "R"))
    lines <- readLines(out_file)
    expect_true(any(grepl("## Solving the model requires desolve package", lines)))

    # mrgsolve
    out_file_mrg <- tempfile(fileext = ".cpp")
    expect_invisible(convertReactions(sbml_file, out_file_mrg, format = "mrgsolve"))
    lines_mrg <- readLines(out_file_mrg)
    expect_true(any(grepl("\\$PROB", lines_mrg)))
    expect_true(any(grepl("\\$PARAM", lines_mrg)))

    # nlmixr2 / rxode2
    out_file_rx <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_file_rx, format = "nlmixr2"))
    lines_rx <- readLines(out_file_rx)
    expect_true(any(grepl("model <- function", lines_rx)))
    expect_true(any(grepl("ini\\(\\{", lines_rx)))

    # "rxode2" is an alias for the same writer
    out_file_rx2 <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_file_rx2, format = "rxode2"))
    expect_identical(readLines(out_file_rx2), lines_rx)

    # MATLAB
    out_file_m <- tempfile(fileext = ".m")
    expect_invisible(convertReactions(sbml_file, out_file_m, format = "MATLAB"))
    lines_m <- readLines(out_file_m)
    expect_true(any(grepl("ode15s", lines_m)))
    expect_true(any(grepl("function dydt = massBalances", lines_m)))
    # MATLAB resolves functions by file name, so the two have to agree
    expect_true(any(grepl(paste0("function \\[t, y\\] = ",
                                 sub("\\.m$", "", basename(out_file_m))),
                          lines_m)))

    # Julia
    out_file_jl <- tempfile(fileext = ".jl")
    expect_invisible(convertReactions(sbml_file, out_file_jl, format = "Julia"))
    lines_jl <- readLines(out_file_jl)
    expect_true(any(grepl("using DifferentialEquations", lines_jl)))
    expect_true(any(grepl("function massbalances!\\(du, u, p, t\\)", lines_jl)))
    expect_true(any(grepl("ODEProblem", lines_jl)))

    # ubiquity
    out_file_ub <- tempfile(fileext = ".txt")
    expect_invisible(convertReactions(sbml_file, out_file_ub, format = "ubiquity"))
    lines_ub <- readLines(out_file_ub)
    expect_true(any(grepl("^<P> ", lines_ub)))
    expect_true(any(grepl("^<I> ", lines_ub)))
    expect_true(any(grepl("^<ODE:", lines_ub)))
})

test_that("ubiquity math uses the SIMINT spellings", {
    # sbmldelay is the only example with an exponentiation, so it is the one
    # that exercises SIMINT_POWER.  It also carries a delay(), which ubiquity
    # cannot express, so the writer is expected to warn.
    sbml_file <- system.file("examples", "sbmldelay.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmldelay.xml"

    out <- tempfile(fileext = ".txt")
    expect_warning(convertReactions(sbml_file, out, format = "ubiquity"),
                   "no equivalent for")

    lines <- readLines(out)
    expect_true(any(grepl("SIMINT_POWER\\[", lines)))
    # `^` is not ubiquity syntax and must not survive into the ODEs
    expect_false(any(grepl("\\^", grep("^<ODE:", lines, value = TRUE))))
    # the unsupported construct is called out in the file, not just at the console
    expect_true(any(grepl("^# WARNING", lines)))
})

test_that("ubiquity assignment rules become dynamic secondary parameters", {
    sbml_file <- system.file("examples", "sbmlassignmentrules.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlassignmentrules.xml"

    out <- tempfile(fileext = ".txt")
    expect_invisible(convertReactions(sbml_file, out, format = "ubiquity"))

    lines <- readLines(out)
    expect_true(any(grepl("^<Ad> S1 = ", lines)))
    # a species set by an assignment rule is not integrated, so it gets no ODE
    expect_false(any(grepl("^<ODE:S1>", lines)))
})

test_that("convertReactions rejects an unknown format", {
    sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlsimple.xml"

    expect_error(convertReactions(sbml_file, tempfile(), format = "fortran"),
                 "Unknown format")
})

test_that("initial values are numeric for concentration-based models", {
    # These models set initialConcentration rather than initialAmount; asking
    # only for the amount used to emit the literal `nan`.
    sbml_file <- system.file("examples", "sbmlassignmentrules.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlassignmentrules.xml"

    for (fmt in c("R", "mrgsolve", "nlmixr2", "MATLAB", "Julia", "ubiquity")) {
        out <- tempfile()
        expect_invisible(convertReactions(sbml_file, out, format = fmt))
        expect_false(any(grepl("\\bnan\\b", readLines(out))))
    }
})

test_that("every state gets a derivative and every derivative a state", {
    # The deSolve, mrgsolve and rxode2 writers used to emit one derivative per
    # *rule* while listing states per *species*.  Two cases came apart:
    # sbmlmutlicompartment has a boundary species with no rate rule, and
    # sbmlalgebraicrules has rules with no variable at all.
    for (model in c("sbmlmutlicompartment.xml", "sbmlalgebraicrules.xml")) {
        sbml_file <- system.file("examples", model, package = "r2sbml")
        if (sbml_file == "") sbml_file <- file.path("../../inst/examples", model)

        out <- tempfile(fileext = ".R")
        expect_invisible(convertReactions(sbml_file, out, format = "R"))
        lines <- readLines(out)

        # an algebraic rule has no variable, so it must not become `d_dt = ...`
        expect_false(any(grepl("^\\s*d_dt\\s*=", lines)))

        env <- new.env()
        env$library <- function(...) invisible(NULL)   # deSolve need not be installed
        source(out, local = env)

        # A model with algebraic rules becomes a daspk residual function of
        # (time, states, derivs, params); everything else stays an ode()
        # derivative function.  Either way one equation per state, and it has
        # to evaluate -- an undefined derivative errors right here.
        v <- if (length(formals(env$massBalances)) == 4L) {
            env$massBalances(0, env$InitialAmounts,
                             env$InitialDerivatives, env$parameters)[[1]]
        } else {
            env$massBalances(0, env$InitialAmounts, env$parameters)[[1]]
        }
        expect_length(v, length(env$InitialAmounts))
        expect_false(any(is.na(v)))

        # a definition carries an `=`, an entry in the returned vector does not
        sym      <- "(res_[A-Za-z0-9_]+|d[A-Za-z0-9_]+_dt)"
        defined  <- sub(paste0("^\\s*", sym, "\\s*=.*$"), "\\1",
                        grep(paste0("^\\s*", sym, "\\s*="), lines, value = TRUE))
        returned <- sub(paste0("^\\s*", sym, "\\s*,?\\s*$"), "\\1",
                        grep(paste0("^\\s*", sym, "\\s*,?\\s*$"), lines, value = TRUE))
        expect_setequal(defined, returned)
    }
})

test_that("a species with no rate rule is held constant, not left undefined", {
    sbml_file <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlmutlicompartment.xml"

    # X is a boundary species: it keeps its slot, with a zero derivative.
    out_r <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_r, format = "R"))
    expect_true(any(grepl("^\\s*dX_dt = 0\\b", readLines(out_r))))

    out_mrg <- tempfile(fileext = ".cpp")
    expect_invisible(convertReactions(sbml_file, out_mrg, format = "mrgsolve"))
    expect_true(any(grepl("^dxdt_X = 0;", readLines(out_mrg))))

    out_rx <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_rx, format = "nlmixr2"))
    expect_true(any(grepl("^\\s*d/dt\\(X\\) <- 0\\b", readLines(out_rx))))
})

test_that("assignment-rule species are locals, not states", {
    sbml_file <- system.file("examples", "sbmlassignmentrules.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlassignmentrules.xml"

    # S1 and S2 follow from their rules at every time point, so they are
    # computed inside the RHS and get neither a slot nor a derivative.
    out_r <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_r, format = "R"))
    lines <- readLines(out_r)
    expect_true(any(grepl("^\\s*S1 = T / \\(1 \\+ Keq\\)", lines)))
    expect_false(any(grepl("dS1_dt", lines)))
    expect_false(any(grepl("^\\s*S1 = states\\[", lines)))

    out_mrg <- tempfile(fileext = ".cpp")
    expect_invisible(convertReactions(sbml_file, out_mrg, format = "mrgsolve"))
    lines_mrg <- readLines(out_mrg)
    expect_true(any(grepl("^double S1 = ", lines_mrg)))
    expect_false(any(grepl("dxdt_S1", lines_mrg)))

    out_rx <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_rx, format = "nlmixr2"))
    lines_rx <- readLines(out_rx)
    expect_true(any(grepl("^\\s*S1 <- ", lines_rx)))
    expect_false(any(grepl("d/dt\\(S1\\)", lines_rx)))
})

test_that("algebraic rules become a DAE for the targets that can solve one", {
    sbml_file <- system.file("examples", "sbmlalgebraicrules.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlalgebraicrules.xml"

    # E and ES are non-constant with no rate rule, and there are exactly two
    # algebraic rules, so the system is square: 5 states, 2 of them algebraic.
    out_r <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(sbml_file, out_r, format = "R"))
    lines <- readLines(out_r)
    # a residual function for daspk, not a derivative function for ode
    expect_true(any(grepl("massBalances <- function\\(time, states, derivs, params\\)", lines)))
    expect_true(any(grepl("^\\s*res_E = k1_on \\* E \\* S - \\(k1_off \\+ k2\\) \\* ES", lines)))
    expect_true(any(grepl("^\\s*res_ES = E \\+ ES - E_total", lines)))
    # an integrated state keeps dy - f(), so the rate rules are still there
    expect_true(any(grepl("^\\s*res_S = derivs\\[\\[\"S\"\\]\\] - \\(", lines)))
    expect_true(any(grepl("^InitialDerivatives <- with\\(", lines)))

    # MATLAB: singular mass matrix, zero row for each algebraically fixed state
    out_m <- tempfile(fileext = ".m")
    expect_no_warning(convertReactions(sbml_file, out_m, format = "MATLAB"))
    lines_m <- readLines(out_m)
    expect_true(any(grepl("^\\s*M = eye\\(5\\);", lines_m)))
    expect_true(any(grepl("^\\s*M\\(1, 1\\) = 0;", lines_m)))   # E
    expect_true(any(grepl("^\\s*M\\(3, 3\\) = 0;", lines_m)))   # ES
    expect_true(any(grepl("'MassSingular', 'yes'", lines_m)))
    expect_true(any(grepl("ode15s\\(@massBalances, tspan, y0, opts\\)", lines_m)))

    # Julia: mass matrix on the ODEFunction, and a solver that accepts one
    out_jl <- tempfile(fileext = ".jl")
    expect_no_warning(convertReactions(sbml_file, out_jl, format = "Julia"))
    lines_jl <- readLines(out_jl)
    expect_true(any(grepl("^M = zeros\\(5, 5\\)", lines_jl)))
    expect_true(any(grepl("mass_matrix = M", lines_jl)))
    expect_true(any(grepl("Rodas5\\(\\)", lines_jl)))
    expect_false(any(grepl("^M\\[1, 1\\]", lines_jl)))          # E is algebraic
    expect_true(any(grepl("^M\\[2, 2\\] = 1.0", lines_jl)))     # S is integrated
})

test_that("targets that cannot express a DAE say so", {
    sbml_file <- system.file("examples", "sbmlalgebraicrules.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlalgebraicrules.xml"

    for (fmt in c("mrgsolve", "nlmixr2", "ubiquity")) {
        expect_warning(convertReactions(sbml_file, tempfile(), format = fmt),
                       "algebraic rule")
    }

    # and a model without algebraic rules must not warn in any target
    simple <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
    if (simple == "") simple <- "../../inst/examples/sbmlsimple.xml"
    for (fmt in c("R", "mrgsolve", "nlmixr2", "MATLAB", "Julia", "ubiquity")) {
        expect_no_warning(convertReactions(simple, tempfile(), format = fmt))
    }
})

test_that("the generated DAE solves and honours its constraints", {
    skip_if_not_installed("deSolve")

    sbml_file <- system.file("examples", "sbmlalgebraicrules.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlalgebraicrules.xml"

    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(sbml_file, out, format = "R"))

    env <- new.env()
    source(out, local = env)
    sol <- deSolve::daspk(y = env$InitialAmounts, dy = env$InitialDerivatives,
                          times = seq(0, 10, by = 1), res = env$massBalances,
                          parms = env$parameters)
    d <- as.data.frame(sol)
    expect_equal(nrow(d), 11L)
    expect_false(any(is.na(d)))

    # both algebraic rules hold along the whole trajectory
    expect_lt(max(abs(d$E + d$ES - d$E_total)), 1e-6)
    expect_lt(max(abs(1 * d$E * d$S - (0.5 + 0.5) * d$ES)), 1e-5)

    # S + P is conserved only *because* the first constraint is enforced:
    # dS/dt + dP/dt cancels exactly when k1_on*E*S == (k1_off + k2)*ES
    expect_lt(max(abs(d$S + d$P - 1)), 1e-5)
})

# A one-species first-order decay in a compartment of volume 4, written out so
# the expected trajectory is known exactly.  `amount` picks which attribute
# carries the initial value and whether the species is in substance units.
decay_model <- function(amount = TRUE) {
    species <- if (amount) {
        'initialAmount="8" hasOnlySubstanceUnits="false"'
    } else {
        'initialConcentration="3" hasOnlySubstanceUnits="true"'
    }
    # in substance units the kinetic law is already an amount rate, so it must
    # not carry the extra factor of c that the concentration form needs
    law <- if (amount) "<ci>k</ci><ci>A</ci><ci>c</ci>" else "<ci>k</ci><ci>A</ci>"
    xml <- sprintf('<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>
<compartment id="c" size="4" spatialDimensions="3" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="A" compartment="c" %s boundaryCondition="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="k" value="0.5" constant="true"/>
</listOfParameters>
<listOfReactions>
<reaction id="decay" reversible="false">
<listOfReactants>
<speciesReference species="A" stoichiometry="1" constant="true"/>
</listOfReactants>
<kineticLaw>
<math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/>%s</apply>
</math>
</kineticLaw>
</reaction>
</listOfReactions>
</model>
</sbml>', species, law)
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

test_that("an amount-valued initial condition is converted to a concentration", {
    # replaceReactions divides the rate rule by the compartment volume, so the
    # state is a concentration; an initialAmount has to be divided to match.
    sbml_file <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
    if (sbml_file == "") sbml_file <- "../../inst/examples/sbmlsimple.xml"

    # E: initialAmount 5e-21 in a 1e-14 compartment -> 5e-07, not 5e-21
    out <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out, format = "R"))
    env <- new.env()
    env$library <- function(...) invisible(NULL)
    source(out, local = env)
    expect_equal(unname(env$InitialAmounts[["E"]]), 5e-07)
    expect_equal(unname(env$InitialAmounts[["S"]]), 1e-06)

    # every writer shares the helper, so none of them may still show the amount
    for (fmt in c("R", "mrgsolve", "nlmixr2", "MATLAB", "Julia", "ubiquity")) {
        o <- tempfile()
        expect_invisible(convertReactions(sbml_file, o, format = fmt))
        expect_false(any(grepl("5e-21", readLines(o), fixed = TRUE)))
    }

    # per-compartment, not per-model: Y2 is in cytoplasm (size 5), Y1n in
    # nucleus (size 1), so only Y2 is rescaled
    multi <- system.file("examples", "sbmlmutlicompartment.xml", package = "r2sbml")
    if (multi == "") multi <- "../../inst/examples/sbmlmutlicompartment.xml"
    out2 <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(multi, out2, format = "R"))
    env2 <- new.env()
    env2$library <- function(...) invisible(NULL)
    source(out2, local = env2)
    expect_equal(unname(env2$InitialAmounts[["Y2"]]), 0.2)
    expect_equal(unname(env2$InitialAmounts[["Y1n"]]), 1)
})

test_that("a hasOnlySubstanceUnits species keeps its amount", {
    # the mirror image: the state is an amount, so an initialConcentration has
    # to be multiplied by the volume, and the rate rule is not divided by it
    out <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(decay_model(amount = FALSE), out, format = "R"))
    env <- new.env()
    env$library <- function(...) invisible(NULL)
    source(out, local = env)
    expect_equal(unname(env$InitialAmounts[["A"]]), 12)          # 3 * 4
    expect_true(any(grepl("dA_dt = -1 \\* \\(k \\* A\\)$", readLines(out))))
})

test_that("the generated model reproduces a known analytic solution", {
    skip_if_not_installed("deSolve")

    # [A](t) = ([A]0) * exp(-k t), with [A]0 = 8 amount / 4 volume = 2
    out <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(decay_model(amount = TRUE), out, format = "R"))
    env <- new.env()
    source(out, local = env)

    times <- c(0, 1, 2, 4, 8)
    sol <- deSolve::ode(y = env$InitialAmounts, times = times,
                        func = env$massBalances, parms = env$parameters)
    expect_equal(as.numeric(sol[, "A"]), 2 * exp(-0.5 * times), tolerance = 1e-5)

    # the same model in substance units: A(t) = 12 * exp(-k t)
    out2 <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(decay_model(amount = FALSE), out2, format = "R"))
    env2 <- new.env()
    source(out2, local = env2)
    sol2 <- deSolve::ode(y = env2$InitialAmounts, times = times,
                         func = env2$massBalances, parms = env2$parameters)
    expect_equal(as.numeric(sol2[, "A"]), 12 * exp(-0.5 * times), tolerance = 1e-5)
})

# A species in a compartment whose volume grows at a constant rate g, with
# optional first-order decay.  [A] = n/V, so the concentration falls even with
# no reaction at all.  `assignment` drives the volume by an assignment rule
# instead of a rate rule: TRUE is V = 1 + g * time, the same volume, whose
# derivative the converter forms itself; "constant" is V = 1 + g, which needs
# no dilution; "piecewise" wraps the first in a construct it cannot
# differentiate.
growing_volume_model <- function(decay = FALSE, assignment = FALSE) {
    time <- '<csymbol encoding="text" definitionURL="http://www.sbml.org/sbml/symbols/time">time</csymbol>'
    linear <- sprintf('<apply><plus/><cn>1</cn><apply><times/><ci>g</ci>%s</apply></apply>', time)
    volume_math <- switch(as.character(assignment),
        "TRUE"      = linear,
        "constant"  = '<apply><plus/><cn>1</cn><ci>g</ci></apply>',
        "piecewise" = sprintf('<piecewise><piece>%s<apply><lt/>%s<cn>100</cn></apply></piece>
                               <otherwise><cn>101</cn></otherwise></piecewise>', linear, time),
        NULL)
    volume_rule <- if (!is.null(volume_math)) {
        sprintf('<assignmentRule variable="c"><math xmlns="http://www.w3.org/1998/Math/MathML">
         %s</math></assignmentRule>', volume_math)
    } else {
        '<rateRule variable="c"><math xmlns="http://www.w3.org/1998/Math/MathML">
         <ci>g</ci></math></rateRule>'
    }
    reaction <- if (!decay) "" else
'<listOfReactions>
<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>k</ci><ci>A</ci><ci>c</ci></apply></math></kineticLaw>
</reaction>
</listOfReactions>'
    xml <- sprintf('<?xml version="1.0" encoding="UTF-8" ?>
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
<parameter id="k" value="0.5" constant="true"/>
</listOfParameters>
<listOfRules>%s</listOfRules>
%s
</model>
</sbml>', volume_rule, reaction)
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

test_that("a compartment with a rate rule becomes a state, not a constant", {
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(growing_volume_model(), out, format = "R"))
    lines <- readLines(out)

    # not emitted as a file-level constant any more
    expect_false(any(grepl("^c = 1$", lines)))
    # integrated instead: an initial value, an unpacking and a derivative
    expect_true(any(grepl("^\\s*c = 1\\b.*compartment volume", lines)))
    expect_true(any(grepl('^\\s*c = states\\[\\["c"\\]\\]', lines)))
    expect_true(any(grepl("^\\s*dc_dt = g\\b", lines)))
    expect_true(any(grepl("^\\s*dc_dt\\s*$", lines)))   # in the returned vector

    # every other target integrates it too
    for (spec in list(c("mrgsolve", "^dxdt_c = g;"),
                      c("nlmixr2",  "^\\s*d/dt\\(c\\) <- g\\b"),
                      c("MATLAB",   "^\\s*dydt\\(2\\) = g;"),
                      c("Julia",    "^\\s*du\\[2\\] = g\\b"),
                      c("ubiquity", "^<ODE:c> g"))) {
        o <- tempfile()
        expect_no_warning(convertReactions(growing_volume_model(), o, format = spec[1]))
        expect_true(any(grepl(spec[2], readLines(o))))
    }
})

test_that("species in a varying compartment get the dilution term", {
    # d[A]/dt = (dn/dt)/V - [A]*(dV/dt)/V; libSBML supplies only the first term
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(growing_volume_model(decay = TRUE), out, format = "R"))
    expect_true(any(grepl("dA_dt = \\(.*\\) - A \\* \\(g\\) / c", readLines(out))))

    # with no reaction at all the derivative is dilution alone, not zero
    out2 <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(growing_volume_model(), out2, format = "R"))
    lines2 <- readLines(out2)
    expect_true(any(grepl("^\\s*dA_dt = -A \\* \\(g\\) / c", lines2)))
    expect_false(any(grepl("dA_dt = 0", lines2)))

    # a constant-volume model must be untouched by any of this
    simple <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
    if (simple == "") simple <- "../../inst/examples/sbmlsimple.xml"
    out3 <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(simple, out3, format = "R"))
    lines3 <- readLines(out3)
    expect_true(any(grepl("^comp = 1e-14$", lines3)))
    expect_false(any(grepl("dcomp_dt", lines3)))
})

test_that("an assignment-rule compartment is diluted through its rule's derivative", {
    # dV/dt is the time derivative of the rule, here d(1 + g t)/dt = g
    for (spec in list(c("R",        "dA_dt = -A \\* \\(.*g.*\\) / c"),
                      c("mrgsolve", "dxdt_A = -A \\* \\(.*g.*\\) / c;"),
                      c("nlmixr2",  "d/dt\\(A\\) <- -A \\* \\(.*g.*\\) / c"),
                      c("MATLAB",   "dydt\\(1\\) = -A \\* \\(.*g.*\\) / c;"),
                      c("Julia",    "du\\[1\\] = -A \\* \\(.*g.*\\) / c"),
                      c("ubiquity", "<ODE:A> -A \\* \\(.*g.*\\) / c"))) {
        o <- tempfile()
        expect_no_warning(convertReactions(growing_volume_model(assignment = TRUE), o,
                                           format = spec[1]))
        expect_true(any(grepl(spec[2], readLines(o))), info = spec[1])
    }

    skip_if_not_installed("deSolve")
    times <- c(0, 1, 3, 7)
    for (decay in c(FALSE, TRUE)) {
        out <- tempfile(fileext = ".R")
        convertReactions(growing_volume_model(decay = decay, assignment = TRUE), out)
        env <- new.env(); source(out, local = env)
        d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                        func = env$massBalances, parms = env$parameters,
                                        rtol = 1e-10, atol = 1e-10))
        k <- if (decay) 0.5 else 0
        expect_equal(d$A, 2 * exp(-k * times) / (1 + times), tolerance = 1e-7)
    }
})

test_that("mrgsolve spells time as SOLVERTIME in $ODE and TIME in $TABLE", {
    # `time` is libc's time() in mrgsolve's C++, so a rule reading SBML's time
    # did not compile at all
    o <- tempfile(fileext = ".cpp")
    expect_no_warning(convertReactions(growing_volume_model(decay = TRUE, assignment = TRUE),
                                       o, format = "mrgsolve"))
    code  <- readLines(o)
    table <- seq_along(code) > match("$TABLE", code)
    expect_true(any(grepl("\\bSOLVERTIME\\b", code[!table])))
    expect_true(any(grepl("\\bTIME\\b", code[table])))
    expect_false(any(grepl("\\btime\\b", code[!grepl("^\\s*(//|#)", code)])))

    skip_on_cran()
    skip_if_not_installed("mrgsolve")
    times <- c(0, 1, 3, 7)
    mod <- suppressMessages(mrgsolve::mcode("r2sbml_time_volume",
                                            paste(code, collapse = "\n"), quiet = TRUE))
    s <- mrgsolve::mrgsim_df(mod, tgrid = times, end = -1, rtol = 1e-10, atol = 1e-10)
    expect_equal(s$A[match(times, s$time)], 2 * exp(-0.5 * times) / (1 + times),
                 tolerance = 1e-6)
    expect_equal(s$c[match(times, s$time)], 1 + times, tolerance = 1e-10)
})

test_that("an assignment rule that does not vary needs no dilution and no warning", {
    for (fmt in c("R", "mrgsolve", "nlmixr2", "MATLAB", "Julia", "ubiquity")) {
        o <- tempfile()
        expect_no_warning(convertReactions(growing_volume_model(assignment = "constant"), o,
                                           format = fmt))
        expect_false(any(grepl("dilution", readLines(o))), info = fmt)
    }
})

test_that("an assignment-rule compartment it cannot differentiate still warns", {
    # piecewise has no derivative here, so the volume is right but the
    # dilution term cannot be formed
    for (fmt in c("R", "mrgsolve", "nlmixr2", "MATLAB", "Julia", "ubiquity")) {
        expect_warning(
            convertReactions(growing_volume_model(assignment = "piecewise"), tempfile(),
                             format = fmt),
            "could not be differentiated")
    }
})

# V = p^2 with dp/dt = g and p(0) = 1, so V = (1 + g t)^2 and, with first-order
# decay, [A] = 2 exp(-k t) / (1 + g t)^2.  Exercises the chain rule through a
# rate-rule state and through a power whose base is not the variable itself,
# which libSBML's own ASTNode::derivative() gets wrong.
squared_volume_model <- function() {
    xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments><compartment id="c" size="1" spatialDimensions="3" constant="false"/></listOfCompartments>
<listOfSpecies><species id="A" compartment="c" initialConcentration="2" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/></listOfSpecies>
<listOfParameters>
<parameter id="g" value="2" constant="true"/>
<parameter id="k" value="0.5" constant="true"/>
<parameter id="p" value="1" constant="false"/>
</listOfParameters>
<listOfRules>
<assignmentRule variable="c"><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><power/><ci>p</ci><cn>2</cn></apply></math></assignmentRule>
<rateRule variable="p"><math xmlns="http://www.w3.org/1998/Math/MathML"><ci>g</ci></math></rateRule>
</listOfRules>
<listOfReactions><reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>k</ci><ci>A</ci><ci>c</ci></apply></math></kineticLaw>
</reaction></listOfReactions>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

test_that("a nonlinear assignment-rule volume reproduces its analytic solution", {
    times <- c(0, 1, 3, 7)
    exact <- 2 * exp(-0.5 * times) / (1 + 2 * times)^2

    skip_if_not_installed("deSolve")
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(squared_volume_model(), out))
    env <- new.env(); source(out, local = env)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters,
                                    rtol = 1e-10, atol = 1e-10))
    expect_equal(d$A, exact, tolerance = 1e-6)
    expect_equal(d$c, (1 + 2 * times)^2, tolerance = 1e-6)

    skip_on_cran()
    skip_if_not_installed("mrgsolve")
    o <- tempfile(fileext = ".cpp")
    convertReactions(squared_volume_model(), o, format = "mrgsolve")
    mod <- suppressMessages(mrgsolve::mcode("r2sbml_squared_volume",
                                            paste(readLines(o), collapse = "\n"), quiet = TRUE))
    s <- mrgsolve::mrgsim_df(mod, tgrid = times, end = -1, rtol = 1e-10, atol = 1e-10)
    expect_equal(s$A[match(times, s$time)], exact, tolerance = 1e-6)

    skip_if_not_installed("rxode2")
    o2 <- tempfile(fileext = ".R")
    convertReactions(squared_volume_model(), o2, format = "rxode2")
    env2 <- new.env(); source(o2, local = env2)
    fn <- Filter(function(n) is.function(env2[[n]]), ls(env2))[1]
    r <- suppressMessages(rxode2::rxSolve(env2[[fn]], events = rxode2::et(times),
                                          rtol = 1e-10, atol = 1e-10))
    expect_equal(r$A, exact, tolerance = 1e-6)
})

test_that("a growing compartment reproduces its analytic solution", {
    skip_if_not_installed("deSolve")

    times <- c(0, 1, 3, 7)

    # no reaction: the amount is fixed at 2, so [A] = 2 / (1 + t)
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(growing_volume_model(), out, format = "R"))
    env <- new.env(); source(out, local = env)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters,
                                    rtol = 1e-10, atol = 1e-10))
    expect_equal(d$c, 1 + times, tolerance = 1e-8)
    expect_equal(d$A, 2 / (1 + times), tolerance = 1e-8)
    expect_equal(d$A * d$c, rep(2, length(times)), tolerance = 1e-8)  # amount conserved

    # with decay: [A] = 2 * exp(-k t) / (1 + t)
    out2 <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(growing_volume_model(decay = TRUE), out2, format = "R"))
    env2 <- new.env(); source(out2, local = env2)
    d2 <- as.data.frame(deSolve::ode(y = env2$InitialAmounts, times = times,
                                     func = env2$massBalances, parms = env2$parameters,
                                     rtol = 1e-10, atol = 1e-10))
    expect_equal(d2$A, 2 * exp(-0.5 * times) / (1 + times), tolerance = 1e-7)
})

# First-order decay whose rate constant itself ramps: dk/dt = r.  So
# k(t) = k0 + r t and [A] = A0 * exp(-(k0 t + r t^2 / 2)).  This is the shape of
# SBML Test Suite case 00033, where the parameter's rate rule used to be
# dropped and k stayed frozen at k0.
ramping_rate_model <- function() {
    xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>
<compartment id="c" size="1" spatialDimensions="3" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="A" compartment="c" initialConcentration="2" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="k" value="0.5" constant="false"/>
<parameter id="r" value="0.2" constant="true"/>
</listOfParameters>
<listOfRules>
<rateRule variable="k"><math xmlns="http://www.w3.org/1998/Math/MathML">
<ci>r</ci></math></rateRule>
</listOfRules>
<listOfReactions>
<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>k</ci><ci>A</ci><ci>c</ci></apply></math></kineticLaw>
</reaction>
</listOfReactions>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

test_that("a parameter with a rate rule becomes a state, not a constant", {
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(ramping_rate_model(), out, format = "R"))
    lines <- readLines(out)

    # integrated: an initial value, an unpacking from states and a derivative
    expect_true(any(grepl("^\\s*k = 0.5\\b.*parameter", lines)))
    expect_true(any(grepl('^\\s*k = states\\[\\["k"\\]\\]', lines)))
    expect_true(any(grepl("^\\s*dk_dt = r\\b", lines)))
    # and no longer passed in as a constant, which would shadow the state
    expect_false(any(grepl('k = params\\[\\["k"\\]\\]', lines)))
    expect_true(any(grepl('r = params\\[\\["r"\\]\\]', lines)))

    # every other target integrates it too, and drops it from its parameters
    for (spec in list(c("mrgsolve", "^dxdt_k = r;",                 "^k = 0.5$"),
                      c("nlmixr2",  "^\\s*d/dt\\(k\\) <- r\\b",     "^\\s*k <- 0.5$"),
                      c("MATLAB",   "^\\s*dydt\\(2\\) = r;",        "^\\s*k = 0.5;$"),
                      c("Julia",    "^\\s*du\\[2\\] = r\\b",        "^\\s*k = p\\["),
                      c("ubiquity", "^<ODE:k> r",                   "^<P> k "))) {
        o <- tempfile()
        expect_no_warning(convertReactions(ramping_rate_model(), o, format = spec[1]))
        l <- readLines(o)
        expect_true(any(grepl(spec[2], l)), info = spec[1])
        expect_false(any(grepl(spec[3], l)), info = spec[1])
    }

    # Julia's p holds only r now, so r is p[1]
    o <- tempfile(fileext = ".jl")
    convertReactions(ramping_rate_model(), o, format = "Julia")
    l <- readLines(o)
    expect_true(any(grepl("^\\s*r = p\\[1\\]", l)))
    expect_true(any(grepl("^p = Float64\\[0.2\\]$", l)))
})

test_that("a ramping rate constant reproduces its analytic solution", {
    skip_if_not_installed("deSolve")

    times <- c(0, 1, 3, 7)
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(ramping_rate_model(), out, format = "R"))
    env <- new.env(); source(out, local = env)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters,
                                    rtol = 1e-10, atol = 1e-10))
    expect_equal(d$k, 0.5 + 0.2 * times, tolerance = 1e-8)
    expect_equal(d$A, 2 * exp(-(0.5 * times + 0.1 * times^2)), tolerance = 1e-7)
})

# --- values the model leaves unset ----------------------------------------
#
# An unset compartment size or parameter value reads back from libSBML as NaN,
# which operator<< prints as the literal `nan` -- a number in none of the six
# targets, so the generated R died at source() with "object 'nan' not found".
# The SBML Test Suite has 86 such cases.  Each kind of unset value has its own
# right answer, tested below.

# A minimal L3V2 model.  `cmt`, `species`, `params`, `rules` and `reactions`
# are spliced in verbatim, so each test states exactly what it relies on.
unset_value_model <- function(cmt, species, params = "", rules = "", reactions = "") {
    xml <- sprintf('<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>%s</listOfCompartments>
<listOfSpecies>%s</listOfSpecies>
<listOfParameters>%s</listOfParameters>
<listOfRules>%s</listOfRules>
<listOfReactions>%s</listOfReactions>
</model>
</sbml>', cmt, species, params, rules, reactions)
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

# First-order decay of A with rate law `law`.
decay_reaction <- function(law) sprintf(
'<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">%s</math></kineticLaw>
</reaction>', law)

# An assignment rule k = 0.5.
k_assignment <- '<assignmentRule variable="k"><math xmlns="http://www.w3.org/1998/Math/MathML">
<cn>0.5</cn></math></assignmentRule>'

# The lines of `lines` strictly between the first line matching `from` and the
# next line after it matching `to` -- one section of a generated file.
file_section <- function(lines, from, to) {
    i <- grep(from, lines)[1]
    if (is.na(i)) return(character())
    rest <- lines[-seq_len(i)]
    j <- grep(to, rest)[1]
    if (is.na(j)) rest else rest[seq_len(j - 1)]
}

# Generated output for every target, as a named list of character vectors.
all_targets <- function(sbml) {
    exts <- c(R = ".R", mrgsolve = ".cpp", nlmixr2 = ".R", MATLAB = ".m",
              Julia = ".jl", ubiquity = ".txt")
    lapply(setNames(names(exts), names(exts)), function(fmt) {
        o <- tempfile(fileext = exts[[fmt]])
        convertReactions(sbml, o, format = fmt)
        readLines(o)
    })
}

# Lowercase `nan` only: NaN and NAN are the deliberate spellings.
has_bare_nan <- function(lines) any(grepl("\\bnan\\b", lines))

zero_d_model <- function() unset_value_model(
    cmt = '<compartment id="C" spatialDimensions="0" constant="true"/>',
    species = '<species id="A" compartment="C" initialAmount="2" boundaryCondition="false"
               hasOnlySubstanceUnits="true" constant="false"/>',
    params = '<parameter id="k" value="0.5" constant="true"/>',
    reactions = decay_reaction("<apply><times/><ci>k</ci><ci>A</ci></apply>"))

test_that("a 0-dimensional compartment with no size is written as 1", {
    # It has no volume to have a value; the species in it count amounts, so
    # the compartment never enters the maths.  1 is what speciesInitialValue()
    # already converts through.
    out <- expect_no_warning(all_targets(zero_d_model()))
    for (fmt in names(out)) expect_false(has_bare_nan(out[[fmt]]), info = fmt)

    expect_true(any(grepl("^C = 1$", out$R)))
    expect_true(any(grepl("^double C = 1;$", file_section(out$mrgsolve, "^\\$MAIN", "^\\$ODE"))))
    expect_true(any(grepl("^\\s*C <- 1$", out$nlmixr2)))
    expect_true(any(grepl("^\\s*C = 1;$", out$MATLAB)))
    expect_true(any(grepl("^\\s*C = 1$", out$Julia)))
    expect_true(any(grepl("^<P> C 1 ", out$ubiquity)))
})

test_that("a 0-dimensional compartment reproduces its analytic solution", {
    skip_if_not_installed("deSolve")

    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(zero_d_model(), out, format = "R"))
    env <- new.env(); source(out, local = env)
    times <- c(0, 1, 3, 7)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters,
                                    rtol = 1e-10, atol = 1e-10))
    expect_equal(d$A, 2 * exp(-0.5 * times), tolerance = 1e-7)
})

# A decay whose rate constant k is set by an assignment rule.  `k_value` is the
# value attribute k declares, or NULL for none.
assigned_k_model <- function(k_value = NULL) unset_value_model(
    cmt = '<compartment id="c" size="1" spatialDimensions="3" constant="true"/>',
    species = '<species id="A" compartment="c" initialConcentration="2" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false"/>',
    params = sprintf('<parameter id="k" %s constant="false"/>',
                     if (is.null(k_value)) "" else sprintf('value="%s"', k_value)),
    rules = k_assignment,
    reactions = decay_reaction("<apply><times/><ci>k</ci><ci>A</ci><ci>c</ci></apply>"))

# k is recomputed in the RHS from its rule, so like an assignment-rule species
# it has no place among the constants -- every target's parameter list must
# leave it out, whatever value (if any) the model declares for it.
expect_assigned_k_dropped <- function(out) {
    for (fmt in names(out)) expect_false(has_bare_nan(out[[fmt]]), info = fmt)

    # R: not in parameters, not unpacked from params, but still assigned
    expect_false(any(grepl("^\\s*k =", file_section(out$R, "^parameters <- c\\($", "^\\s*\\)$"))))
    expect_false(any(grepl('k = params\\[\\["k"\\]\\]', out$R)))
    expect_true(any(grepl("^\\s*k = 0.5$", out$R)))

    # mrgsolve: not in $PARAM, still a local in $ODE
    expect_false(any(grepl("^k =", file_section(out$mrgsolve, "^\\$PARAM", "^\\$CMT"))))
    expect_true(any(grepl("^double k = 0.5;", out$mrgsolve)))

    # nlmixr2: not in ini()
    expect_false(any(grepl("^\\s*k <-", file_section(out$nlmixr2, "ini\\(\\{", "^\\s*\\}\\)"))))

    # MATLAB: not among the parameters
    expect_false(any(grepl("^\\s*k =", file_section(out$MATLAB, "% Parameters", "^\\s*$"))))

    # Julia: not read from p, and p does not carry it
    expect_false(any(grepl("^\\s*k = p\\[", out$Julia)))
    expect_true(any(grepl("^p = Float64\\[\\]$", out$Julia)))

    # ubiquity: a dynamic secondary parameter, not a <P>
    expect_false(any(grepl("^<P> k ", out$ubiquity)))
    expect_true(any(grepl("^<Ad> k = ", out$ubiquity)))
}

test_that("an unvalued assignment-rule parameter is left out of every parameter list", {
    expect_assigned_k_dropped(expect_no_warning(all_targets(assigned_k_model())))
})

test_that("an assignment-rule parameter is left out even when it declares a value", {
    # the declared 3 is overridden by the rule at every time point, so passing
    # it in as a constant would only offer a value the model never uses
    out <- expect_no_warning(all_targets(assigned_k_model(k_value = 3)))
    expect_assigned_k_dropped(out)
    expect_false(any(grepl("\\b3\\b", file_section(out$R, "^parameters <- c\\($", "^\\s*\\)$"))))
})

test_that("an assignment-rule parameter still drives the simulation", {
    skip_if_not_installed("deSolve")

    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(assigned_k_model(), out, format = "R"))
    env <- new.env(); source(out, local = env)
    times <- c(0, 1, 3, 7)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters,
                                    rtol = 1e-10, atol = 1e-10))
    expect_equal(d$A, 2 * exp(-0.5 * times), tolerance = 1e-7)
})

# A square DAE whose rate expressions read an assignment-rule parameter.  S
# decays at rate k; B is a non-constant boundary species with no rate rule, so
# it is the one undetermined state, and the algebraic rule 0 = B*B - 4*S*S fixes
# it.  B = 2*S would be solved as an assignment rule instead; squaring keeps
# this a DAE, with the same solution from the consistent start B = 2.
dae_assigned_k_model <- function() unset_value_model(
    cmt = '<compartment id="c" size="1" spatialDimensions="3" constant="true"/>',
    species = '<species id="S" compartment="c" initialConcentration="1" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false"/>
               <species id="B" compartment="c" initialConcentration="2" boundaryCondition="true"
               hasOnlySubstanceUnits="false" constant="false"/>',
    params = '<parameter id="k" constant="false"/>',
    rules = paste0(k_assignment,
        '<algebraicRule><math xmlns="http://www.w3.org/1998/Math/MathML">
         <apply><minus/><apply><times/><ci>B</ci><ci>B</ci></apply>
           <apply><times/><cn>4</cn><ci>S</ci><ci>S</ci></apply></apply>
         </math></algebraicRule>'),
    reactions = sub('species="A"', 'species="S"',
                    decay_reaction("<apply><times/><ci>k</ci><ci>S</ci><ci>c</ci></apply>")))

test_that("InitialDerivatives evaluates assignment rules the parameters no longer carry", {
    # InitialDerivatives is computed at file level from InitialAmounts and
    # parameters alone; once k leaves `parameters`, its assignment rule has to
    # be evaluated there too, or this fails with "object 'k' not found".
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(dae_assigned_k_model(), out, format = "R"))
    expect_false(has_bare_nan(readLines(out)))
    env <- new.env()
    expect_no_error(source(out, local = env))
    expect_true(is.numeric(env$InitialDerivatives))
    expect_true(all(is.finite(env$InitialDerivatives)))
    expect_equal(unname(env$InitialDerivatives[["S"]]), -0.5)

    skip_if_not_installed("deSolve")
    times <- seq(0, 5, by = 1)
    d <- as.data.frame(deSolve::daspk(y = env$InitialAmounts, dy = env$InitialDerivatives,
                                      times = times, res = env$massBalances,
                                      parms = env$parameters))
    expect_equal(nrow(d), length(times))
    expect_equal(d$S, exp(-0.5 * times), tolerance = 1e-5)
    expect_lt(max(abs(d$B - 2 * d$S)), 1e-6)   # the constraint holds throughout
})

test_that("a value nothing sets is written as NaN, with a warning naming it", {
    # No rule, initial assignment or size gives these a value, so there is no
    # right number to write.  1 would be a plausible wrong answer; each target's
    # own not-a-number keeps the file valid and the warning says why.
    unsized <- unset_value_model(
        cmt = '<compartment id="cell" spatialDimensions="3" constant="true"/>',
        species = '<species id="A" compartment="cell" initialAmount="1" boundaryCondition="false"
                   hasOnlySubstanceUnits="false" constant="false"/>',
        params = '<parameter id="k" value="0.5" constant="true"/>',
        reactions = decay_reaction("<apply><times/><ci>k</ci><ci>A</ci><ci>cell</ci></apply>"))
    unvalued <- unset_value_model(
        cmt = '<compartment id="c" size="1" spatialDimensions="3" constant="true"/>',
        species = '<species id="A" compartment="c" initialAmount="1" boundaryCondition="false"
                   hasOnlySubstanceUnits="false" constant="false"/>',
        params = '<parameter id="kx" constant="true"/>',
        reactions = decay_reaction("<apply><times/><ci>kx</ci><ci>A</ci><ci>c</ci></apply>"))

    exts <- c(R = ".R", mrgsolve = ".cpp", nlmixr2 = ".R", MATLAB = ".m",
              Julia = ".jl", ubiquity = ".txt")
    gen <- function(sbml, id) {
        lapply(setNames(names(exts), names(exts)), function(fmt) {
            o <- tempfile(fileext = exts[[fmt]])
            expect_warning(convertReactions(sbml, o, format = fmt), id, info = fmt)
            readLines(o)
        })
    }

    cmt <- gen(unsized, "cell")
    for (fmt in names(cmt)) expect_false(has_bare_nan(cmt[[fmt]]), info = fmt)
    expect_true(any(grepl("^cell = NaN$", cmt$R)))
    expect_true(any(grepl("^double cell = NAN;$", file_section(cmt$mrgsolve, "^\\$MAIN", "^\\$ODE"))))
    expect_true(any(grepl("^\\s*cell <- NaN$", cmt$nlmixr2)))
    expect_true(any(grepl("^\\s*cell = NaN;$", cmt$MATLAB)))
    expect_true(any(grepl("^\\s*cell = NaN$", cmt$Julia)))
    expect_true(any(grepl("^<P> cell NaN ", cmt$ubiquity)))

    par <- gen(unvalued, "kx")
    for (fmt in names(par)) expect_false(has_bare_nan(par[[fmt]]), info = fmt)
    expect_true(any(grepl("^\\s*kx = NaN$", file_section(par$R, "^parameters <- c\\($", "^\\s*\\)$"))))
    expect_true(any(grepl("^kx = NaN$", file_section(par$mrgsolve, "^\\$PARAM", "^\\$CMT"))))
    expect_true(any(grepl("^\\s*kx <- NaN$", par$nlmixr2)))
    expect_true(any(grepl("^\\s*kx = NaN;$", par$MATLAB)))
    expect_true(any(grepl("^p = Float64\\[NaN\\]$", par$Julia)))
    expect_true(any(grepl("^<P> kx NaN ", par$ubiquity)))
})
