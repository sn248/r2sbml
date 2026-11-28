# r2sbml (development version)

* Models can now be edited and written back to SBML:
  `setParameterValue()`, `setCompartmentSize()`, `setSpeciesIC()` and
  `setRateLaw()` change a loaded model in place, `copyModel()` makes an
  independent copy, and `writeSBML()` validates the model and writes it to a
  file. A value setter warns when an initial assignment or assignment rule
  overrides the value it sets. `setRateLaw()` parses an infix formula,
  refuses one naming an id the reaction cannot see, and keeps the reaction's
  local parameters. `removeReaction()` deletes a reaction, refusing while
  remaining math reads its rate or one of its stoichiometries.

* Fixed a memory leak in `getModel()`. When R collected a model, the pointer
  freed the model but leaked the SBML document that owns it. It now frees the
  document, and the model with it.
* `getModel()` now refuses a file that contains no model, with an error naming
  the file. It used to return a pointer that the query functions then rejected
  with the unhelpful error "external pointer is not valid".
* The package now builds for WebAssembly, so it runs in webR (R in the
  browser). Building with webR's `rwasm` used to fail configuring the bundled
  'libSBML', because cmake searched the build machine for libxml2 and zlib.
  When the compiler is `emcc`, the build now passes cmake emscripten's
  toolchain file and the WebAssembly libxml2 and zlib.
* Installation no longer fails when `parallel::detectCores()` cannot count
  the physical cores and returns `NA`, as it did under `rwasm`. 'libSBML'
  then builds on two cores.

# r2sbml 0.1.1

* Fixed installation with clang 23 and libc++, which failed compiling the
  bundled 'libSBML' sources because they did not include `<new>`.
* The package now links its bundled 'libSBML' by path. It used to be linked
  with `-lsbml-static`, which picked up a system 'libSBML' installed by Linux
  distributions whenever R's own library flags put the system directory first.
  This made the package fail to load on CRAN's clang sanitizer check
  (undefined symbol `unzGoToFirstFile`).
* The bundled 'libSBML' is now built without zlib and bzip2 support, which
  r2sbml does not use, so libbz2 is no longer a system requirement.
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
