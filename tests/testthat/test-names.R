# --- SBML ids the targets cannot use, and chained relationals ----------------
#
# An SBML id may be a reserved word or constant in the target (`Inf`, `TRUE`),
# or a name the generated code uses itself (`time`, Julia's `p`).  Each is
# renamed with a trailing underscore; deSolve output still carries the SBML id.
# And MathML's n-ary relationals are chains: gt(a, b, c) is a > b && b > c.

# TRUE decays into time at rate rate * Inf = 1, so TRUE = exp(-t) and
# time = 1 - exp(-t); NaN = 3 * TRUE and y = 1, since 2 > 1 > 0.
names_model <- function() {
    xml <- '<?xml version="1.0" encoding="UTF-8"?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model id="names">
<listOfCompartments>
<compartment id="c" spatialDimensions="3" size="1" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="TRUE" compartment="c" initialAmount="1" hasOnlySubstanceUnits="true"
boundaryCondition="false" constant="false"/>
<species id="time" compartment="c" initialAmount="0" hasOnlySubstanceUnits="true"
boundaryCondition="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="Inf" value="2" constant="true"/>
<parameter id="p" value="3" constant="true"/>
<parameter id="rate" value="0.5" constant="true"/>
<parameter id="NaN" constant="false"/>
<parameter id="y" constant="false"/>
</listOfParameters>
<listOfRules>
<assignmentRule variable="NaN"><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>TRUE</ci><ci>p</ci></apply></math></assignmentRule>
<assignmentRule variable="y"><math xmlns="http://www.w3.org/1998/Math/MathML">
<piecewise><piece><cn>1</cn><apply><gt/><ci>Inf</ci><cn>1</cn><cn>0</cn></apply></piece>
<otherwise><cn>0</cn></otherwise></piecewise></math></assignmentRule>
</listOfRules>
<listOfReactions>
<reaction id="J0" reversible="false">
<listOfReactants><speciesReference species="TRUE" stoichiometry="1" constant="true"/></listOfReactants>
<listOfProducts><speciesReference species="time" stoichiometry="1" constant="true"/></listOfProducts>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>rate</ci><ci>Inf</ci><ci>TRUE</ci></apply></math></kineticLaw>
</reaction>
</listOfReactions>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

convert_to <- function(sbml, format, ext) {
    o <- tempfile(fileext = ext)
    utils::capture.output(convertReactions(sbml, o, format = format))
    readLines(o)
}

test_that("reserved SBML ids are renamed, and deSolve keeps the SBML ids", {
    skip_if_not_installed("deSolve")
    sbml <- names_model()
    o <- tempfile(fileext = ".R")
    msg <- utils::capture.output(convertReactions(sbml, o, format = "R"))
    expect_true(any(grepl("Renamed Inf to Inf_", msg)))

    env <- new.env()
    sys.source(o, envir = env)
    tt  <- c(0, 1, 2)
    sol <- as.data.frame(deSolve::ode(env$InitialAmounts, tt, env$massBalances,
                                      env$parameters, atol = 1e-10, rtol = 1e-10))
    expect_named(env$parameters, c("Inf", "p", "rate"))
    expect_equal(names(sol), c("time", "TRUE", "time", "y", "NaN"))
    expect_equal(sol[[1]], tt)
    expect_equal(sol[[2]], exp(-tt), tolerance = 1e-6)
    expect_equal(sol[[3]], 1 - exp(-tt), tolerance = 1e-6)
    expect_equal(sol$y, rep(1, 3))
    expect_equal(sol[["NaN"]], 3 * exp(-tt), tolerance = 1e-6)
})

test_that("each target renames the names it reserves", {
    sbml <- names_model()
    jl <- convert_to(sbml, "Julia", ".jl")
    expect_true(any(grepl("^    p_ = p\\[2\\]$", jl)))
    expect_false(any(grepl("^    p = p\\[", jl)))

    m <- convert_to(sbml, "MATLAB", ".m")
    expect_true(any(grepl("^        y_ = ", m)))      # y is the solution output

    mrg <- convert_to(sbml, "mrgsolve", ".cpp")
    expect_true(all(c("Inf_ = 2", "rate_ = 0.5") %in% mrg))   # mrgsolve reserves rate

    ub <- convert_to(sbml, "ubiquity", ".txt")
    expect_true(any(grepl("^<P> Inf_ 2 ", ub)))
})

test_that("an n-ary relational is the AND of its adjacent pairs", {
    sbml <- names_model()
    expect_true(any(grepl("(Inf_ > 1) && (1 > 0)", convert_to(sbml, "MATLAB", ".m"), fixed = TRUE)))
    expect_true(any(grepl("(Inf_ > 1.0) && (1.0 > 0.0)", convert_to(sbml, "mrgsolve", ".cpp"), fixed = TRUE)))
    expect_true(any(grepl("SIMINT_AND[SIMINT_GT[Inf_][1]][SIMINT_GT[1][0]]",
                          convert_to(sbml, "ubiquity", ".txt"), fixed = TRUE)))
})

test_that("mrgsolve and rxode2 compile a model with renamed ids", {
    skip_on_cran()
    skip_if_not_installed("mrgsolve")
    skip_if_not_installed("rxode2")
    sbml <- names_model()
    tt <- c(0, 1, 2)

    code <- paste(convert_to(sbml, "mrgsolve", ".cpp"), collapse = "\n")
    mod  <- suppressMessages(mrgsolve::mcode("r2sbml_names", code, quiet = TRUE))
    out  <- as.data.frame(mrgsolve::mrgsim_df(mod, tgrid = tt))
    expect_equal(out$TRUE_, exp(-tt), tolerance = 1e-5)
    expect_equal(out$y, rep(1, 3))

    rf <- tempfile(fileext = ".R")
    utils::capture.output(convertReactions(sbml, rf, format = "rxode2"))
    env <- new.env()
    sys.source(rf, envir = env)
    rx  <- suppressMessages(rxode2::rxode2(env$model))
    out <- as.data.frame(suppressMessages(rxode2::rxSolve(rx, rxode2::et(tt))))
    expect_equal(out$TRUE_, exp(-tt), tolerance = 1e-5)
})

test_that("an initial assignment to NaN is expanded rather than failing", {
    xml <- '<?xml version="1.0" encoding="UTF-8"?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model id="nan_ia">
<listOfParameters>
<parameter id="P" value="0" constant="true"/>
<parameter id="R" value="0" constant="true"/>
</listOfParameters>
<listOfInitialAssignments>
<initialAssignment symbol="P"><math xmlns="http://www.w3.org/1998/Math/MathML"><infinity/></math></initialAssignment>
<initialAssignment symbol="R"><math xmlns="http://www.w3.org/1998/Math/MathML"><notanumber/></math></initialAssignment>
</listOfInitialAssignments>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    o <- tempfile(fileext = ".R")
    utils::capture.output(suppressWarnings(convertReactions(f, o, format = "R")))
    env <- new.env()
    sys.source(o, envir = env)
    expect_identical(env$parameters[["P"]], Inf)
    expect_true(is.nan(env$parameters[["R"]]))
})
