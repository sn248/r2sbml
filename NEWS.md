# r2sbml 0.1.1

* Fixed installation with clang 23 and libc++, which failed compiling the
  bundled 'libSBML' sources because they did not include `<new>`.
* The bundled 'libSBML' is now built without zlib and bzip2 support, which
  r2sbml does not use. This fixes loading the package on CRAN's clang
  sanitizer check (undefined symbol `unzGoToFirstFile`) and drops the libbz2
  system requirement.
* Installation now stops with an error if building 'libSBML' fails. It used to
  continue and fail later, with an unrelated error when loading the package.
* Fixed undefined behaviour reported by CRAN's sanitizer checks: an out-of-range
  cast when generating 'mrgsolve' and 'ubiquity' code for very large literals,
  and invalid casts in the bundled 'libSBML' sources.
* `convertReactions()` no longer relies on 'libSBML' to create the parameter
  for a stoichiometry set by a rule. On CRAN's macOS machines, that parameter
  was missing and the conversion failed.

# r2sbml 0.1.0

* Initial CRAN submission.
* `getModel()` and the query functions list a model's species, parameters,
  compartments, reactions, rules, events and function definitions.
* `convertReactions()` writes a model as simulation code for 'deSolve',
  'mrgsolve', 'rxode2', 'MATLAB', 'Julia' or 'ubiquity'. Generated deSolve
  code is checked against the SBML Test Suite, where 1007 of the 1011 cases
  in scope pass.
