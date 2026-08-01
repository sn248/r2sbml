#ifndef R2SBML_FORMULA_TO_INFIX_H
#define R2SBML_FORMULA_TO_INFIX_H

/**
 * Serialisation of libsbml ASTs to infix text.
 *
 * libsbml ships two infix writers.  SBML_formulaToString() emits the SBML
 * Level 1 syntax, which predates most of the MathML that Level 2 and 3 models
 * actually use: it has no relational or logical operators, so `b > 1` comes
 * back as `gt(b, 1)` and `a && b` as `and(a, b)`, and it writes exponentiation
 * as `pow(a, b)`.  None of `gt`, `and` or `pow` is a function in R or rxode2,
 * so anything that reached those writers was emitting code that could not run.
 *
 * SBML_formulaToL3String() emits the Level 3 syntax, which has the operators.
 * It has one behaviour worth knowing about: it appends unit annotations to
 * literal numbers, so `3 mole` serialises as `3 mole` rather than `3`.  That
 * is correct for round-tripping SBML and useless for generating code, so the
 * settings below turn it off with setParseUnits(L3P_NO_UNITS).
 *
 * Level 3 writes exponentiation as `a^b`.  That suits R and rxode2 but not
 * mrgsolve, whose model blocks are C++, where `^` is bitwise XOR and does not
 * compile for doubles at all.  formulaToInfixC() therefore rewrites power
 * nodes into calls to pow() before serialising.  Note that the rest of the
 * Level 3 syntax is a genuine improvement for mrgsolve too: `b > 1` and
 * `a && b` are valid C++ where the Level 1 spellings were not.
 */

#include <string>
#include <cstdlib>

#include <sbml/SBMLTypes.h>
#include <sbml/math/ASTNode.h>
#include <sbml/math/L3FormulaFormatter.h>
#include <sbml/math/L3ParserSettings.h>

namespace r2sbml {

/** Level 3 writer settings shared by both serialisers: units suppressed. */
inline const LIBSBML_CPP_NAMESPACE_QUALIFIER L3ParserSettings& l3WriterSettings()
{
  static LIBSBML_CPP_NAMESPACE_QUALIFIER L3ParserSettings settings;
  static bool initialised = false;
  if (!initialised)
  {
    settings.setParseUnits(L3P_NO_UNITS);
    initialised = true;
  }
  return settings;
}

/** Serialise @p math as Level 3 infix.  Returns "" when @p math is null. */
inline std::string formulaToInfix(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  if (math == NULL) return std::string();

  char* text = LIBSBML_CPP_NAMESPACE_QUALIFIER SBML_formulaToL3StringWithSettings(
                 math, &l3WriterSettings());
  if (text == NULL) return std::string();

  std::string formula(text);
  free(text);
  return formula;
}

/**
 * Rewrite every exponentiation below @p node into a call to pow().
 *
 * Retyping to AST_FUNCTION (a plain named call) rather than
 * AST_FUNCTION_POWER is deliberate: the Level 3 writer spells both AST_POWER
 * and AST_FUNCTION_POWER as `a^b`, and only a named call comes back out as
 * `pow(a, b)`.  Children are visited first so that nested and
 * right-associative powers convert too: `a^b^c` becomes pow(a, pow(b, c)).
 */
inline void powersToCalls(LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* node)
{
  if (node == NULL) return;

  for (unsigned int i = 0; i < node->getNumChildren(); ++i)
  {
    powersToCalls(node->getChild(i));
  }

  if (node->getType() == AST_POWER || node->getType() == AST_FUNCTION_POWER)
  {
    node->setType(AST_FUNCTION);
    node->setName("pow");
  }
}

/**
 * Serialise @p math as Level 3 infix with `a^b` written as `pow(a, b)`, for
 * targets whose model code is C++.  Works on a copy, so the model is
 * untouched.  Returns "" when @p math is null.
 */
inline std::string formulaToInfixC(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  if (math == NULL) return std::string();

  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* copy = math->deepCopy();
  powersToCalls(copy);
  std::string formula = formulaToInfix(copy);
  delete copy;
  return formula;
}

} // namespace r2sbml

#endif // R2SBML_FORMULA_TO_INFIX_H
