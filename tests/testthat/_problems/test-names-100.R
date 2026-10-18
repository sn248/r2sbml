# Extracted from test-names.R:100

# setup ------------------------------------------------------------------------
library(testthat)
test_env <- simulate_test_env(package = "r2sbml", path = "..")
attach(test_env, warn.conflicts = FALSE)

# prequel ----------------------------------------------------------------------
names_model <- function() {
    xml <- '<?xml version="1.0" encoding="UTF-8"?>
<sbml xmlns="http://www.sbml.org/sbml/level3/version2/core" level="3" version="2">
<model id="names">
<listOfCompartments>
<compartment id="c" spatialDimensions="3" size="1" constant="true"/>
</listOfCompartments>
<listOfSpecies>
<species id="TRUE" compartment="c" initialAmount="1" hasOnlySubstanceUnits="true"
boundaryCondition="false" constant="false"/>
<species id="time" compartment="c" initialAmount="0" hasOnlySubstanceUnits="true"
boundaryCondition="false" constant="false"/>
</listOfSpecies>
<listOfParameters>
<parameter id="Inf" value="2" constant="true"/>
<parameter id="p" value="3" constant="true"/>
<parameter id="rate" value="0.5" constant="true"/>
<parameter id="NaN" constant="false"/>
<parameter id="y" constant="false"/>
</listOfParameters>
<listOfRules>
<assignmentRule variable="NaN"><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>TRUE</ci><ci>p</ci></apply></math></assignmentRule>
<assignmentRule variable="y"><math xmlns="http://www.w3.org/1998/Math/MathML">
<piecewise><piece><cn>1</cn><apply><gt/><ci>Inf</ci><cn>1</cn><cn>0</cn></apply></piece>
<otherwise><cn>0</cn></otherwise></piecewise></math></assignmentRule>
</listOfRules>
<listOfReactions>
<reaction id="J0" reversible="false">
<listOfReactants><speciesReference species="TRUE" stoichiometry="1" constant="true"/></listOfReactants>
<listOfProducts><speciesReference species="time" stoichiometry="1" constant="true"/></listOfProducts>
<kineticLaw><math xmlns="http://www.w3.org/1998/Math/MathML">
<apply><times/><ci>rate</ci><ci>Inf</ci><ci>TRUE</ci></apply></math></kineticLaw>
</reaction>
</listOfReactions>
</model>
</sbml>'
    f <- tempfile(fileext = ".xml")
    writeLines(xml, f)
    f
}
convert_to <- function(sbml, format, ext) {
    o <- tempfile(fileext = ext)
    utils::capture.output(convertReactions(sbml, o, format = format))
    readLines(o)
}

# test -------------------------------------------------------------------------
sbml <- names_model()
expect_true(any(grepl("(Inf_ > 1) && (1 > 0)", convert_to(sbml, "MATLAB", ".m"), fixed = TRUE)))
expect_true(any(grepl("(Inf_ > 1.0) && (1.0 > 0.0)", convert_to(sbml, "mrgsolve", ".cpp"), fixed = TRUE)))
expect_true(any(grepl("SIMINT_AND[SIMINT_GT[Inf_][1]][SIMINT_GT[1][0]]",
                          convert_to(sbml, "ubiquity", ".txt"), fixed = TRUE)))
