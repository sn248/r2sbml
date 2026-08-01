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
#include <vector>

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

/**
 * Serialise @p math in ubiquity's model-definition syntax.
 *
 * ubiquity is the one target that cannot be reached by fixing up the Level 3
 * writer's output.  It spells exponentiation, the transcendental functions and
 * every comparison as bracketed prefix calls -- `SIMINT_POWER[a][b]`,
 * `SIMINT_LOGN[a]`, `SIMINT_GT[a][b]` -- so there is no infix text to patch,
 * and delegating a subtree to the Level 3 writer does not compose: a power
 * nested anywhere inside would come back out as `a^b`.  Hence a full walk.
 *
 * Arithmetic stays infix.  Every operand is parenthesised rather than tracking
 * precedence, which is verbose but cannot get the grouping wrong.
 *
 * Anything with no ubiquity spelling -- root, piecewise, factorial, delay,
 * and the trigonometric functions -- is emitted as a plain `name(args)` call
 * and flagged by ubiquityUnsupported() so the caller can warn.  ubiquity will
 * reject those files, which is the honest outcome: silently emitting
 * something that parses but computes the wrong thing would be worse.
 */
inline std::string formulaToUbiquity(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math);

/** Names of constructs in @p math that formulaToUbiquity cannot translate. */
inline void ubiquityUnsupported(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* node,
                                std::vector<std::string>& found)
{
  if (node == NULL) return;

  switch (node->getType())
  {
    case AST_FUNCTION_ROOT:      found.push_back("root");      break;
    case AST_FUNCTION_PIECEWISE: found.push_back("piecewise"); break;
    case AST_FUNCTION_FACTORIAL: found.push_back("factorial"); break;
    case AST_FUNCTION_DELAY:     found.push_back("delay");     break;
    case AST_LOGICAL_OR:         found.push_back("or");        break;
    case AST_LOGICAL_NOT:        found.push_back("not");       break;
    case AST_LOGICAL_XOR:        found.push_back("xor");       break;
    case AST_FUNCTION:
      // A named call libsbml did not recognise; delay arrives here too when
      // the csymbol was not resolved.
      found.push_back(node->getName() ? node->getName() : "unknown function");
      break;
    default: break;
  }

  for (unsigned int i = 0; i < node->getNumChildren(); ++i)
  {
    ubiquityUnsupported(node->getChild(i), found);
  }
}

inline std::string formulaToUbiquity(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n)
{
  if (n == NULL) return std::string();

  const unsigned int kids = n->getNumChildren();

  // SIMINT_NAME[a] and SIMINT_NAME[a][b] forms.
  const char* unary  = NULL;
  const char* binary = NULL;

  switch (n->getType())
  {
    case AST_POWER:
    case AST_FUNCTION_POWER:    binary = "SIMINT_POWER";  break;
    case AST_RELATIONAL_LT:     binary = "SIMINT_LT";     break;
    case AST_RELATIONAL_LEQ:    binary = "SIMINT_LE";     break;
    case AST_RELATIONAL_GT:     binary = "SIMINT_GT";     break;
    case AST_RELATIONAL_GEQ:    binary = "SIMINT_GE";     break;
    case AST_RELATIONAL_EQ:     binary = "SIMINT_EQ";     break;
    case AST_LOGICAL_AND:       binary = "SIMINT_AND";    break;
    case AST_FUNCTION_EXP:      unary  = "SIMINT_EXP";    break;
    case AST_FUNCTION_LN:       unary  = "SIMINT_LOGN";   break;
    default: break;
  }

  // MathML <log> is base 10 when given one argument and base-b with two.
  if (n->getType() == AST_FUNCTION_LOG)
  {
    if (kids == 1) unary = "SIMINT_LOG10";
    else if (kids == 2)
    {
      // log_b(x) has no ubiquity spelling; write it as a ratio of logs.
      return "(SIMINT_LOGN[" + formulaToUbiquity(n->getChild(1)) +
             "]/SIMINT_LOGN[" + formulaToUbiquity(n->getChild(0)) + "])";
    }
  }

  if (unary != NULL && kids == 1)
  {
    return std::string(unary) + "[" + formulaToUbiquity(n->getChild(0)) + "]";
  }
  if (binary != NULL && kids == 2)
  {
    return std::string(binary) + "[" + formulaToUbiquity(n->getChild(0)) +
           "][" + formulaToUbiquity(n->getChild(1)) + "]";
  }
  // AND over more than two operands nests pairwise.
  if (binary != NULL && kids > 2)
  {
    std::string acc = formulaToUbiquity(n->getChild(0));
    for (unsigned int i = 1; i < kids; ++i)
    {
      acc = std::string(binary) + "[" + acc + "][" +
            formulaToUbiquity(n->getChild(i)) + "]";
    }
    return acc;
  }

  switch (n->getType())
  {
    case AST_PLUS:
    case AST_TIMES:
    case AST_DIVIDE:
    case AST_MINUS:
    {
      const char op = (n->getType() == AST_PLUS)  ? '+'
                    : (n->getType() == AST_TIMES) ? '*'
                    : (n->getType() == AST_DIVIDE) ? '/' : '-';

      if (n->getType() == AST_MINUS && kids == 1)
      {
        return "(-" + formulaToUbiquity(n->getChild(0)) + ")";
      }
      if (kids == 0) return std::string();

      std::string acc = "(" + formulaToUbiquity(n->getChild(0));
      for (unsigned int i = 1; i < kids; ++i)
      {
        acc += op + formulaToUbiquity(n->getChild(i));
      }
      return acc + ")";
    }

    // The simulation time is an internal variable, not a name to pass through.
    case AST_NAME_TIME:      return "SIMINT_TIME";
    case AST_CONSTANT_E:     return "SIMINT_EXP[1]";
    case AST_CONSTANT_PI:    return "3.14159265358979";
    case AST_CONSTANT_TRUE:  return "1";
    case AST_CONSTANT_FALSE: return "0";

    default: break;
  }

  if (n->isNumber() || n->isName())
  {
    // Numbers and identifiers have the same spelling in both languages, so
    // let the Level 3 writer format them.
    return serialiseL3(n);
  }

  // Unrecognised: emit a call and let ubiquityUnsupported() flag it.
  std::string name = n->getName() ? n->getName() : "SIMINT_UNKNOWN";
  std::string acc  = name + "(";
  for (unsigned int i = 0; i < kids; ++i)
  {
    if (i) acc += ", ";
    acc += formulaToUbiquity(n->getChild(i));
  }
  return acc + ")";
}

} // namespace r2sbml

#endif // R2SBML_FORMULA_TO_INFIX_H
