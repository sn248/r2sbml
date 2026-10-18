# More places where replaceReactions writes the wrong reaction term, or none,
# all found by the SBML Test Suite.  As in test-initial-values.R, the
# generated code runs either way, so each is checked against a closed form.

# A minimal L3V2 model; every argument is spliced in verbatim.
l3_model <- function(cmt, species = "", params = "", rules = "", reactions = "",
                     ias = "") {
    xml <- sprintf('<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>%s</listOfCompartments>
<listOfSpecies>%s</listOfSpecies>
<listOfParameters>%s</listOfParameters>
<listOfInitialAssignments>%s</listOfInitialAssignments>
<listOfRules>%s</listOfRules>
<listOfReactions>%s</listOfReactions>
</model>
</sbml>', cmt, species, params, ias, rules, reactions)
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

mathml <- function(x) sprintf('<math xmlns="http://www.w3.org/1998/Math/MathML">%s</math>', x)

# Convert to deSolve and source; returns the environment.
source_r <- function(sbml) {
    out <- tempfile(fileext = ".R")
    convertReactions(sbml, out, format = "R")
    env <- new.env(); source(out, local = env)
    env
}

solve_r <- function(sbml, times) {
    env <- source_r(sbml)
    as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                               func = env$massBalances, parms = env$parameters,
                               rtol = 1e-10, atol = 1e-12))
}

times <- c(0, 0.5, 1, 2, 4)

species_A <- function(cmt = "C", attrs = 'initialConcentration="10"') sprintf(
    '<species id="A" compartment="%s" %s boundaryCondition="false"
              hasOnlySubstanceUnits="false" constant="false"/>', cmt, attrs)

# A zero-order reaction at rate 0.5 with the given reactant references.
zero_order <- function(refs) sprintf(
'<reaction id="J0" reversible="false">
<listOfReactants>%s</listOfReactants>
<kineticLaw>%s</kineticLaw>
</reaction>', refs, mathml("<cn>0.5</cn>"))

# --- a species referenced more than once by one reaction ----------------------

test_that("repeated references to one species add their stoichiometries", {
    skip_if_not_installed("deSolve")
    m <- l3_model(
        cmt = '<compartment id="C" size="1" spatialDimensions="3" constant="true"/>',
        species = species_A(),
        reactions = zero_order(
            '<speciesReference species="A" stoichiometry="1" constant="true"/>
             <speciesReference species="A" stoichiometry="2" constant="true"/>'))
    expect_equal(solve_r(m, times)$A, 10 - 3 * 0.5 * times, tolerance = 1e-8)
})

test_that("repeated references whose stoichiometry is assigned add too", {
    skip_if_not_installed("deSolve")
    m <- l3_model(
        cmt = '<compartment id="C" size="1" spatialDimensions="3" constant="true"/>',
        species = species_A(),
        ias = paste(sprintf('<initialAssignment symbol="%s">%s</initialAssignment>',
                            c("s1", "s2"), mathml(c("<cn>2</cn>", "<cn>1</cn>"))),
                    collapse = ""),
        reactions = zero_order(
            '<speciesReference id="s1" species="A" stoichiometry="4" constant="true"/>
             <speciesReference id="s2" species="A" stoichiometry="2" constant="true"/>'))
    expect_equal(solve_r(m, times)$A, 10 - 3 * 0.5 * times, tolerance = 1e-8)
})

# --- dilution: what is conserved is the amount ---------------------------------

growing <- '<compartment id="C" size="1" spatialDimensions="3" constant="false"/>'
grow_rule <- sprintf('<rateRule variable="C">%s</rateRule>', mathml("<cn>1</cn>"))

test_that("a constant species is diluted by a growing compartment", {
    skip_if_not_installed("deSolve")
    m <- l3_model(cmt = growing, rules = grow_rule,
                  species = '<species id="S" compartment="C" initialConcentration="1"
                             boundaryCondition="false" hasOnlySubstanceUnits="false"
                             constant="true"/>')
    d <- solve_r(m, times)
    expect_equal(d$S * d$C, rep(1, length(times)), tolerance = 1e-8)
})

test_that("a species' own rate rule is not diluted", {
    skip_if_not_installed("deSolve")
    m <- l3_model(cmt = growing,
                  rules = paste0(grow_rule, sprintf('<rateRule variable="S">%s</rateRule>',
                                                    mathml("<cn>0.4</cn>"))),
                  species = '<species id="S" compartment="C" initialConcentration="0"
                             boundaryCondition="true" hasOnlySubstanceUnits="false"
                             constant="false"/>')
    expect_equal(solve_r(m, times)$S, 0.4 * times, tolerance = 1e-8)
})

# --- when libSBML leaves out the division by the volume -------------------------

test_that("a declared size of 1 under an initial assignment still divides", {
    skip_if_not_installed("deSolve")
    # libSBML skips `/ C` for a constant compartment of declared size 1, even
    # though the initial assignment makes it 2.
    m <- l3_model(
        cmt = '<compartment id="C" size="1" spatialDimensions="3" constant="true"/>',
        ias = sprintf('<initialAssignment symbol="C">%s</initialAssignment>', mathml("<cn>2</cn>")),
        species = species_A(attrs = 'initialAmount="4"'),
        reactions = zero_order('<speciesReference species="A" stoichiometry="1" constant="true"/>'))
    expect_equal(solve_r(m, times)$A, 2 - 0.25 * times, tolerance = 1e-8)
})

test_that("a sized 0-dimensional compartment divides by its size", {
    skip_if_not_installed("deSolve")
    m <- l3_model(
        cmt = '<compartment id="C" size="2" spatialDimensions="0" constant="true"/>',
        species = species_A(attrs = 'initialAmount="4"'),
        reactions = zero_order('<speciesReference species="A" stoichiometry="1" constant="true"/>'))
    expect_equal(solve_r(m, times)$A, 2 - 0.25 * times, tolerance = 1e-8)
})

# --- math that names things replaceReactions removes or never expected ----------

test_that("a species-reference id read by other math is its stoichiometry", {
    skip_if_not_installed("deSolve")
    m <- l3_model(
        cmt = '<compartment id="C" size="1" spatialDimensions="3" constant="true"/>',
        species = species_A(),
        params = '<parameter id="x" constant="false"/>',
        rules = sprintf('<assignmentRule variable="x">%s</assignmentRule>', mathml("<ci>sr</ci>")),
        reactions = zero_order('<speciesReference id="sr" species="A" stoichiometry="2" constant="true"/>'))
    d <- solve_r(m, times)
    expect_equal(d$A, 10 - 2 * 0.5 * times, tolerance = 1e-8)
    expect_equal(d$x, rep(2, length(times)))
})

test_that("a rule with no math has no effect", {
    m <- l3_model(
        cmt = '<compartment id="C" size="1" spatialDimensions="3" constant="true"/>',
        params = '<parameter id="p" value="3" constant="false"/>',
        rules = '<assignmentRule variable="p"/>')
    for (fmt in c("R", "mrgsolve", "nlmixr2", "MATLAB", "Julia", "ubiquity")) {
        o <- tempfile()
        convertReactions(m, o, format = fmt)
        expect_false(any(grepl("\\bp\\s*(=|<-)\\s*;?\\s*$", readLines(o))), info = fmt)
    }
    expect_equal(source_r(m)$parameters[["p"]], 3)
})

test_that("an initial assignment to NaN converts", {
    m <- l3_model(
        cmt = '<compartment id="C" size="1" spatialDimensions="3" constant="true"/>',
        params = '<parameter id="p" value="0" constant="true"/>',
        ias = sprintf('<initialAssignment symbol="p">%s</initialAssignment>',
                      mathml("<notanumber/>")))
    # warnUndefinedValues() cannot tell a NaN the file asks for from a missing
    # value, so it still names p.
    expect_warning(env <- source_r(m), "\\bp\\b")
    expect_true(is.nan(env$parameters[["p"]]))
})
