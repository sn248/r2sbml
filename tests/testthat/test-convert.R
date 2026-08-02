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

    # nlmixr2 / rxode
    out_file_rx <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(sbml_file, out_file_rx, format = "nlmixr2"))
    lines_rx <- readLines(out_file_rx)
    expect_true(any(grepl("model <- function", lines_rx)))
    expect_true(any(grepl("ini\\(\\{", lines_rx)))

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
