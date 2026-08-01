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
