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

        defined  <- sub("^\\s*d(.*)_dt\\s*=.*$", "\\1",
                        grep("^\\s*d.*_dt\\s*=", lines, value = TRUE))
        returned <- sub("^\\s*d(.*)_dt\\s*,?\\s*$", "\\1",
                        grep("^\\s*d.*_dt\\s*,?\\s*$", lines, value = TRUE))
        expect_setequal(defined, returned)

        # and the RHS evaluates: an undefined derivative errors here
        env <- new.env()
        env$library <- function(...) invisible(NULL)   # deSolve need not be installed
        source(out, local = env)
        d <- env$massBalances(0, env$InitialAmounts, env$parameters)[[1]]
        expect_length(d, length(env$InitialAmounts))
        expect_false(any(is.na(d)))
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
