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

/** Raw Level 3 serialisation, no dialect fixes applied. */
inline std::string serialiseL3(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
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
 * Rename natural log below @p node from `ln` to `log`.
 *
 * MathML's <ln> serialises as `ln(x)`, which is not a function in R, rxode2,
 * MATLAB, Julia or C++ -- all five spell natural log `log`.  (Base-10 log is
 * unaffected: MathML <log> already serialises as `log10(x)`, which all five
 * understand.)
 */
inline void naturalLogToLog(LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* node)
{
  if (node == NULL) return;

  for (unsigned int i = 0; i < node->getNumChildren(); ++i)
  {
    naturalLogToLog(node->getChild(i));
  }

  if (node->getType() == AST_FUNCTION_LN)
  {
    node->setType(AST_FUNCTION);
    node->setName("log");
  }
}

/** Serialise @p math as Level 3 infix.  Returns "" when @p math is null. */
inline std::string formulaToInfix(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  if (math == NULL) return std::string();

  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* copy = math->deepCopy();
  naturalLogToLog(copy);
  std::string formula = serialiseL3(copy);
  delete copy;
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
  naturalLogToLog(copy);
  powersToCalls(copy);
  std::string formula = serialiseL3(copy);
  delete copy;
  return formula;
}

/**
 * Serialise @p math as Level 3 infix in MATLAB's spelling.
 *
 * MATLAB writes logical negation `~` and inequality `~=`, where the Level 3
 * writer emits `!` and `!=`.  In MATLAB `!` is the shell-escape prefix and is
 * a syntax error inside an expression, so the substitution is required.
 *
 * Rewriting the text is safe here, unlike the `^` case which needed the AST:
 * an SBML identifier is [A-Za-z_][A-Za-z0-9_]* so it can never contain `!`,
 * and formula output has no string literals.  Every `!` in the serialised
 * text is therefore either the negation operator or the head of `!=`.
 * Order matters -- `!=` has to go first, or it would become `~!=`.
 */
inline std::string formulaToInfixMatlab(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  std::string formula = formulaToInfix(math);

  std::string::size_type at = 0;
  while ((at = formula.find("!=", at)) != std::string::npos)
  {
    formula.replace(at, 2, "~=");
    at += 2;
  }
  for (std::string::size_type i = 0; i < formula.size(); ++i)
  {
    if (formula[i] == '!') formula[i] = '~';
  }

  return formula;
}

} // namespace r2sbml

#endif // R2SBML_FORMULA_TO_INFIX_H
