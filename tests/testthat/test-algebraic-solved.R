# An algebraic rule naming exactly one undetermined variable, linearly, is
# rewritten as an assignment rule (solveLinearAlgebraicRules()), so it works
# in every target rather than only the three that solve DAEs.

algebraic_model <- function(cmt, species, params, rules, reactions) {
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

alg_rule <- function(body) sprintf(
    '<algebraicRule><math xmlns="http://www.w3.org/1998/Math/MathML">%s</math></algebraicRule>', body)

# A decays at rate k.  Three algebraic rules, each linear in one variable:
#   p2 - 3*M = 0       names p2 and M, so it can only be solved after M is;
#   k - 0.5 = 0        k has no value at all;
#   (k + 1)*M - A = 0  M is a species no reaction changes.
# So k = 0.5, A = exp(-t/2), M = A/1.5 and p2 = 2*A.
solvable_model <- function() algebraic_model(
    cmt = '<compartment id="c" size="1" spatialDimensions="3" constant="true"/>',
    species = '<species id="A" compartment="c" initialConcentration="1" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false"/>
               <species id="M" compartment="c" initialConcentration="0" boundaryCondition="false"
               hasOnlySubstanceUnits="false" constant="false"/>',
    params = '<parameter id="k" constant="false"/>
              <parameter id="p2" value="0" constant="false"/>',
    rules = paste0(
        alg_rule('<apply><minus/><ci>p2</ci><apply><times/><cn>3</cn><ci>M</ci></apply></apply>'),
        alg_rule('<apply><minus/><ci>k</ci><cn>0.5</cn></apply>'),
        alg_rule('<apply><minus/><apply><times/><apply><plus/><ci>k</ci><cn>1</cn></apply><ci>M</ci></apply><ci>A</ci></apply>')),
    reactions = '<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<listOfModifiers><modifierSpeciesReference species="M"/></listOfModifiers>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>k</ci><ci>A</ci><ci>c</ci></apply></math></kineticLaw>
</reaction>')

convert_lines <- function(sbml, format, ext) {
    out <- tempfile(fileext = ext)
    utils::capture.output(convertReactions(sbml, out, format = format))
    readLines(out)
}

tt <- c(0, 1, 2, 4)

test_that("solvable algebraic rules convert without a warning in every target", {
    targets <- c(R = ".R", mrgsolve = ".cpp", nlmixr2 = ".R", MATLAB = ".m",
                 Julia = ".jl", ubiquity = ".txt")
    for (f in names(targets)) {
        out <- tempfile(fileext = targets[[f]])
        expect_no_warning(utils::capture.output(convertReactions(solvable_model(), out, format = f)))
        expect_false(any(grepl("lgebraic", readLines(out))), info = f)
    }
})

test_that("deSolve integrates the solved rules as an ODE", {
    skip_if_not_installed("deSolve")
    lines <- convert_lines(solvable_model(), "R", ".R")
    expect_false(any(grepl("Residuals", lines)))   # ode, not daspk
    env <- new.env()
    eval(parse(text = lines), envir = env)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = tt,
                                    func = env$massBalances, parms = env$parameters))
    expect_equal(d$A, exp(-0.5 * tt), tolerance = 1e-5)
    expect_equal(d$M, d$A / 1.5, tolerance = 1e-10)
    expect_equal(d$p2, 2 * d$A, tolerance = 1e-10)
})

test_that("mrgsolve and rxode2 enforce the solved rules", {
    skip_on_cran()
    skip_if_not_installed("mrgsolve")
    skip_if_not_installed("rxode2")

    code <- paste(convert_lines(solvable_model(), "mrgsolve", ".cpp"), collapse = "\n")
    mod  <- suppressMessages(mrgsolve::mcode("r2sbml_algebraic", code, quiet = TRUE))
    out  <- as.data.frame(mrgsolve::mrgsim_df(mod, tgrid = tt))
    expect_equal(out$A, exp(-0.5 * tt), tolerance = 1e-5)
    expect_equal(out$M, out$A / 1.5, tolerance = 1e-8)
    expect_equal(out$p2, 2 * out$A, tolerance = 1e-8)

    rf <- tempfile(fileext = ".R")
    utils::capture.output(convertReactions(solvable_model(), rf, format = "rxode2"))
    env <- new.env()
    sys.source(rf, envir = env)
    rx  <- suppressMessages(rxode2::rxode2(env$model))
    out <- as.data.frame(suppressMessages(rxode2::rxSolve(rx, rxode2::et(tt))))
    expect_equal(out$A, exp(-0.5 * tt), tolerance = 1e-5)
    expect_equal(out$M, out$A / 1.5, tolerance = 1e-8)
    expect_equal(out$p2, 2 * out$A, tolerance = 1e-8)
})

test_that("an algebraic rule can size a compartment", {
    skip_if_not_installed("deSolve")
    # V has no size; 0 = V - 2 gives it one, so A's amount of 2 is a
    # concentration of 1.  (The assignment-rule compartment warning is about
    # dilution, which a constant volume does not need.)
    sbml <- algebraic_model(
        cmt = '<compartment id="V" spatialDimensions="3" constant="false"/>',
        species = '<species id="A" compartment="V" initialAmount="2" boundaryCondition="false"
                   hasOnlySubstanceUnits="false" constant="false"/>',
        params = '<parameter id="k" value="0.5" constant="true"/>',
        rules = alg_rule('<apply><minus/><ci>V</ci><cn>2</cn></apply>'),
        reactions = '<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>k</ci><ci>A</ci><ci>V</ci></apply></math></kineticLaw>
</reaction>')
    lines <- suppressWarnings(convert_lines(sbml, "R", ".R"))
    env <- new.env()
    eval(parse(text = lines), envir = env)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = tt,
                                    func = env$massBalances, parms = env$parameters))
    expect_equal(d$A, exp(-0.5 * tt), tolerance = 1e-5)
    expect_equal(d$V, rep(2, length(tt)))
})

test_that("a rule naming two undetermined variables stays a DAE", {
    # The example's two rules each name both E and ES, so neither is peeled.
    f <- system.file("examples", "sbmlalgebraicrules.xml", package = "r2sbml")
    if (!nzchar(f)) f <- "../../inst/examples/sbmlalgebraicrules.xml"
    expect_true(any(grepl("Residuals", convert_lines(f, "R", ".R"))))
})
