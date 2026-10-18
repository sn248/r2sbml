# --- constructs every target spells differently ------------------------------
#
# The Level 3 writer passes ceiling, piecewise and the non-finite numbers
# through in spellings that are valid in few or none of the six targets:
# `ceil` is not an R function, `piecewise(...)` and `INF` are not anything,
# and operator<< writes an infinite value as `inf`.  Each test below checks
# every target's spelling, and the deSolve ones check the result as well.

# A one-species model: A decays at rate `k` (an assignment rule) unless
# `rate` is given, with `rules` and `params` spliced in verbatim.
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

# An assignment rule `variable = <mathml>`.
assign_rule <- function(variable, mathml) sprintf(
    '<assignmentRule variable="%s"><math xmlns="http://www.w3.org/1998/Math/MathML">%s</math></assignmentRule>',
    variable, mathml)

# k = piecewise(2 if A > 0.5, otherwise 0.5): the decay slows once A halves.
switching_k <- assign_rule("k",
    '<piecewise><piece><cn>2</cn><apply><gt/><ci>A</ci><cn>0.5</cn></apply></piece>
     <otherwise><cn>0.5</cn></otherwise></piecewise>')

# Output for every target, named; fails the test on any warning.
spelled_targets <- function(sbml) {
    exts <- c(R = ".R", mrgsolve = ".cpp", nlmixr2 = ".R", MATLAB = ".m",
              Julia = ".jl", ubiquity = ".txt")
    lapply(setNames(names(exts), names(exts)), function(fmt) {
        o <- tempfile(fileext = exts[[fmt]])
        expect_no_warning(convertReactions(sbml, o, format = fmt))
        readLines(o)
    })
}

test_that("ceiling and floor are spelled as each target spells them", {
    m <- spelling_model(paste(
        assign_rule("k", "<cn>1</cn>"),
        assign_rule("up",   "<apply><ceiling/><ci>A</ci></apply>"),
        assign_rule("down", "<apply><floor/><ci>A</ci></apply>")),
        params = '<parameter id="up" constant="false"/><parameter id="down" constant="false"/>')
    out <- spelled_targets(m)

    # R's function is ceiling; C++, MATLAB and Julia's is ceil
    for (fmt in c("R", "nlmixr2")) expect_true(any(grepl("\\bceiling\\(A\\)", out[[fmt]])), info = fmt)
    for (fmt in c("mrgsolve", "MATLAB", "Julia")) {
        expect_true(any(grepl("\\bceil\\(A\\)", out[[fmt]])), info = fmt)
        expect_false(any(grepl("ceiling", out[[fmt]])), info = fmt)
    }
    expect_false(any(grepl("\\bceil\\(", out$R)))
    # ubiquity pastes it into R and C at once, where only floor is common
    expect_true(any(grepl("^<Ad> up = \\(-floor\\(-\\(A\\)\\)\\)$", out$ubiquity)))

    for (fmt in names(out)) expect_true(any(grepl("\\bfloor\\(A\\)", out[[fmt]])), info = fmt)
})

test_that("piecewise becomes a conditional each target can run", {
    m <- spelling_model(assign_rule("k",
        '<piecewise><piece><cn>1</cn><apply><gt/><ci>A</ci><cn>2</cn></apply></piece>
         <piece><cn>2</cn><apply><lt/><ci>A</ci><cn>0</cn></apply></piece>
         <otherwise><cn>3</cn></otherwise></piecewise>'))
    out <- spelled_targets(m)

    for (fmt in names(out)) expect_false(any(grepl("piecewise\\(", out[[fmt]])), info = fmt)

    nested <- "ifelse\\(A > 2, 1, %sifelse\\(A < 0, 2, 3\\)\\)"
    expect_true(any(grepl(sprintf(nested, ""), out$R)))
    expect_true(any(grepl(sprintf(nested, ""), out$nlmixr2)))
    expect_true(any(grepl(sprintf(nested, ""), out$Julia)))
    # C++ and MATLAB have no conditional expression: a helper, defined once
    expect_true(any(grepl("r2sbml_ifelse\\(A > 2\\.0, 1\\.0, r2sbml_ifelse\\(A < 0\\.0, 2\\.0, 3\\.0\\)\\)", out$mrgsolve)))
    expect_true(any(grepl("^\\$GLOBAL$", out$mrgsolve)))
    expect_equal(sum(grepl("inline double r2sbml_ifelse\\(bool c, double a, double b\\)", out$mrgsolve)), 1)
    expect_true(any(grepl("r2sbml_ifelse\\(A > 2, 1, r2sbml_ifelse\\(A < 0, 2, 3\\)\\)", out$MATLAB)))
    expect_equal(sum(grepl("^function v = r2sbml_ifelse\\(c, a, b\\)$", out$MATLAB)), 1)

    # ubiquity's conditional is a statement, so the piecewise gets a
    # parameter of its own, defined before the rule that reads it
    u <- out$ubiquity
    def <- grep("^<Ad> r2sbml_pw1 = 0.0", u)
    use <- grep("^<Ad> k = r2sbml_pw1$", u)
    expect_length(def, 1)
    expect_length(use, 1)
    expect_true(def < use)
    expect_true(any(grepl("^<IF:r2sbml_pw1:COND> SIMINT_GT\\[A\\]\\[2\\.0\\]; 1\\.0$", u)))
    expect_true(any(grepl("^<IF:r2sbml_pw1:COND> SIMINT_LT\\[A\\]\\[0\\.0\\]; 2\\.0$", u)))
    expect_true(any(grepl("^<IF:r2sbml_pw1:ELSE> 3\\.0$", u)))
})

test_that("a piecewise nested in a rate law is hoisted for ubiquity too", {
    # the piecewise sits in the kinetic law, so it reaches ubiquity through a
    # rate rule rather than an assignment rule
    m <- spelling_model(assign_rule("k", "<cn>1</cn>"), rate =
        '<apply><times/><cn>2</cn><piecewise><piece><cn>1</cn><apply><gt/><ci>A</ci><cn>2</cn></apply></piece>
         <otherwise><piecewise><piece><cn>4</cn><apply><lt/><ci>A</ci><cn>0</cn></apply></piece>
         <otherwise><cn>5</cn></otherwise></piecewise></otherwise></piecewise></apply>')
    u <- spelled_targets(m)$ubiquity

    # the inner piecewise is hoisted first, so the outer one can read it
    expect_true(any(grepl("^<IF:r2sbml_pw1:ELSE> 5\\.0$", u)))
    expect_true(any(grepl("^<IF:r2sbml_pw2:ELSE> r2sbml_pw1$", u)))
    expect_true(grep("^<Ad> r2sbml_pw1 ", u) < grep("^<Ad> r2sbml_pw2 ", u))
    expect_true(any(grepl("^<ODE:A> .*r2sbml_pw2", u)))
    expect_false(any(grepl("piecewise\\(", u)))
})

test_that("a piecewise with no otherwise is NaN where no condition holds", {
    m <- spelling_model(assign_rule("k",
        '<piecewise><piece><cn>1</cn><apply><gt/><ci>A</ci><cn>2</cn></apply></piece></piecewise>'))
    out <- spelled_targets(m)
    for (fmt in c("R", "nlmixr2", "Julia")) {
        expect_true(any(grepl("ifelse\\(A > 2, 1, NaN\\)", out[[fmt]])), info = fmt)
    }
    expect_true(any(grepl("r2sbml_ifelse\\(A > 2\\.0, 1\\.0, NAN\\)", out$mrgsolve)))
    expect_true(any(grepl("r2sbml_ifelse\\(A > 2, 1, NaN\\)", out$MATLAB)))
    expect_true(any(grepl("^<IF:r2sbml_pw1:ELSE> \\(0.0/0.0\\)$", out$ubiquity)))
})

test_that("true and false are spelled 1 and 0 for R", {
    # SBML Test Suite cases 00190 and 00195 put a bare <true/> or <false/> in
    # a piecewise condition.  R would take TRUE/FALSE, but rxode2 reads them
    # as undefined parameters.
    m <- spelling_model(assign_rule("k",
        '<piecewise><piece><cn>1</cn><true/></piece><piece><cn>2</cn><false/></piece>
         <otherwise><cn>3</cn></otherwise></piecewise>'))
    out <- spelled_targets(m)
    for (fmt in c("R", "nlmixr2")) {
        expect_true(any(grepl("ifelse\\(1, 1, ifelse\\(0, 2, 3\\)\\)", out[[fmt]])), info = fmt)
        expect_false(any(grepl("TRUE|FALSE", out[[fmt]])), info = fmt)
    }
    expect_true(any(grepl("ifelse\\(true, 1, ifelse\\(false, 2, 3\\)\\)", out$Julia)))
    expect_true(any(grepl("r2sbml_ifelse\\(true, 1\\.0, r2sbml_ifelse\\(false, 2\\.0, 3\\.0\\)\\)", out$mrgsolve)))
    expect_true(any(grepl("r2sbml_ifelse\\(true, 1, r2sbml_ifelse\\(false, 2, 3\\)\\)", out$MATLAB)))
})

test_that("the piecewise helpers are only written when a model needs them", {
    simple <- system.file("examples", "sbmlsimple.xml", package = "r2sbml")
    if (simple == "") simple <- "../../inst/examples/sbmlsimple.xml"
    out <- spelled_targets(simple)
    expect_false(any(grepl("GLOBAL|r2sbml_ifelse", out$mrgsolve)))
    expect_false(any(grepl("r2sbml_ifelse", out$MATLAB)))
})

test_that("infinity and NaN in math are spelled per target", {
    m <- spelling_model(paste(
        assign_rule("k", "<cn>1</cn>"),
        assign_rule("hi", "<infinity/>"),
        assign_rule("lo", "<apply><minus/><infinity/></apply>"),
        assign_rule("nn", "<notanumber/>")),
        params = paste0('<parameter id="hi" constant="false"/><parameter id="lo" constant="false"/>',
                        '<parameter id="nn" constant="false"/>'))
    out <- spelled_targets(m)

    # the Level 3 writer's INF is a number in none of them
    for (fmt in names(out)) expect_false(any(grepl("\\bINF\\b", out[[fmt]])), info = fmt)
    for (fmt in c("R", "nlmixr2", "MATLAB", "Julia")) {
        l <- out[[fmt]]
        expect_true(any(grepl("\\bhi (=|<-) Inf;?$", l)), info = fmt)
        expect_true(any(grepl("\\blo (=|<-) -Inf;?$", l)), info = fmt)
        expect_true(any(grepl("\\bnn (=|<-) NaN;?$", l)), info = fmt)
    }
    expect_true(any(grepl("^double hi = INFINITY;$", out$mrgsolve)))
    expect_true(any(grepl("^double lo = -INFINITY;$", out$mrgsolve)))
    expect_true(any(grepl("^double nn = NAN;$", out$mrgsolve)))
    expect_true(any(grepl("^<Ad> hi = \\(1.0/0.0\\)$", out$ubiquity)))
    expect_true(any(grepl("^<Ad> lo = \\(-\\(1.0/0.0\\)\\)$", out$ubiquity)))
    expect_true(any(grepl("^<Ad> nn = \\(0.0/0.0\\)$", out$ubiquity)))
})

test_that("an infinite parameter value is spelled per target, not as inf", {
    # SBML allows value="INF"; operator<< used to write it as `inf`
    m <- spelling_model(assign_rule("k", "<cn>1</cn>"),
        params = '<parameter id="cap" value="INF" constant="true"/><parameter id="floor0" value="-INF" constant="true"/>')
    out <- spelled_targets(m)

    for (fmt in names(out)) expect_false(any(grepl("\\binf\\b", sub("-inf inf", "", out[[fmt]]))), info = fmt)
    expect_true(any(grepl("^\\s+cap = Inf,$", out$R)))
    expect_true(any(grepl("^\\s+floor0 = -Inf$", out$R)))
    expect_true(any(grepl("^cap = Inf$", out$mrgsolve)))           # $PARAM is read by R
    expect_true(any(grepl("^\\s+cap <- Inf$", out$nlmixr2)))
    expect_true(any(grepl("^\\s+cap = Inf;$", out$MATLAB)))
    expect_true(any(grepl("^p = Float64\\[Inf, -Inf\\]$", out$Julia)))
    # a ubiquity <P> value is data, pasted into R-like files only, so it
    # takes the plain spelling; the bounds keep ubiquity's own `inf`
    expect_true(any(grepl("^<P> cap Inf -inf inf ", out$ubiquity)))
    expect_true(any(grepl("^<P> floor0 -Inf -inf inf ", out$ubiquity)))
})

test_that("a switching piecewise reproduces its analytic solution", {
    skip_if_not_installed("deSolve")

    # k = 2 while A > 0.5, then 0.5: A = exp(-2t) until A halves at
    # t1 = log(2)/2, then A = 0.5 * exp(-0.5 (t - t1))
    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(spelling_model(switching_k), out, format = "R"))
    env <- new.env(); source(out, local = env)
    times <- c(0, 0.2, 1, 2)
    d <- as.data.frame(deSolve::ode(y = env$InitialAmounts, times = times,
                                    func = env$massBalances, parms = env$parameters,
                                    rtol = 1e-10, atol = 1e-12))
    t1 <- log(2) / 2
    closed <- ifelse(times < t1, exp(-2 * times), 0.5 * exp(-0.5 * (times - t1)))
    expect_equal(d$A, closed, tolerance = 1e-6)
})

test_that("the switching piecewise runs in rxode2", {
    skip_on_cran()
    skip_if_not_installed("rxode2")

    out <- tempfile(fileext = ".R")
    expect_no_warning(convertReactions(spelling_model(switching_k), out, format = "nlmixr2"))
    env <- new.env(); source(out, local = env)
    times <- c(0, 0.2, 1, 2)
    suppressMessages({
        ui <- rxode2::rxode2(env$model)
        s  <- as.data.frame(rxode2::rxSolve(ui, events = rxode2::et(times),
                                            atol = 1e-12, rtol = 1e-10))
    })
    t1 <- log(2) / 2
    closed <- ifelse(times < t1, exp(-2 * times), 0.5 * exp(-0.5 * (times - t1)))
    expect_equal(s$A, closed, tolerance = 1e-5)
})

# --- the Level 3 Version 2 MathML ---------------------------------------------
#
# None of sec, arccot, quotient, rem, implies, exponentiale or the avogadro
# csymbol is a function or name in any target, and plus/times/xor may have
# zero, one or many operands.  expandMathML() rewrites them all into what every
# target has.  Each rule below is a function of A = exp(-t), with its closed
# form beside it.

l3v2_apply <- function(op, ...) paste0("<apply><", op, "/>", ..., "</apply>")
l3v2_gt <- function(x) l3v2_apply("gt", "<ci>A</ci>", sprintf("<cn>%s</cn>", x))
l3v2_if <- function(cond) sprintf(
    "<piecewise><piece><cn>1</cn>%s</piece><otherwise><cn>0</cn></otherwise></piecewise>", cond)

l3v2_functions <- list(
    sc = list(l3v2_apply("sec", "<ci>A</ci>"), function(A) 1 / cos(A)),
    ac = list(l3v2_apply("arccot", "<ci>A</ci>"), function(A) atan(1 / A)),
    ah = list(l3v2_apply("arccoth", l3v2_apply("plus", "<ci>A</ci><cn>1</cn>")),
              function(A) atanh(1 / (A + 1))),
    q  = list(l3v2_apply("quotient", l3v2_apply("times", "<ci>A</ci><cn>5</cn>"), "<cn>2</cn>"),
              function(A) floor(5 * A / 2)),
    r  = list(l3v2_apply("rem", l3v2_apply("times", "<ci>A</ci><cn>5</cn>"), "<cn>2</cn>"),
              function(A) (5 * A) %% 2),
    e  = list(l3v2_apply("times", "<exponentiale/><ci>A</ci>"), function(A) exp(1) * A),
    av = list(l3v2_apply("divide", paste0('<csymbol encoding="text" definitionURL=',
              '"http://www.sbml.org/sbml/symbols/avogadro">avogadro</csymbol>'), "<cn>1e23</cn>"),
              function(A) 6.02214179 + 0 * A),
    n  = list(l3v2_apply("plus", "<ci>A</ci>", l3v2_apply("times")), function(A) A + 1),
    # log with a base, which R reads reversed, and root, which no target has
    lb = list(l3v2_apply("log", "<logbase><cn>2</cn></logbase>",
                         l3v2_apply("plus", "<ci>A</ci><cn>3</cn>")),
              function(A) log2(A + 3)),
    lt = list(l3v2_apply("log", "<logbase><cn>10</cn></logbase>", "<ci>A</ci>"),
              function(A) log10(A)),
    r3 = list(l3v2_apply("root", "<degree><cn>3</cn></degree>", "<ci>A</ci>"),
              function(A) A^(1 / 3)),
    # an odd root of a negative number is real
    rn = list(l3v2_apply("root", "<degree><cn>3</cn></degree>",
                         l3v2_apply("minus", "<cn>0.5</cn>", "<ci>A</ci>")),
              function(A) sign(0.5 - A) * abs(0.5 - A)^(1 / 3)),
    r2 = list(l3v2_apply("root", "<degree><cn>2</cn></degree>", "<ci>A</ci>"),
              function(A) sqrt(A)))

# ubiquity has no or/not/xor, so the logical ones are kept apart
l3v2_logic <- list(
    im = list(l3v2_if(l3v2_apply("implies", l3v2_gt(0.5), l3v2_gt(0.7))),
              function(A) as.numeric(A <= 0.5 | A > 0.7)),
    x3 = list(l3v2_if(l3v2_apply("xor", l3v2_gt(0.2), l3v2_gt(0.5), l3v2_gt(0.8))),
              function(A) as.numeric(xor(xor(A > 0.2, A > 0.5), A > 0.8))),
    b  = list(l3v2_gt(0.5), function(A) as.numeric(A > 0.5)))

l3v2_model <- function(rules) spelling_model(
    paste(c(assign_rule("k", "<cn>1</cn>"),
            mapply(assign_rule, names(rules), vapply(rules, `[[`, "", 1))), collapse = "\n"),
    params = paste(sprintf('<parameter id="%s" constant="false"/>', names(rules)), collapse = ""))

l3v2_times <- c(0, 0.3, 0.6, 0.9)    # A crosses 0.8 and 0.5 in between

expect_closed_forms <- function(solution, rules, tolerance = 1e-6) {
    solution <- as.data.frame(solution)
    for (n in names(rules)) {
        expect_equal(solution[[n]], rules[[n]][[2]](solution$A), tolerance = tolerance, info = n)
    }
    expect_equal(solution$A, exp(-l3v2_times), tolerance = tolerance)
}

test_that("the Level 3 Version 2 functions and constants are written out for every target", {
    # SBML Test Suite cases 00892, 00959, 00961, 01277-01279, 01488, 01490
    out <- spelled_targets(l3v2_model(l3v2_functions))
    unspelled <- paste0("\\b(sec|arccot|arccoth|quotient|rem|plus|times|root)\\(",
                        "|\\b(exponentiale|avogadro)\\b|\\blog\\([^()]*,")
    for (fmt in names(out)) expect_false(any(grepl(unspelled, out[[fmt]])), info = fmt)
    expect_true(any(grepl("\\blt = log10\\(A\\)", out$R)))
    expect_true(any(grepl("\\br2 = sqrt\\(A\\)", out$R)))
    expect_true(any(grepl("^<Ad> r2 = SIMINT_POWER\\[A\\]\\[0.5\\]$", out$ubiquity)))
    expect_true(any(grepl("\\bq = floor\\(A \\* 5 / 2\\)", out$R)))
    expect_true(any(grepl("^<Ad> r = \\(\\(A\\*5\\.0\\)-\\(2\\.0\\*floor\\(\\(\\(A\\*5\\.0\\)/2\\.0\\)\\)\\)\\)$", out$ubiquity)))
    expect_true(any(grepl("^<Ad> e = \\(SIMINT_EXP\\[1\\.0\\]\\*A\\)$", out$ubiquity)))
})

test_that("mrgsolve literals are doubles, so a division of two is not an integer one", {
    # 1 / 2 is 0 in C++, and root(3, A) became pow(A, 1 / 3), i.e. 1
    m <- spelling_model(paste(assign_rule("k", "<cn>1</cn>"),
                              assign_rule("h", l3v2_apply("divide", "<cn>1</cn><cn>2</cn>")),
                              assign_rule("w", "<cn>3</cn>")),
                        params = '<parameter id="h" constant="false"/><parameter id="w" constant="false"/>')
    o <- tempfile(fileext = ".cpp")
    expect_no_warning(convertReactions(m, o, format = "mrgsolve"))
    code <- readLines(o)
    expect_true(any(grepl("double h = 1\\.0 / 2\\.0;", code)))
    expect_true(any(grepl("double w = 3\\.0;", code)))
    out <- spelled_targets(l3v2_model(l3v2_functions))
    expect_true(any(grepl("r3 = .*pow\\(.*, 1\\.0 / 3\\.0\\)", out$mrgsolve)))
    expect_true(any(grepl("^<Ad> r3 = .*\\(1\\.0/3\\.0\\)", out$ubiquity)))
    expect_true(any(grepl("lt = log10\\(A\\)", out$mrgsolve)))
})

test_that("implies, n-ary xor and a bare comparison are spelled for every target", {
    # SBML Test Suite cases 00201, 01279, 01290
    m <- l3v2_model(l3v2_logic)
    out <- lapply(c(R = ".R", nlmixr2 = ".R", mrgsolve = ".cpp", MATLAB = ".m", Julia = ".jl"),
                  function(ext) tempfile(fileext = ext))
    for (fmt in names(out)) {
        expect_no_warning(convertReactions(m, out[[fmt]], format = fmt))
        out[[fmt]] <- readLines(out[[fmt]])
        expect_false(any(grepl("\\bimplies\\(", out[[fmt]])), info = fmt)
    }
    # R's xor() returns a logical rxode2 rejects, and rxode2 cannot parse
    # `!` over `||`, so neither appears; C++ has no xor() at all
    for (fmt in c("R", "nlmixr2")) {
        expect_false(any(grepl("xor\\(|!|TRUE|FALSE", out[[fmt]])), info = fmt)
        expect_true(any(grepl("\\bb (=|<-) 1 \\* \\(A > 0.5\\)", out[[fmt]])), info = fmt)
    }
    expect_false(any(grepl("xor\\(", out$mrgsolve)))
    expect_true(any(grepl("xor\\(xor\\(", out$MATLAB)))
    expect_true(any(grepl("xor\\(xor\\(", out$Julia)))
    expect_warning(convertReactions(m, tempfile(fileext = ".txt"), format = "ubiquity"),
                   "no equivalent for: not, or, xor")
})

test_that("the Level 3 Version 2 math reproduces its closed forms in deSolve", {
    skip_if_not_installed("deSolve")
    rules <- c(l3v2_functions, l3v2_logic)
    out <- tempfile(fileext = ".R")
    convertReactions(l3v2_model(rules), out, format = "R")
    env <- new.env(); source(out, local = env)
    expect_closed_forms(deSolve::ode(env$InitialAmounts, l3v2_times, env$massBalances,
                                     env$parameters, rtol = 1e-10, atol = 1e-12), rules)
})

test_that("the Level 3 Version 2 math runs in mrgsolve and rxode2", {
    skip_on_cran()
    rules <- c(l3v2_functions, l3v2_logic)
    m <- l3v2_model(rules)

    if (requireNamespace("mrgsolve", quietly = TRUE)) {
        d <- tempfile(); dir.create(d)
        convertReactions(m, file.path(d, "l3v2.cpp"), format = "mrgsolve")
        mod <- mrgsolve::mread("l3v2", project = d, file = "l3v2.cpp", quiet = TRUE)
        s <- mrgsolve::mrgsim_df(mod, tgrid = l3v2_times, end = -1, rtol = 1e-10, atol = 1e-12)
        expect_closed_forms(s, rules)
    }

    if (requireNamespace("rxode2", quietly = TRUE)) {
        o <- tempfile(fileext = ".R")
        convertReactions(m, o, format = "nlmixr2")
        env <- new.env(); source(o, local = env)
        s <- suppressMessages(rxode2::rxSolve(rxode2::rxode2(env$model),
                                              events = rxode2::et(l3v2_times),
                                              atol = 1e-12, rtol = 1e-10))
        expect_closed_forms(s, rules)
    }
})
