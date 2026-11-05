# r2sbml 0.1.1

* Fixed installation with clang 23 and libc++, which failed compiling the
  bundled 'libSBML' sources because they did not include `<new>`.
* Installation now stops with an error if building 'libSBML' fails. It used to
  continue and fail later, with an unrelated error when loading the package.

# r2sbml 0.1.0

* Initial CRAN submission.
* `getModel()` and the query functions list a model's species, parameters,
  compartments, reactions, rules, events and function definitions.
* `convertReactions()` writes a model as simulation code for 'deSolve',
  'mrgsolve', 'rxode2', 'MATLAB', 'Julia' or 'ubiquity'. Generated deSolve
  code is checked against the SBML Test Suite, where 1007 of the 1011 cases
  in scope pass.
