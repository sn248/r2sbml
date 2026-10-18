# Runs one SBML Test Suite case (https://github.com/sbmlteam/sbml-test-suite)
# through convertReactions(format = "R") and deSolve, and compares the result
# with the case's expected time course.
#
# A semantic case directory <id>/ holds <id>-sbml-l3v2.xml (the model),
# <id>-settings.txt (time span, the variables to check, and the absolute and
# relative tolerances) and <id>-results.csv (the expected values).  Those three
# files are all this needs, so a case is added to the shipped regression set in
# sbml-test-suite/ just by copying its directory there.
#
# Used both by the shipped cases in test-sbml-test-suite.R and by the opt-in
# run over the whole suite in the same file.

# key: value lines; the list-valued keys are comma separated and may be empty.
sts_settings <- function(file) {
    s  <- readLines(file, warn = FALSE)
    s  <- s[grepl(":", s)]
    kv <- setNames(trimws(sub("^[^:]*:", "", s)), trimws(sub(":.*", "", s)))
    split <- function(x) {
        if (is.na(x)) return(character())
        x <- trimws(strsplit(x, ",")[[1]])
        x[nzchar(x)]
    }
    list(start    = as.numeric(kv[["start"]]),
         duration = as.numeric(kv[["duration"]]),
         steps    = as.numeric(kv[["steps"]]),
         vars     = split(kv["variables"]),
         abs      = as.numeric(kv[["absolute"]]),
         rel      = as.numeric(kv[["relative"]]),
         amount   = split(kv["amount"]),
         conc     = split(kv["concentration"]))
}

# The attributes of every <tag ...> element, as a list of named character
# vectors.  A regex is enough for the few attributes needed here and keeps the
# tests free of an XML-parser dependency; getSpeciesTable() does not report
# hasOnlySubstanceUnits.
sts_elements <- function(xml, tag) {
    els <- regmatches(xml, gregexpr(sprintf("<%s\\b[^>]*>", tag), xml))[[1]]
    lapply(els, function(e) {
        m <- regmatches(e, gregexpr('\\b([A-Za-z]+)="([^"]*)"', e))[[1]]
        setNames(sub('^[A-Za-z]+="(.*)"$', "\\1", m), sub("=.*", "", m))
    })
}

# Returns list(status = "pass" | "fail" | "error", detail = <why>).
sts_run_case <- function(dir) {
    id   <- basename(dir)
    file <- function(suffix) file.path(dir, paste0(id, suffix))
    st   <- sts_settings(file("-settings.txt"))
    xmlf <- file("-sbml-l3v2.xml")

    rf  <- tempfile(fileext = ".R")
    on.exit(unlink(rf))
    err <- tryCatch({
        utils::capture.output(suppressWarnings(convertReactions(xmlf, rf, format = "R")))
        NULL
    }, error = function(e) paste("convertReactions:", conditionMessage(e)))
    if (!is.null(err)) return(list(status = "error", detail = err))

    env <- new.env()
    err <- tryCatch({ suppressWarnings(sys.source(rf, envir = env)); NULL },
                    error = function(e) paste("source:", conditionMessage(e)))
    if (!is.null(err)) return(list(status = "error", detail = err))

    times <- seq(st$start, st$start + st$duration, length.out = st$steps + 1)
    # A model carrying algebraic rules is written as a DAE for daspk.
    dae <- exists("InitialDerivatives", envir = env, inherits = FALSE)
    sol <- tryCatch(suppressWarnings(
        if (dae) {
            deSolve::daspk(y = env$InitialAmounts, dy = env$InitialDerivatives,
                           times = times, res = env$massBalances,
                           parms = env$parameters, atol = 1e-12, rtol = 1e-10)
        } else {
            deSolve::ode(env$InitialAmounts, times, env$massBalances,
                         env$parameters, atol = 1e-12, rtol = 1e-10)
        }), error = function(e) e)
    if (inherits(sol, "error"))
        return(list(status = "error", detail = paste("solver:", conditionMessage(sol))))
    sol <- as.data.frame(sol)
    if (nrow(sol) != length(times))
        return(list(status = "error", detail = "solver stopped early"))

    # A species state is a concentration unless hasOnlySubstanceUnits is set;
    # results.csv reports each variable as an amount or a concentration as
    # settings.txt says, so convert through the compartment volume.
    xml <- paste(readLines(xmlf, warn = FALSE), collapse = " ")
    vol <- vapply(sts_elements(xml, "compartment"), function(a)
        if (is.na(a["size"])) 1 else as.numeric(a[["size"]]), 0)
    names(vol) <- vapply(sts_elements(xml, "compartment"), `[[`, "", "id")
    sp      <- sts_elements(xml, "species")
    sp_id   <- vapply(sp, `[[`, "", "id")
    sp_cmt  <- setNames(vapply(sp, `[[`, "", "compartment"), sp_id)
    sp_hosu <- setNames(vapply(sp, function(a) isTRUE(a["hasOnlySubstanceUnits"] == "true"), TRUE), sp_id)

    expected <- utils::read.csv(file("-results.csv"), check.names = FALSE)
    names(expected) <- trimws(names(expected))
    # The first column of each is the time axis, and a variable may be named
    # `time` or `Time` itself (01820, 01821), so neither is looked up.
    expected <- expected[-1]
    sol      <- sol[-1]

    bad <- character()
    for (v in st$vars) {
        if (v %in% names(sol)) {
            got <- sol[[v]]
        } else if (v %in% names(env$parameters)) {
            got <- rep(env$parameters[[v]], length(times))
        } else if (exists(v, envir = env, inherits = FALSE)) {
            got <- rep(get(v, envir = env), length(times))
        } else {
            return(list(status = "fail", detail = paste(v, "is not in the solution")))
        }
        if (v %in% sp_id) {
            cid <- sp_cmt[[v]]
            # The generated file's own value, not the XML's size attribute,
            # which an initial assignment may override (00471 and others).
            V <- if (cid %in% names(sol)) sol[[cid]]
                 else if (exists(cid, envir = env, inherits = FALSE)) get(cid, envir = env)
                 else vol[[cid]]
            if (v %in% st$amount && !sp_hosu[[v]]) got <- got * V
            if (v %in% st$conc   &&  sp_hosu[[v]]) got <- got / V
        }
        # A finite expected value must be matched within the case's absolute
        # plus relative tolerance.  A NaN or Inf one -- the suite has a few --
        # must be matched exactly, rather than dropping out of the comparison.
        e      <- expected[[v]]
        finite <- is.finite(e)
        off    <- abs(got - e) - (st$abs + st$rel * abs(e))
        same   <- (is.na(e) & is.na(got)) | (!is.na(e) & !is.na(got) & got == e)
        miss   <- ifelse(finite, !is.finite(got) | off > 0, !same)
        if (any(miss)) {
            worst <- if (any(finite)) max(abs(got - e)[finite] / pmax(abs(e[finite]), 1e-300)) else NaN
            bad <- c(bad, sprintf("%s (max relative error %.3g)", v, worst))
        }
    }
    if (length(bad)) return(list(status = "fail", detail = paste(bad, collapse = ", ")))
    list(status = "pass", detail = "")
}

# Constructs convertReactions does not translate at all.  The full-suite run
# skips cases tagged with them; see todo.md.
sts_unsupported_tags <- c("EventNoDelay", "EventWithDelay", "EventPriority",
                          "CSymbolDelay", "FastReaction", "CSymbolRateOf",
                          "StoichiometryMath", "RandomEventExecution")

# TRUE when the full suite's case `dir` is in scope: an L3V2 file exists, no
# SBML package is used, and none of sts_unsupported_tags applies.  Reads the
# tags from <id>-model.m, which the shipped cases do not carry.
sts_in_scope <- function(dir) {
    id <- basename(dir)
    if (!file.exists(file.path(dir, paste0(id, "-sbml-l3v2.xml")))) return(FALSE)
    m <- readLines(file.path(dir, paste0(id, "-model.m")), warn = FALSE)
    tags <- function(key) {
        l <- grep(paste0("^", key, ":"), m, value = TRUE)
        if (!length(l)) return(character())
        x <- trimws(strsplit(sub(paste0("^", key, ":"), "", l[1]), ",")[[1]])
        x[nzchar(x)]
    }
    if (length(tags("packagesPresent"))) return(FALSE)
    !length(intersect(c(tags("componentTags"), tags("testTags")), sts_unsupported_tags))
}
