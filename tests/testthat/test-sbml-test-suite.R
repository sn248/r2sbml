# Cases from the SBML Test Suite, checked numerically: each model is converted
# to deSolve code, simulated, and compared with the suite's own expected time
# course within the tolerances the case sets.  This catches the bugs that a
# structural test cannot -- generated code that runs, but integrates the wrong
# thing from the wrong place.  The runner is in helper-sbml-test-suite.R.
#
# The cases in sbml-test-suite/ are a curated set that passes today.  To add
# one, copy its directory from the suite's cases/semantic/ (only the
# -sbml-l3v2.xml, -results.csv and -settings.txt files are read).

sts_dir <- "sbml-test-suite"

for (case in sort(list.dirs(sts_dir, recursive = FALSE))) {
    test_that(paste("SBML Test Suite case", basename(case)), {
        skip_if_not_installed("deSolve")
        r <- sts_run_case(case)
        expect(identical(r$status, "pass"),
               sprintf("case %s: %s: %s", basename(case), r$status, r$detail))
    })
}

# The whole suite, opt-in and never on CRAN: set R2SBML_SBML_TEST_SUITE to the
# cases/semantic directory of a checkout of
# https://github.com/sbmlteam/sbml-test-suite.  Cases using a construct
# convertReactions does not translate (sts_unsupported_tags, SBML packages)
# are skipped, as they were when the floor below was measured.
#
# The floor is the number of in-scope cases that passed at the last
# measurement (master 4bc2da9: 647 of 1011).  A fix that passes more should
# raise it; falling below it means a regression.
sts_pass_floor <- 647

test_that("the full SBML Test Suite does not regress", {
    skip_on_cran()
    skip_if_not_installed("deSolve")
    root <- Sys.getenv("R2SBML_SBML_TEST_SUITE")
    skip_if(root == "", "R2SBML_SBML_TEST_SUITE is not set")
    expect_true(dir.exists(root), info = root)

    cases <- list.dirs(root, recursive = FALSE)
    cases <- cases[grepl("^[0-9]{5}$", basename(cases))]
    cases <- cases[vapply(cases, sts_in_scope, TRUE)]
    status <- vapply(cases, function(d) sts_run_case(d)$status, "")

    passed <- sum(status == "pass")
    # stderr, because testthat's reporters capture message() and output.
    cat(sprintf("SBML Test Suite: %d of %d in-scope cases pass (floor %d)\n",
                passed, length(cases), sts_pass_floor), file = stderr())
    expect_gte(passed, sts_pass_floor)
})
