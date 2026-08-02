#!/usr/bin/env Rscript

## Remove internal root-level markdown from the built pkgdown site.
##
## pkgdown's package_mds() globs every *.md in the package root and renders it
## to a site page.  Its exclusion list is hardcoded -- README, LICENCE, NEWS,
## the issue templates and cran-comments -- and there is no configuration key
## for anything else, so CLAUDE.md (instructions for Claude Code, not user
## documentation) lands on the public site.  The page is unlinked from the
## navbar but does appear in search.json and sitemap.xml, so a reader searching
## the site finds internal build notes.
##
## todo.md is deliberately *kept*: it is a genuine known-issues list, and the
## limitations vignette points readers at it.
##
## Run after pkgdown::build_site().  Safe to run when there is nothing to do.

drop <- c("CLAUDE")

docs <- "docs"
if (!dir.exists(docs)) {
    stop("No docs/ directory -- run pkgdown::build_site() first.", call. = FALSE)
}

removed <- character()
for (stem in drop) {
    for (f in file.path(docs, paste0(stem, c(".html", ".md")))) {
        if (file.exists(f)) {
            unlink(f)
            removed <- c(removed, f)
        }
    }
}

## search.json holds one entry per *heading*, not per page, and "path" is the
## full canonical URL (https://…/CLAUDE.html) when the site has a url: set and
## a root-relative path when it does not -- so match on the trailing filename.
search_json <- file.path(docs, "search.json")
if (file.exists(search_json) && requireNamespace("jsonlite", quietly = TRUE)) {
    idx  <- jsonlite::fromJSON(search_json, simplifyVector = FALSE)
    keep <- Filter(function(e) {
        ## not every entry carries a path; those can only be kept
        p <- e[["path"]]
        if (!is.character(p) || length(p) != 1L) return(TRUE)
        !any(vapply(drop, function(s)
            endsWith(p, paste0("/", s, ".html")), logical(1)))
    }, idx)
    if (length(keep) != length(idx)) {
        jsonlite::write_json(keep, search_json, auto_unbox = TRUE)
        removed <- c(removed, sprintf("%s (%d entr%s)", search_json,
                                      length(idx) - length(keep),
                                      if (length(idx) - length(keep) == 1) "y" else "ies"))
    }
}

## sitemap.xml holds one <url><loc>...</loc></url> block per page
sitemap <- file.path(docs, "sitemap.xml")
if (file.exists(sitemap)) {
    xml <- readLines(sitemap, warn = FALSE)
    pat <- paste0("(", paste(drop, collapse = "|"), ")\\.html")
    hit <- grepl(pat, xml)
    if (any(hit)) {
        writeLines(xml[!hit], sitemap)
        removed <- c(removed, sprintf("%s (%d line(s))", sitemap, sum(hit)))
    }
}

if (length(removed)) {
    message("pkgdown_clean: removed internal pages from the site")
    for (r in removed) message("  - ", r)
} else {
    message("pkgdown_clean: nothing to remove")
}
