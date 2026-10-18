# Assignment-rule variables are computed as locals inside each target's RHS,
# so none of them is in the solver's state vector.  Until they were returned
# explicitly, their trajectories could not be read from any solution -- the
# largest single group of SBML Test Suite failures (84 cases).  These tests
# pin down how each target now exposes them.

assignment_rules_file <- function() {
    f <- system.file("examples", "sbmlassignmentrules.xml", package = "r2sbml")
    if (f == "") f <- "../../inst/examples/sbmlassignmentrules.xml"
    f
}

simple_file <- function() {
    f <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
    if (f == "") f <- "../../inst/examples/sbmlsimple.xml"
    f
}

convert_lines <- function(sbml, fmt, ext = ".txt") {
    o <- tempfile(fileext = ext)
    convertReactions(sbml, o, format = fmt)
    readLines(o)
}

test_that("deSolve returns each assignment-rule variable as a solution column", {
    skip_if_not_installed("deSolve")

    out <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(assignment_rules_file(), out, format = "R"))
    expect_true(any(grepl("return\\(list\\(MassBalances, c\\(S1 = S1, S2 = S2\\)\\)\\)",
                          readLines(out))))

    env <- new.env(); source(out, local = env)
    times <- c(0, 1, 5, 20)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters,
                                    rtol = 1e-10, atol = 1e-12))
    # the rules are S1 = T / (1 + Keq) and S2 = Keq * S1, with Keq = 2.5;
    # the columns must follow them along the whole trajectory, not just at t = 0
    expect_true(all(c("S1", "S2") %in% names(d)))
    expect_equal(d$S1, d$T / 3.5, tolerance = 1e-12)
    expect_equal(d$S2, 2.5 * d$S1, tolerance = 1e-12)
    expect_gt(d$S1[4], 0)
})

test_that("a model with no assignment rules keeps the old return shape", {
    out <- convert_lines(simple_file(), "R", ".R")
    expect_true(any(grepl("^\\s*return\\(list\\(MassBalances\\)\\)$", out)))

    mrg <- convert_lines(simple_file(), "mrgsolve", ".cpp")
    expect_false(any(grepl("^\\$TABLE|^\\$CAPTURE", mrg)))
    expect_false(any(grepl("observables", convert_lines(simple_file(), "MATLAB", ".m"))))
    expect_false(any(grepl("observables", convert_lines(simple_file(), "Julia", ".jl"))))
})

test_that("every other target exposes the assignment-rule variables", {
    f <- assignment_rules_file()

    # mrgsolve: recomputed in $TABLE from the output-time states, then captured
    mrg <- convert_lines(f, "mrgsolve", ".cpp")
    table <- mrg[seq(grep("^\\$TABLE$", mrg), length(mrg))]
    expect_true(any(grepl("^S1 = T / \\(1 \\+ Keq\\);$", table)))
    expect_true(any(grepl("^\\$CAPTURE S1 S2$", mrg)))

    # rxode2: plain assignments in model() are already output columns
    rx <- convert_lines(f, "nlmixr2", ".R")
    expect_true(any(grepl("^\\s*S1 <- T / \\(1 \\+ Keq\\)$", rx)))

    # MATLAB: a third output, filled row by row from the same rules
    m <- convert_lines(f, "MATLAB", ".m")
    expect_true(any(grepl("^function \\[t, y, r2sbml_obs\\] = ", m)))
    expect_true(any(grepl("^\\s*function r2sbml_row = observables\\(time, states\\)$", m)))
    expect_true(any(grepl("^\\s*r2sbml_row = \\[S1, S2\\];$", m)))
    expect_true(any(grepl("Columns of obs.*: S1, S2$", m)))

    # Julia: a function returning a NamedTuple
    jl <- convert_lines(f, "Julia", ".jl")
    expect_true(any(grepl("^function observables\\(u, p, t\\)$", jl)))
    expect_true(any(grepl("^\\s*return \\(S1 = S1, S2 = S2\\)$", jl)))

    # ubiquity: outputs over the <Ad> secondary parameters
    ub <- convert_lines(f, "ubiquity", ".txt")
    expect_true(any(grepl("^<O> S1_out = S1$", ub)))
    expect_true(any(grepl("^<O> S2_out = S2$", ub)))
})

test_that("mrgsolve and rxode2 solutions carry the assignment-rule columns", {
    skip_on_cran()
    f <- assignment_rules_file()

    if (requireNamespace("mrgsolve", quietly = TRUE)) {
        d <- tempfile(); dir.create(d)
        convertReactions(f, file.path(d, "ar.cpp"), format = "mrgsolve")
        m <- mrgsolve::mread("ar", project = d, file = "ar.cpp", quiet = TRUE)
        s <- mrgsolve::mrgsim_df(m, end = 5, delta = 1)
        expect_equal(s$S1, s$T / 3.5, tolerance = 1e-10)
        expect_equal(s$S2, 2.5 * s$S1, tolerance = 1e-10)
    }

    if (requireNamespace("rxode2", quietly = TRUE)) {
        o <- tempfile(fileext = ".R")
        convertReactions(f, o, format = "nlmixr2")
        env <- new.env(); source(o, local = env)
        rx <- suppressMessages(rxode2::rxode2(env$model))
        s <- as.data.frame(suppressMessages(
            rxode2::rxSolve(rx, events = rxode2::et(c(0, 1, 5)))))
        expect_equal(s$S1, s$T / 3.5, tolerance = 1e-10)
    }
})

# A DAE that also carries an assignment rule: S decays, B is fixed by the
# algebraic rule B*B = 4*S*S (squared so that it stays a DAE rather than being
# solved as B = 2 S), and Y = 3 S is an assignment.  daspk takes the same
# list-of-extra-outputs return as ode, so Y gets a column here too.
dae_with_assignment_model <- function() {
    xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>
<compartment id="c" size="1" spatialDimensions="3" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="S" compartment="c" initialConcentration="1" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
<species id="B" compartment="c" initialConcentration="2" boundaryCondition="true"
hasOnlySubstanceUnits="false" constant="false"/>
<species id="Y" compartment="c" initialConcentration="3" boundaryCondition="true"
hasOnlySubstanceUnits="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="k" value="0.5" constant="true"/>
</listOfParameters>
<listOfRules>
<algebraicRule><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><minus/><apply><times/><ci>B</ci><ci>B</ci></apply><apply><times/><cn>4</cn><ci>S</ci><ci>S</ci></apply></apply></math></algebraicRule>
<assignmentRule variable="Y"><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><cn>3</cn><ci>S</ci></apply></math></assignmentRule>
</listOfRules>
<listOfReactions>
<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="S" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>k</ci><ci>S</ci><ci>c</ci></apply></math></kineticLaw>
</reaction>
</listOfReactions>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

test_that("the DAE residual function returns assignment-rule outputs too", {
    skip_if_not_installed("deSolve")

    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(dae_with_assignment_model(), out, format = "R"))
    expect_true(any(grepl("return\\(list\\(Residuals, c\\(Y = Y\\)\\)\\)", readLines(out))))

    env <- new.env(); source(out, local = env)
    times <- c(0, 1, 2, 4)
    d <- as.data.frame(deSolve::daspk(y = env$InitialAmounts, dy = env$InitialDerivatives,
                                      times = times, res = env$massBalances,
                                      parms = env$parameters))
    expect_equal(d$S, exp(-0.5 * times), tolerance = 1e-5)
    expect_equal(d$Y, 3 * d$S, tolerance = 1e-10)
    expect_equal(d$B, 2 * d$S, tolerance = 1e-6)
})

# Every variable follows an assignment rule, A = 1 + t, so nothing is
# integrated.  An empty state vector is accepted by no solver -- deSolve
# stops with "`y' must be numeric" -- although the trajectory is perfectly
# well defined.
rules_only_model <- function() {
    xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>
<compartment id="c" size="1" spatialDimensions="3" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="A" compartment="c" initialConcentration="1" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
</listOfSpecies>
<listOfRules>
<assignmentRule variable="A"><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><plus/><cn>1</cn>
<csymbol encoding="text" definitionURL="http://www.sbml.org/sbml/symbols/time"> t </csymbol>
</apply></math></assignmentRule>
</listOfRules>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

test_that("a model with nothing to integrate gets one placeholder state", {
    f <- rules_only_model()
    for (spec in list(c("R",        ".R",   "^\\s*r2sbml_placeholder = 0\\b"),
                      c("mrgsolve", ".cpp", "^r2sbml_placeholder$"),
                      c("nlmixr2",  ".R",   "^\\s*r2sbml_placeholder\\(0\\) <- 0$"),
                      c("MATLAB",   ".m",   "% r2sbml_placeholder \\(placeholder state"),
                      c("Julia",    ".jl",  "^u0 = Float64\\[0\\]$"),
                      c("ubiquity", ".txt", "^<ODE:r2sbml_placeholder> 0\\b"))) {
        l <- convert_lines(f, spec[1], spec[2])
        expect_true(any(grepl(spec[3], l)), info = spec[1])
    }

    skip_if_not_installed("deSolve")
    out <- tempfile(fileext = ".R")
    convertReactions(f, out, format = "R")
    env <- new.env(); source(out, local = env)
    times <- c(0, 1, 2.5)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters))
    expect_equal(d$A, 1 + times)
    expect_equal(d$r2sbml_placeholder, rep(0, length(times)))
})
