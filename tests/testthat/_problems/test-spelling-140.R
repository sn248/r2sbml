# Extracted from test-spelling.R:140

# setup ------------------------------------------------------------------------
library(testthat)
test_env <- simulate_test_env(package = "r2sbml", path = "..")
attach(test_env, warn.conflicts = FALSE)

# prequel ----------------------------------------------------------------------
spelling_model <- function(rules, params = "", rate = "<ci>k</ci>") {
    xml <- sprintf('<?xml version="1.0" encoding="UTF-8" ?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model substanceUnits="mole" volumeUnits="litre" timeUnits="second" extentUnits="mole">
<listOfCompartments>
<compartment id="c" size="1" spatialDimensions="3" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="A" compartment="c" initialConcentration="1" boundaryCondition="false"
hasOnlySubstanceUnits="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="k" constant="false"/>
%s
</listOfParameters>
<listOfRules>%s</listOfRules>
<listOfReactions>
<reaction id="decay" reversible="false">
<listOfReactants><speciesReference species="A" stoichiometry="1" constant="true"/></listOfReactants>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/>%s<ci>A</ci></apply></math></kineticLaw>
</reaction>
</listOfReactions>
</model>
</sbml>', params, rules, rate)
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}
assign_rule <- function(variable, mathml) sprintf(
    '<assignmentRule variable="%s"><math xmlns="http://www.w3.org/1998/Math/MathML">%s</math></assignmentRule>',
    variable, mathml)
switching_k <- assign_rule("k",
    '<piecewise><piece><cn>2</cn><apply><gt/><ci>A</ci><cn>0.5</cn></apply></piece>
     <otherwise><cn>0.5</cn></otherwise></piecewise>')
spelled_targets <- function(sbml) {
    exts <- c(R = ".R", mrgsolve = ".cpp", nlmixr2 = ".R", MATLAB = ".m",
              Julia = ".jl", ubiquity = ".txt")
    lapply(setNames(names(exts), names(exts)), function(fmt) {
        o <- tempfile(fileext = exts[[fmt]])
        expect_no_warning(convertReactions(sbml, o, format = fmt))
        readLines(o)
    })
}

# test -------------------------------------------------------------------------
m <- spelling_model(assign_rule("k",
        '<piecewise><piece><cn>1</cn><apply><gt/><ci>A</ci><cn>2</cn></apply></piece></piecewise>'))
out <- spelled_targets(m)
for (fmt in c("R", "nlmixr2", "Julia")) {
        expect_true(any(grepl("ifelse\\(A > 2, 1, NaN\\)", out[[fmt]])), info = fmt)
    }
expect_true(any(grepl("r2sbml_ifelse\\(A > 2, 1, NAN\\)", out$mrgsolve)))
