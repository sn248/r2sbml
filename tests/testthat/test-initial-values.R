# Three ways convertReactions used to integrate the wrong thing while the
# generated code ran without complaint, all found by the SBML Test Suite.  The
# code runs either way, so each is checked against a closed-form solution
# rather than against the text of the generated file.

# A minimal L3V2 model; every argument is spliced in verbatim.
l3_model <- function(cmt, species, params = "", rules = "", reactions = "",
                     ias = "", model_attrs = "") {
    xml <- sprintf('<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole" %s>
<listOfCompartments>%s</listOfCompartments>
<listOfSpecies>%s</listOfSpecies>
<listOfParameters>%s</listOfParameters>
<listOfInitialAssignments>%s</listOfInitialAssignments>
<listOfRules>%s</listOfRules>
<listOfReactions>%s</listOfReactions>
</model>
</sbml>', model_attrs, cmt, species, params, ias, rules, reactions)
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

mathml <- function(x) sprintf('<math xmlns="http://www.w3.org/1998/Math/MathML">%s</math>', x)

# Convert to deSolve, source, solve with ode() and return the solution.
solve_r <- function(sbml, times) {
    out <- tempfile(fileext = ".R")
    convertReactions(sbml, out, format = "R")
    env <- new.env(); source(out, local = env)
    as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                               func = env$massBalances, parms = env$parameters,
                               rtol = 1e-10, atol = 1e-12))
}

times <- c(0, 0.5, 1, 2, 4)

# --- the initial volume of an assignment-rule compartment ---------------------
#
# Case 00143: size 5 is declared, but the rule `compartment = 1` holds from
# t = 0, so an initial *amount* has to be converted through 1, not 5.  Here the
# declared size is 5 and the rule gives 2, so [A](0) = 4 / 2 = 2, and first-
# order decay gives [A] = 2 exp(-k t).
assigned_volume_model <- function(rule) l3_model(
    cmt = '<compartment id="c" size="5" spatialDimensions="3" constant="false"/>
           <compartment id="d" size="1" spatialDimensions="3" constant="true"/>',
    species = '<species id="A" compartment="c" initialAmount="4" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false"/>
               <species id="B" compartment="d" initialAmount="1" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="true"/>',
    params = '<parameter id="k" value="0.5" constant="true"/>
              <parameter id="p" value="1" constant="true"/>',
    rules = sprintf('<assignmentRule variable="c">%s</assignmentRule>', mathml(rule)),
    reactions = sprintf('<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw>%s</kineticLaw></reaction>',
        mathml("<apply><times/><ci>k</ci><ci>A</ci><ci>c</ci></apply>")))

test_that("an assignment-rule compartment converts initial amounts through the rule's value", {
    for (rule in c("<cn>2</cn>",                                    # a constant
                   "<apply><times/><cn>2</cn><ci>p</ci></apply>")) {  # a parameter
        sbml <- assigned_volume_model(rule)
        out <- tempfile(fileext = ".R")
        suppressWarnings(convertReactions(sbml, out, format = "R"))
        expect_true(any(grepl("^\\s*A = 2\\b", readLines(out))), info = rule)

        # every target starts from the same value
        for (spec in list(c("mrgsolve", "^A_0 = 2;"), c("nlmixr2", "^\\s*A\\(0\\) <- 2$"),
                          c("MATLAB", "^\\s*2; % A$"), c("Julia", "^u0 = Float64\\[2, "),
                          c("ubiquity", "^<I> A = 2$"))) {
            o <- tempfile()
            suppressWarnings(convertReactions(sbml, o, format = spec[1]))
            expect_true(any(grepl(spec[2], readLines(o))), info = paste(rule, spec[1]))
        }
    }
})

test_that("an assignment-rule compartment reproduces its analytic solution", {
    skip_if_not_installed("deSolve")
    d <- suppressWarnings(solve_r(assigned_volume_model("<cn>2</cn>"), times))
    expect_equal(d$A, 2 * exp(-0.5 * times), tolerance = 1e-7)
})

test_that("a compartment rule that reads a species falls back to the declared size, and says so", {
    # a species value is in the units the file declares, not the ones being
    # integrated, so the rule is not evaluated rather than evaluated wrongly
    sbml <- assigned_volume_model("<apply><plus/><cn>1</cn><ci>B</ci></apply>")
    expect_warning(convertReactions(sbml, tempfile(), format = "R"),
                   "could not be evaluated at t = 0")
})

# --- stoichiometry a rule sets ------------------------------------------------
#
# Case 00971: a speciesReference declares stoichiometry 1 but its id Xref is
# set by a rule.  libSBML used the declared 1 whenever one was present.  With
# the zeroth-order source  -> n X  at rate k = 1, X(0) = 0:
#   assignment rule  Xref = 3        X = 3 t
#   initial assign.  Xref = 2        X = 2 t
#   rate rule        dXref/dt = 1    X = t + t^2 / 2   (Xref starts at the declared 1)
#   assignment rule  Xref = time     X = t^2 / 2
stoich_model <- function(rules = "", ias = "") l3_model(
    cmt = '<compartment id="c" size="1" spatialDimensions="3" constant="true"/>',
    species = '<species id="X" compartment="c" initialAmount="0" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false"/>',
    params = '<parameter id="k" value="1" constant="true"/>',
    rules = rules, ias = ias,
    reactions = sprintf('<reaction id="make" reversible="false">
<listOfProducts><speciesReference id="Xref" species="X" stoichiometry="1" constant="false"/></listOfProducts>
<kineticLaw>%s</kineticLaw></reaction>', mathml("<ci>k</ci>")))

time_csymbol <- '<csymbol encoding="text" definitionURL="http://www.sbml.org/sbml/symbols/time">t</csymbol>'

stoich_cases <- list(
    assigned = list(model = function() stoich_model(rules = sprintf(
                        '<assignmentRule variable="Xref">%s</assignmentRule>', mathml("<cn>3</cn>"))),
                    X = function(t) 3 * t),
    initial  = list(model = function() stoich_model(ias = sprintf(
                        '<initialAssignment symbol="Xref">%s</initialAssignment>', mathml("<cn>2</cn>"))),
                    X = function(t) 2 * t),
    rated    = list(model = function() stoich_model(rules = sprintf(
                        '<rateRule variable="Xref">%s</rateRule>', mathml("<cn>1</cn>"))),
                    X = function(t) t + t^2 / 2),
    timed    = list(model = function() stoich_model(rules = sprintf(
                        '<assignmentRule variable="Xref">%s</assignmentRule>', mathml(time_csymbol))),
                    X = function(t) t^2 / 2))

test_that("a rule-set stoichiometry reaches the generated derivative", {
    for (nm in names(stoich_cases)) {
        sbml <- stoich_cases[[nm]]$model()
        out <- tempfile(fileext = ".R")
        expect_no_warning(convertReactions(sbml, out, format = "R"))
        l <- readLines(out)
        # the declared 1 must not survive as the whole derivative
        expect_false(any(grepl("^\\s*dX_dt = k$", l)), info = nm)
        # every other target converts too
        for (fmt in c("mrgsolve", "nlmixr2", "MATLAB", "Julia", "ubiquity")) {
            expect_no_warning(convertReactions(sbml, tempfile(), format = fmt))
        }
    }
})

test_that("a rule-set stoichiometry reproduces its analytic solution", {
    skip_if_not_installed("deSolve")
    for (nm in names(stoich_cases)) {
        d <- solve_r(stoich_cases[[nm]]$model(), times)
        expect_equal(d$X, stoich_cases[[nm]]$X(times), tolerance = 1e-7, info = nm)
    }
})

test_that("a rate-rule stoichiometry is integrated from its declared value", {
    sbml <- stoich_cases$rated$model()
    out <- tempfile(fileext = ".R")
    convertReactions(sbml, out, format = "R")
    l <- readLines(out)
    expect_true(any(grepl("^\\s*Xref = 1\\b.*parameter", l)))
    expect_true(any(grepl("^\\s*dXref_dt = 1\\b", l)))
    # the placeholder used to steer libSBML must not be left behind
    expect_false(any(grepl("^\\s*Xref = Xref$", l)))
})

# --- conversion factors -------------------------------------------------------
#
# Cases 00975-00977: in Level 3 a reaction changes a species by
# stoichiometry * rate * conversionFactor, using the species' own factor, else
# the model's.  libSBML's replaceReactions ignored both.  With A -> B at rate
# k*A, A's own factor 2 and the model's factor 4 applying to B:
#   dA/dt = -2 k A           A = exp(-2 k t)
#   dB/dt = +4 k A           B = 2 (1 - exp(-2 k t))
conversion_model <- function() l3_model(
    model_attrs = 'conversionFactor="modelconv"',
    cmt = '<compartment id="c" size="1" spatialDimensions="3" constant="true"/>',
    species = '<species id="A" compartment="c" initialAmount="1" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false" conversionFactor="Aconv"/>
               <species id="B" compartment="c" initialAmount="0" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false"/>',
    params = '<parameter id="k" value="0.5" constant="true"/>
              <parameter id="Aconv" value="2" constant="true"/>
              <parameter id="modelconv" value="4" constant="true"/>',
    reactions = sprintf('<reaction id="r" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<listOfProducts><speciesReference species="B" stoichiometry="1" constant="true"/></listOfProducts>
<kineticLaw>%s</kineticLaw></reaction>', mathml("<apply><times/><ci>k</ci><ci>A</ci></apply>")))

test_that("conversion factors scale each species' reaction terms", {
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(conversion_model(), out, format = "R"))
    l <- readLines(out)
    expect_true(any(grepl("^\\s*dA_dt = Aconv \\* \\(", l)))
    expect_true(any(grepl("^\\s*dB_dt = modelconv \\* \\(", l)))
    for (spec in list(c("mrgsolve", "^dxdt_A = Aconv \\* \\("),
                      c("nlmixr2",  "^\\s*d/dt\\(A\\) <- Aconv \\* \\("),
                      c("MATLAB",   "^\\s*dydt\\(1\\) = Aconv \\* \\("),
                      c("Julia",    "^\\s*du\\[1\\] = Aconv \\* \\("),
                      c("ubiquity", "^<ODE:A> \\(Aconv\\*"))) {
        o <- tempfile()
        expect_no_warning(convertReactions(conversion_model(), o, format = spec[1]))
        expect_true(any(grepl(spec[2], readLines(o))), info = spec[1])
    }
})

test_that("conversion factors reproduce their analytic solution", {
    skip_if_not_installed("deSolve")
    d <- solve_r(conversion_model(), times)
    expect_equal(d$A, exp(-times), tolerance = 1e-7)
    expect_equal(d$B, 2 * (1 - exp(-times)), tolerance = 1e-7)
})
