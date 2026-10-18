# Extracted from test-outputs.R:62

# setup ------------------------------------------------------------------------
library(testthat)
test_env <- simulate_test_env(package = "r2sbml", path = "..")
attach(test_env, warn.conflicts = FALSE)

# prequel ----------------------------------------------------------------------
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

# test -------------------------------------------------------------------------
f <- assignment_rules_file()
mrg <- convert_lines(f, "mrgsolve", ".cpp")
table <- mrg[seq(grep("^\\$TABLE$", mrg), length(mrg))]
expect_true(any(grepl("^S1 = T / \\(1 \\+ Keq\\);$", table)))
