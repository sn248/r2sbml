example_file <- function(f) {
    p <- system.file("examples", f, package = "r2sbml")
    if (p == "") p <- file.path("../../inst/examples", f)
    p
}

# None of the bundled examples sets a `name`, or leaves a species with neither
# initial attribute, so the interesting branches need a model of our own.
named_model <- function() {
    xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>
<compartment id="c1" name="Cytosol" size="1" spatialDimensions="3" constant="true"/>
<compartment id="c2" size="2" spatialDimensions="3" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="A" name="Glucose" compartment="c1" initialAmount="3" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
<species id="B" compartment="c1" initialConcentration="7" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
<species id="C" compartment="c2" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
</listOfSpecies>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}

test_that("getSpeciesIC reports whichever initial attribute is set", {
    # sbmlsimple.xml is amount-based; reading initialConcentration alone
    # returned NaN for all four species
    model <- getModel(example_file("sbmlsimple.xml"))
    ic <- getSpeciesIC(model)

    expect_false(any(is.nan(ic)))
    expect_equal(unname(ic[["E"]]), 5e-21)
    expect_equal(unname(ic[["S"]]), 1e-20)

    # named by species id, so the vector says what it holds
    expect_equal(names(ic), c("E", "S", "P", "ES"))

    # a concentration-based model still reports its concentrations
    ic2 <- getSpeciesIC(getModel(example_file("sbmlassignmentrules.xml")))
    expect_false(any(is.nan(ic2)))
    expect_equal(unname(ic2[["X0"]]), 1)

    # no example has a species with neither attribute set
    ic3 <- getSpeciesIC(getModel(named_model()))
    expect_equal(unname(ic3[["A"]]), 3)   # initialAmount
    expect_equal(unname(ic3[["B"]]), 7)   # initialConcentration
    # neither set: NA, meaning "the file does not say", not a fabricated 0
    expect_true(is.na(ic3[["C"]]))
    expect_false(is.nan(ic3[["C"]]))
})

test_that("getSpeciesIC does not convert units", {
    # convertReactions divides an amount by the compartment volume because the
    # ODE integrates concentrations; the query function reports the file as-is
    model <- getModel(example_file("sbmlsimple.xml"))
    expect_equal(unname(getSpeciesIC(model)[["E"]]), 5e-21)   # not 5e-07

    out <- tempfile(fileext = ".R")
    expect_invisible(convertReactions(example_file("sbmlsimple.xml"), out, format = "R"))
    env <- new.env()
    env$library <- function(...) invisible(NULL)
    source(out, local = env)
    expect_equal(unname(env$InitialAmounts[["E"]]), 5e-07)    # converted
})

test_that("names fall back to the id when no name attribute is set", {
    # every bundled example sets only ids, so this used to be all empty strings
    model <- getModel(example_file("sbmlsimple.xml"))
    expect_equal(getSpeciesNames(model), c("E", "S", "P", "ES"))
    expect_equal(getCmtNames(model), "comp")

    # where a name is set it wins, and the two functions agree on the rule
    m <- getModel(named_model())
    expect_equal(getSpeciesNames(m), c("Glucose", "B", "C"))
    expect_equal(getCmtNames(m), c("Cytosol", "c2"))
})

test_that("getSpeciesTable still reports the raw attributes", {
    # the table is the honest one: separate ID and Name columns, and both
    # initial-value columns, so NaN there marks the attribute the model omitted
    tbl <- getSpeciesTable(getModel(named_model()))

    expect_equal(tbl$ID, c("A", "B", "C"))
    expect_equal(tbl$Name, c("Glucose", "", ""))        # not filled in
    expect_equal(tbl$InitialAmount[1], 3)
    expect_true(is.nan(tbl$InitialConcentration[1]))
    expect_equal(tbl$InitialConcentration[2], 7)
})

test_that("the vector accessors do not print", {
    # they return data; they used to also echo each element to the console
    model <- getModel(example_file("sbmlsimple.xml"))
    expect_silent(getSpeciesNames(model))
    expect_silent(getSpeciesIC(model))
    expect_silent(getCmtNames(model))
})

test_that("an absent component names itself in the error message", {
    # the explanation used to go to stdout while the condition carried only
    # "Stopping!", so a caller could not tell the cases apart
    model <- getModel(example_file("sbmlsimple.xml"))

    expect_error(getRuleMath(model),          "No Rules present")
    expect_error(getParameterTable(model),    "No parameters present")
    expect_error(getFunctionDefinition(model), "No Function definitions present")

    events <- getModel(example_file("sbmlfunctiondefinition.xml"))
    expect_error(getEventMath(events), "No Events present")

    # and the old opaque message is gone rather than merely prefixed
    msg <- tryCatch(getRuleMath(model), error = conditionMessage)
    expect_false(grepl("Stopping", msg, fixed = TRUE))
})

test_that("getModel and convertReactions agree on which files are readable", {
    broken <- tempfile(fileext = ".xml")
    writeLines("this is not xml at all <<<>>>", broken)

    # both refuse, and both say which file and why
    expect_error(getModel(broken), "Cannot read")
    expect_error(getModel(broken), "model was not loaded")
    expect_error(convertReactions(broken, tempfile()), "Cannot read")
    expect_error(convertReactions(broken, tempfile()), "Nothing was written")

    # and both accept a clean file
    good <- example_file("sbmlsimple.xml")
    expect_s3_class(tryCatch({getModel(good); structure(list(), class = "ok")},
                             error = function(e) e), "ok")
    expect_invisible(convertReactions(good, tempfile(), format = "R"))
})
