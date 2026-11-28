example_file <- function(f) {
  p <- system.file("examples", f, package = "r2sbml")
  if (p == "") {
    p <- file.path("../../inst/examples", f)
  }
  p
}

load_quietly <- function(f) {
  model <- NULL
  capture.output(model <- getModel(f))
  model
}

# Write, read back and return the reloaded model.
round_trip <- function(model) {
  out <- tempfile(fileext = ".xml")
  writeSBML(model, out)
  load_quietly(out)
}

param_value <- function(model, id) {
  t <- getParameterTable(model)
  t$Value[t$ID == id]
}

cmt_size <- function(model, id) {
  s <- getCmtSizes(model)
  unname(s[getCmtNames(model) == id])
}

multi <- function() load_quietly(example_file("sbmlmutlicompartment.xml"))

test_that("setParameterValue edits in place and survives a round trip", {
  model <- multi()
  before <- getParameterTable(model)

  expect_invisible(res <- setParameterValue(model, "k1", 1000))
  expect_identical(res, model)
  # in place: visible through the same pointer without reloading
  expect_equal(param_value(model, "k1"), 1000)

  back <- round_trip(model)
  expect_equal(param_value(back, "k1"), 1000)
  others <- before$ID != "k1"
  expect_equal(getParameterTable(back)$Value[others], before$Value[others])
})

test_that("setCompartmentSize edits in place and survives a round trip", {
  model <- multi()
  expect_invisible(setCompartmentSize(model, "nucleus", 2))
  expect_equal(cmt_size(model, "nucleus"), 2)

  back <- round_trip(model)
  expect_equal(cmt_size(back, "nucleus"), 2)
  expect_equal(cmt_size(back, "cytoplasm"), 5)
})

test_that("setSpeciesIC sets the named attribute and unsets the other", {
  model <- multi()
  # X starts with initialAmount = 1
  expect_invisible(setSpeciesIC(model, "X", 0.5, as = "concentration"))
  out <- tempfile(fileext = ".xml")
  writeSBML(model, out)
  x_line <- grep('<species id="X"', readLines(out), value = TRUE)
  expect_length(x_line, 1)
  expect_match(x_line, 'initialConcentration="0.5"')
  expect_no_match(x_line, "initialAmount")
  back <- load_quietly(out)
  expect_equal(unname(getSpeciesIC(back)[["X"]]), 0.5)

  setSpeciesIC(model, "X", 3, as = "amount")
  back <- round_trip(model)
  expect_equal(unname(getSpeciesIC(back)[["X"]]), 3)
})

test_that("the setters chain", {
  model <- multi()
  model |>
    setParameterValue("k1", 1) |>
    setParameterValue("k2", 2) |>
    setCompartmentSize("cytoplasm", 3)
  expect_equal(param_value(model, "k1"), 1)
  expect_equal(param_value(model, "k2"), 2)
  expect_equal(cmt_size(model, "cytoplasm"), 3)
})

test_that("an unknown id is an error naming the component", {
  model <- multi()
  expect_error(
    setParameterValue(model, "nope", 1),
    "No parameter 'nope' in the model.",
    fixed = TRUE
  )
  expect_error(
    setCompartmentSize(model, "nope", 1),
    "No compartment 'nope' in the model.",
    fixed = TRUE
  )
  expect_error(
    setSpeciesIC(model, "nope", 1, as = "amount"),
    "No species 'nope' in the model.",
    fixed = TRUE
  )
  # a species id is not a parameter
  expect_error(setParameterValue(model, "X", 1), "No parameter 'X'")
})

test_that("bad values and a bad `as` are errors", {
  model <- multi()
  expect_error(setParameterValue(model, "k1", "1"), "single number")
  expect_error(setParameterValue(model, "k1", c(1, 2)), "single number")
  expect_error(setParameterValue(model, "k1", TRUE), "single number")
  expect_error(setSpeciesIC(model, "X", 1, as = "mass"), "`as` must be")
  expect_error(setSpeciesIC(model, "X", 1), "as")
})

test_that("integer values are accepted and NA unsets the value", {
  model <- multi()
  setParameterValue(model, "k1", 7L)
  expect_equal(param_value(model, "k1"), 7)

  setParameterValue(model, "k1", NA)
  expect_true(is.nan(param_value(model, "k1")))
  # libSBML's best-practice check flags a parameter with no value, as a
  # warning: the file is still written
  out <- tempfile(fileext = ".xml")
  capture.output(
    type = "message",
    expect_warning(writeSBML(model, out), "1 SBML warning")
  )
  k1 <- grep('id="k1"', readLines(out), value = TRUE)
  expect_no_match(k1, "value=")
})

test_that("a value an assignment rule sets warns that it has no effect", {
  model <- load_quietly(example_file("sbmlassignmentrules.xml"))
  expect_warning(
    setSpeciesIC(model, "S1", 1, as = "concentration"),
    "species 'S1' has no effect: an assignment rule sets it"
  )
  expect_no_warning(setSpeciesIC(model, "X0", 2, as = "concentration"))
  expect_no_warning(setParameterValue(model, "Keq", 3))
})

test_that("a value an initial assignment sets warns that it has no effect", {
  xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model>
<listOfParameters>
<parameter id="a" value="1" constant="true"/>
<parameter id="b" value="2" constant="true"/>
</listOfParameters>
<listOfInitialAssignments>
<initialAssignment symbol="b">
<math xmlns="http://www.w3.org/1998/Math/MathML"><apply><times/><cn>2</cn><ci>a</ci></apply></math>
</initialAssignment>
</listOfInitialAssignments>
</model>
</sbml>'
  f <- tempfile(fileext = ".xml")
  writeLines(xml, f)
  model <- load_quietly(f)
  expect_warning(
    setParameterValue(model, "b", 5),
    "parameter 'b' has no effect: an initial assignment sets it"
  )
  expect_no_warning(setParameterValue(model, "a", 5))
})

test_that("copyModel makes an independent copy", {
  model <- multi()
  alias <- model
  copy <- copyModel(model)

  setParameterValue(copy, "k1", 1)
  expect_equal(param_value(copy, "k1"), 1)
  expect_equal(param_value(model, "k1"), 2500)

  # assignment copies the pointer, not the model
  setParameterValue(alias, "k2", 9)
  expect_equal(param_value(model, "k2"), 9)

  rm(model, alias)
  gc()
  expect_equal(param_value(copy, "k2"), 2500)
})

test_that("writeSBML refuses a model that fails the consistency checks", {
  # The reader does not resolve ids in math, so getModel() accepts a rule
  # naming an undefined id; the full consistency check run before writing
  # rejects it, at error severity.
  xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model>
<listOfParameters>
<parameter id="a" value="1" constant="false"/>
</listOfParameters>
<listOfRules>
<assignmentRule variable="a">
<math xmlns="http://www.w3.org/1998/Math/MathML"><ci>undefined_q</ci></math>
</assignmentRule>
</listOfRules>
</model>
</sbml>'
  f <- tempfile(fileext = ".xml")
  writeLines(xml, f)
  model <- load_quietly(f)

  out <- tempfile(fileext = ".xml")
  capture.output(
    type = "message",
    expect_error(writeSBML(model, out), "Cannot write .* Nothing was written")
  )
  expect_false(file.exists(out))
})

test_that("writeSBML returns invisibly and keeps level and version", {
  model <- multi()
  out <- tempfile(fileext = ".xml")
  expect_invisible(writeSBML(model, out))
  orig <- readLines(example_file("sbmlmutlicompartment.xml"))
  lv <- function(x) regmatches(x, regexpr('level="[0-9]+" version="[0-9]+"', x))
  expect_equal(
    lv(paste(readLines(out), collapse = " ")),
    lv(paste(orig, collapse = " "))
  )
})

test_that("convertReactions uses an edited model", {
  model <- multi()
  setParameterValue(model, "k1", 1234)
  out <- tempfile(fileext = ".R")
  capture.output(convertReactions(model, out, format = "R"))
  expect_true(any(grepl("1234", readLines(out))))
})

rate_laws <- function(model) {
  res <- NULL
  capture.output(res <- getReactionMath(model))
}

simple <- function() load_quietly(example_file("sbmlsimple.xml"))

test_that("setRateLaw replaces a rate law in place and survives a round trip", {
  model <- simple()
  before <- rate_laws(model)

  # kcat is vcat's local parameter, so it stays in scope
  expect_invisible(
    res <- setRateLaw(model, "vcat", "comp * kcat * ES * ES / (ES + P)")
  )
  expect_identical(res, model)
  now <- rate_laws(model)
  expect_equal(now[2], "Reaction 1, formula: comp * kcat * ES * ES / (ES + P)")
  expect_equal(now[1], before[1])

  out <- tempfile(fileext = ".xml")
  writeSBML(model, out)
  expect_equal(rate_laws(load_quietly(out)), now)
  # the local parameter was kept
  expect_true(any(grepl('<localParameter id="kcat"', readLines(out))))
})

test_that("setRateLaw accepts function definitions and csymbols", {
  model <- load_quietly(example_file("sbmlfunctiondefinition.xml"))
  setRateLaw(model, "reaction_1", "f(S1) * compartmentOne * exp(-time)")
  expect_match(rate_laws(model), "f\\(S1\\) \\* compartmentOne \\* exp\\(-time\\)")
})

test_that("a formula that does not parse is an error carrying the parser's message", {
  model <- simple()
  before <- rate_laws(model)
  expect_error(
    setRateLaw(model, "vcat", "kcat * (ES"),
    "Cannot parse the rate law of reaction 'vcat': .*syntax error"
  )
  expect_equal(rate_laws(model), before)
})

test_that("an undefined name is an error and leaves the model unchanged", {
  model <- simple()
  before <- rate_laws(model)
  expect_error(
    setRateLaw(model, "vcat", "q1 * ES + g(ES) + q1"),
    "names undefined ids: 'q1', 'g()'. The model was not changed.",
    fixed = TRUE
  )
  # another reaction's local parameter is out of scope
  expect_error(
    setRateLaw(model, "vcat", "comp * kon * ES"),
    "names an undefined id: 'kon'",
    fixed = TRUE
  )
  expect_equal(rate_laws(model), before)
})

test_that("setRateLaw rejects an unknown reaction and a bad formula argument", {
  model <- simple()
  expect_error(
    setRateLaw(model, "nope", "1"),
    "No reaction 'nope' in the model.",
    fixed = TRUE
  )
  expect_error(setRateLaw(model, "vcat", NA_character_), "single string")
  expect_error(setRateLaw(model, "vcat", c("1", "2")), "single string")
  expect_error(setRateLaw(model, "vcat", 1), "single string")
})

test_that("setRateLaw gives a reaction without a kinetic law one", {
  xml <- '<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model>
<listOfCompartments>
<compartment id="c" size="1" spatialDimensions="3" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="A" compartment="c" initialAmount="1" hasOnlySubstanceUnits="true"
 boundaryCondition="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="k" value="0.5" constant="true"/>
</listOfParameters>
<listOfReactions>
<reaction id="decay" reversible="false">
<listOfReactants>
<speciesReference species="A" stoichiometry="1" constant="true"/>
</listOfReactants>
</reaction>
</listOfReactions>
</model>
</sbml>'
  f <- tempfile(fileext = ".xml")
  writeLines(xml, f)
  model <- load_quietly(f)
  expect_length(rate_laws(model), 0)
  setRateLaw(model, "decay", "k * A")
  # the model declares no units, so the write warns
  back <- NULL
  capture.output(
    type = "message",
    expect_warning(back <- round_trip(model), "SBML warning")
  )
  expect_equal(rate_laws(back)[1], "Reaction 0, formula: k * A")
})

test_that("convertReactions uses an edited rate law", {
  model <- simple()
  setRateLaw(model, "vcat", "comp * kcat * ES * ES / (ES + P)")
  out <- tempfile(fileext = ".R")
  capture.output(convertReactions(model, out, format = "R"))
  expect_true(any(grepl("/ (ES + P)", readLines(out), fixed = TRUE)))
})
