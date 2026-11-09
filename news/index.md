# Changelog

## r2sbml 0.1.1

- Fixed installation with clang 23 and libc++, which failed compiling
  the bundled ‘libSBML’ sources because they did not include `<new>`.
- The package now links its bundled ‘libSBML’ by path. It used to be
  linked with `-lsbml-static`, which picked up a system ‘libSBML’
  installed by Linux distributions whenever R’s own library flags put
  the system directory first. This made the package fail to load on
  CRAN’s clang sanitizer check (undefined symbol `unzGoToFirstFile`).
- The bundled ‘libSBML’ is now built without zlib and bzip2 support,
  which r2sbml does not use, so libbz2 is no longer a system
  requirement.
- Installation now stops with an error if building ‘libSBML’ fails. It
  used to continue and fail later, with an unrelated error when loading
  the package.
- Fixed undefined behaviour reported by CRAN’s sanitizer checks: an
  out-of-range cast when generating ‘mrgsolve’ and ‘ubiquity’ code for
  very large literals, and invalid casts in the bundled ‘libSBML’
  sources.
- [`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
  no longer relies on ‘libSBML’ to create the parameter for a
  stoichiometry set by a rule. On CRAN’s macOS machines, that parameter
  was missing and the conversion failed.

## r2sbml 0.1.0

CRAN release: 2026-11-03

- Initial CRAN submission.
- [`getModel()`](https://sn248.github.io/r2sbml/reference/getModel.md)
  and the query functions list a model’s species, parameters,
  compartments, reactions, rules, events and function definitions.
- [`convertReactions()`](https://sn248.github.io/r2sbml/reference/convertReactions.md)
  writes a model as simulation code for ‘deSolve’, ‘mrgsolve’, ‘rxode2’,
  ‘MATLAB’, ‘Julia’ or ‘ubiquity’. Generated deSolve code is checked
  against the SBML Test Suite, where 1007 of the 1011 cases in scope
  pass.
