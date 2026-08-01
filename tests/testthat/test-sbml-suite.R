test_that("Conversion works on SBML test suite cases", {
  # Get all the L3V2 XML files in the test suite
  test_suite_dir <- system.file("examples", package = "r2sbml")
  if (test_suite_dir == "") test_suite_dir <- "../../inst/examples"
  if (dir.exists(test_suite_dir)) {
    xml_files <- list.files(test_suite_dir, pattern = "\\.xml$", full.names = TRUE)

    for (xml_file in xml_files) {
        out_r <- tempfile(fileext = ".R")
        out_mrg <- tempfile(fileext = ".cpp")
        out_rx <- tempfile(fileext = ".R")
        out_m <- tempfile(fileext = ".m")
        out_jl <- tempfile(fileext = ".jl")

        # Test R/deSolve
        expect_invisible(convertReactions(xml_file, out_r, format = "R"))
        expect_true(file.exists(out_r))

        # Test mrgsolve
        expect_invisible(convertReactions(xml_file, out_mrg, format = "mrgsolve"))
        expect_true(file.exists(out_mrg))

        # Test nlmixr2/rxode
        expect_invisible(convertReactions(xml_file, out_rx, format = "nlmixr2"))
        expect_true(file.exists(out_rx))

        # Test MATLAB
        expect_invisible(convertReactions(xml_file, out_m, format = "MATLAB"))
        expect_true(file.exists(out_m))

        # Test Julia
        expect_invisible(convertReactions(xml_file, out_jl, format = "Julia"))
        expect_true(file.exists(out_jl))
    }
  }
})
