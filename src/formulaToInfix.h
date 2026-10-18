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
 * The four infix languages the Level 3 writer's output is adapted to.  R
 * covers rxode2 too: its model syntax takes the same spellings.
 */
enum class Dialect { R, C, Matlab, Julia };

/**
 * Name of the call a piecewise becomes.  R and Julia have a three-argument
 * ifelse(); C++ and MATLAB have no conditional *expression* (C++'s ?: has no
 * AST node to retype into, MATLAB has nothing), so they call a one-line helper
 * the writer emits alongside the model -- see usesPiecewise().
 */
inline const char* ifelseName(Dialect d)
{
  return (d == Dialect::R || d == Dialect::Julia) ? "ifelse" : "r2sbml_ifelse";
}

/** The dialect's spelling of a non-finite number. */
inline const char* infinityName(Dialect d) { return d == Dialect::C ? "INFINITY" : "Inf"; }
inline const char* nanName(Dialect d)      { return d == Dialect::C ? "NAN"      : "NaN"; }

/** A bare name node, for spellings the Level 3 writer must pass through. */
inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* nameNode(const char* name)
{
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n =
      new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(AST_NAME);
  n->setName(name);
  return n;
}

/** A call named @p name over @p args, which it takes ownership of. */
inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode*
callNode(const char* name, LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* a,
         LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* b = NULL)
{
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n =
      new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(AST_FUNCTION);
  n->setName(name);
  n->addChild(a);
  if (b != NULL) n->addChild(b);
  return n;
}

/** An operator node of @p type over @p a and @p b, taking ownership of both. */
inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode*
opNode(ASTNodeType_t type, LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* a,
       LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* b = NULL)
{
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n =
      new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(type);
  n->addChild(a);
  if (b != NULL) n->addChild(b);
  return n;
}

inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* realNode(double value)
{
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n =
      new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(AST_REAL);
  n->setValue(value);
  return n;
}

/** 1/x, for the reciprocal trigonometric functions. */
inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode*
reciprocal(LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* x)
{
  return opNode(AST_DIVIDE, realNode(1.0), x);
}

/**
 * Rewrite the MathML that no target spells into what they all do, returning
 * the subtree's new root (the caller owns the old one if it changed).  Runs
 * before rewriteForDialect() and before the ubiquity walk, so every rewrite
 * here must use only names that R, rxode2, C++, MATLAB, Julia and ubiquity
 * (which pastes into R *and* C) all have: floor, exp, cos, asin, asinh, ...
 *
 * - `exponentiale` is exp(1); the `avogadro` csymbol is SBML's fixed
 *   6.02214179e23 (not CODATA's later value), which libSBML stores on the node.
 * - sec, csc, cot and their hyperbolic forms are reciprocals; the arc*
 *   functions are asin/acos/atan/asinh/acosh/atanh, of 1/x for the reciprocal
 *   ones.  These are libSBML's own definitions (SBMLTransforms), arccot
 *   included: atan(1/x), so its range is (-pi/2, pi/2).
 * - quotient and rem round down, as libSBML evaluates them:
 *   floor(a/b) and a - b*floor(a/b).  (R's %/% and %% agree.)
 * - implies(a, b) is !a || b.
 * - plus, times, and, or and xor may have zero or one operand in Level 3
 *   Version 2; the empty forms are their identities and the single forms the
 *   operand.  xor over more than two operands nests, since R's takes two.
 */
inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode*
expandMathML(LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* node)
{
  if (node == NULL) return NULL;

  for (unsigned int i = 0; i < node->getNumChildren(); ++i)
  {
    LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* child = node->getChild(i);
    LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* now   = expandMathML(child);
    if (now != child) node->replaceChild(i, now, true);
  }

  const unsigned int kids = node->getNumChildren();
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* x =
      kids > 0 ? node->getChild(0)->deepCopy() : NULL;
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* out = NULL;

  switch (node->getType())
  {
    case AST_CONSTANT_E:      out = opNode(AST_FUNCTION_EXP, realNode(1.0));       break;
    case AST_NAME_AVOGADRO:   out = realNode(node->getReal());           break;

    case AST_FUNCTION_SEC:    out = reciprocal(opNode(AST_FUNCTION_COS, x));       x = NULL; break;
    case AST_FUNCTION_CSC:    out = reciprocal(opNode(AST_FUNCTION_SIN, x));       x = NULL; break;
    case AST_FUNCTION_COT:    out = reciprocal(opNode(AST_FUNCTION_TAN, x));       x = NULL; break;
    case AST_FUNCTION_SECH:   out = reciprocal(opNode(AST_FUNCTION_COSH, x));      x = NULL; break;
    case AST_FUNCTION_CSCH:   out = reciprocal(opNode(AST_FUNCTION_SINH, x));      x = NULL; break;
    case AST_FUNCTION_COTH:   out = reciprocal(opNode(AST_FUNCTION_TANH, x));      x = NULL; break;
    case AST_FUNCTION_ARCSIN: out = callNode("asin", x);                  x = NULL; break;
    case AST_FUNCTION_ARCCOS: out = callNode("acos", x);                  x = NULL; break;
    case AST_FUNCTION_ARCTAN: out = callNode("atan", x);                  x = NULL; break;
    case AST_FUNCTION_ARCSINH: out = callNode("asinh", x);                x = NULL; break;
    case AST_FUNCTION_ARCCOSH: out = callNode("acosh", x);                x = NULL; break;
    case AST_FUNCTION_ARCTANH: out = callNode("atanh", x);                x = NULL; break;
    case AST_FUNCTION_ARCSEC:  out = callNode("acos", reciprocal(x));     x = NULL; break;
    case AST_FUNCTION_ARCCSC:  out = callNode("asin", reciprocal(x));     x = NULL; break;
    case AST_FUNCTION_ARCCOT:  out = callNode("atan", reciprocal(x));     x = NULL; break;
    case AST_FUNCTION_ARCSECH: out = callNode("acosh", reciprocal(x));    x = NULL; break;
    case AST_FUNCTION_ARCCSCH: out = callNode("asinh", reciprocal(x));    x = NULL; break;
    case AST_FUNCTION_ARCCOTH: out = callNode("atanh", reciprocal(x));    x = NULL; break;

    case AST_FUNCTION_QUOTIENT:
    case AST_FUNCTION_REM:
      if (kids == 2)
      {
        const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* b = node->getChild(1);
        LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* q =
            opNode(AST_FUNCTION_FLOOR, opNode(AST_DIVIDE, x->deepCopy(), b->deepCopy()));
        out = node->getType() == AST_FUNCTION_QUOTIENT
                ? q : opNode(AST_MINUS, x->deepCopy(), opNode(AST_TIMES, b->deepCopy(), q));
      }
      break;

    case AST_LOGICAL_IMPLIES:
      if (kids == 2)
      {
        out = opNode(AST_LOGICAL_OR, opNode(AST_LOGICAL_NOT, x),
                     node->getChild(1)->deepCopy());
        x = NULL;
      }
      break;

    case AST_PLUS:
    case AST_TIMES:
    case AST_LOGICAL_AND:
    case AST_LOGICAL_OR:
    case AST_LOGICAL_XOR:
      if (kids == 1) { out = x; x = NULL; }
      else if (kids == 0)
      {
        const ASTNodeType_t t = node->getType();
        out = t == AST_PLUS  ? realNode(0.0)
            : t == AST_TIMES ? realNode(1.0)
            : new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(
                  t == AST_LOGICAL_AND ? AST_CONSTANT_TRUE : AST_CONSTANT_FALSE);
      }
      else if (node->getType() == AST_LOGICAL_XOR && kids > 2)
      {
        out = x;
        for (unsigned int i = 1; i < kids; ++i)
          out = opNode(AST_LOGICAL_XOR, out, node->getChild(i)->deepCopy());
        x = NULL;
      }
      break;

    default: break;
  }

  delete x;
  return out != NULL ? out : node;
}

/**
 * `!x`, spelled `x == 0` for R: rxode2 cannot parse `!` applied to an `&&`
 * or `||` expression (`!((a) || (b))` is a syntax error there), and the two
 * agree for numbers and logicals alike.  Takes ownership of @p x.
 */
inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode*
negation(LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* x, Dialect d)
{
  return d == Dialect::R ? opNode(AST_RELATIONAL_EQ, x, realNode(0.0))
                         : opNode(AST_LOGICAL_NOT, x);
}

/**
 * Rewrite the subtree at @p node into what @p d can run, and return its new
 * root -- @p node itself unless the root had to be replaced, in which case
 * the caller owns the old one.  Children are rewritten first, so every
 * construct below composes: a power or a piecewise nested anywhere converts.
 *
 * - `ln(x)` is `log(x)` in all four.  (MathML <log> already serialises as
 *   `log10(x)`, which all four understand.)
 * - `a^b` is bitwise XOR in C++, so it becomes pow(a, b).  The node is
 *   retyped to AST_FUNCTION (a plain named call) rather than
 *   AST_FUNCTION_POWER, because the Level 3 writer spells both power types as
 *   `a^b`; only a named call comes back out as `pow(a, b)`.
 * - `ceiling` serialises as `ceil`, which is right in C++, MATLAB and Julia;
 *   R's function is `ceiling`.  `true`/`false` are 1 and 0 in R (rxode2 has no TRUE).
 * - Infinity and NaN serialise as `INF` and `NaN`.  `INF` is a number in none
 *   of the four, and C++ needs `NAN` too.
 * - A relational with more than two operands is a chain: MathML's
 *   `gt(a, b, c)` means a > b > c, i.e. a > b && b > c.  The Level 3 writer
 *   emits `a > b > c`, a syntax error in R and the wrong comparison in C++,
 *   MATLAB and Julia, so it becomes the `&&` of the adjacent pairs.  With one
 *   operand it is vacuously true.
 * - piecewise(v1, c1, v2, c2, ..., otherwise) serialises unchanged, and is a
 *   function in none of the four.  It becomes a nested three-argument call,
 *   ifelse(c1, v1, ifelse(c2, v2, otherwise)), evaluated eagerly -- which is
 *   what R's ifelse does anyway.  With no `otherwise`, SBML leaves the value
 *   undefined when no condition holds, so the last fallback is NaN.
 */
inline LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode*
rewriteForDialect(LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* node, Dialect d)
{
  if (node == NULL) return NULL;

  for (unsigned int i = 0; i < node->getNumChildren(); ++i)
  {
    LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* child = node->getChild(i);
    LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* now   = rewriteForDialect(child, d);
    if (now != child) node->replaceChild(i, now, true);
  }

  switch (node->getType())
  {
    case AST_FUNCTION_LN:
      node->setType(AST_FUNCTION);
      node->setName("log");
      return node;

    case AST_POWER:
    case AST_FUNCTION_POWER:
      if (d == Dialect::C)
      {
        node->setType(AST_FUNCTION);
        node->setName("pow");
      }
      return node;

    case AST_FUNCTION_CEILING:
      if (d == Dialect::R)
      {
        node->setType(AST_FUNCTION);
        node->setName("ceiling");
      }
      return node;

    // `true`/`false` are right in C++, MATLAB and Julia.  R's TRUE/FALSE are
    // not names rxode2 knows -- it asks for them as parameters -- so the R
    // dialect writes 1 and 0, which R accepts anywhere a logical goes.
    case AST_CONSTANT_TRUE:
      return d == Dialect::R ? realNode(1.0) : node;
    case AST_CONSTANT_FALSE:
      return d == Dialect::R ? realNode(0.0) : node;

    // C++ has neither an xor() nor a pi.  R's xor() returns a logical, which
    // rxode2 rejects ("needs to return a length 1 numeric"), so R takes the
    // C spelling of xor too; MATLAB and Julia have both.  It is
    // (a && !b) || (!a && b) rather than !a != !b, which rxode2 cannot parse,
    // or a != b, which is wrong for numeric operands.  expandMathML() has
    // already made every xor binary.
    case AST_LOGICAL_XOR:
      if ((d == Dialect::C || d == Dialect::R) && node->getNumChildren() == 2)
      {
        const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* a = node->getChild(0);
        const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* b = node->getChild(1);
        return opNode(AST_LOGICAL_OR,
                      opNode(AST_LOGICAL_AND, a->deepCopy(), negation(b->deepCopy(), d)),
                      opNode(AST_LOGICAL_AND, negation(a->deepCopy(), d), b->deepCopy()));
      }
      return node;

    case AST_LOGICAL_NOT:
      if (d == Dialect::R && node->getNumChildren() == 1)
        return negation(node->getChild(0)->deepCopy(), d);
      return node;
    case AST_CONSTANT_PI:
      return d == Dialect::C ? opNode(AST_TIMES, realNode(4.0), callNode("atan", realNode(1.0)))
                             : node;

    case AST_FUNCTION_PIECEWISE:
    {
      const unsigned int kids  = node->getNumChildren();
      const unsigned int pairs = kids / 2;

      LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* rest =
          (kids % 2 == 1) ? node->getChild(kids - 1)->deepCopy() : nameNode(nanName(d));
      for (unsigned int p = pairs; p-- > 0; )
      {
        LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* call =
            new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(AST_FUNCTION);
        call->setName(ifelseName(d));
        call->addChild(node->getChild(2 * p + 1)->deepCopy());   // condition
        call->addChild(node->getChild(2 * p)->deepCopy());       // value
        call->addChild(rest);
        rest = call;
      }
      return rest;
    }

    default: break;
  }

  if (node->isRelational() && node->getNumChildren() != 2)
  {
    const unsigned int kids = node->getNumChildren();
    if (kids < 2)
    {
      // 1 rather than TRUE in R, as for AST_CONSTANT_TRUE above.
      return d == Dialect::R ? realNode(1.0)
                             : new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(AST_CONSTANT_TRUE);
    }
    LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* all =
        new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(AST_LOGICAL_AND);
    for (unsigned int i = 0; i + 1 < kids; ++i)
    {
      LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* pair =
          new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(node->getType());
      pair->addChild(node->getChild(i)->deepCopy());
      pair->addChild(node->getChild(i + 1)->deepCopy());
      all->addChild(pair);
    }
    return all;
  }

  if (node->isNumber())
  {
    if (node->isNaN()) return nameNode(nanName(d));
    if (node->isInfinity()) return nameNode(infinityName(d));
    if (node->isNegInfinity())
    {
      LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* minus =
          new LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode(AST_MINUS);
      minus->addChild(nameNode(infinityName(d)));
      return minus;
    }
  }
  return node;
}

/**
 * Serialise @p math for @p d.  Works on a copy, so the model is untouched.
 *
 * Every caller writes the result where a number is expected -- a rule's
 * value, a derivative -- so a formula that is itself a comparison or a
 * boolean is multiplied by 1: deSolve rejects a logical derivative outright
 * (SBML Test Suite case 01290, a rate rule of `true`).
 */
inline std::string formulaToDialect(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math,
                                    Dialect d)
{
  if (math == NULL) return std::string();

  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* copy = math->deepCopy();
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* root = expandMathML(copy);
  if (root != copy) delete copy;
  if (root->isBoolean()) root = opNode(AST_TIMES, realNode(1.0), root);

  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* out = rewriteForDialect(root, d);
  std::string formula = serialiseL3(out);
  if (out != root) delete root;
  delete out;
  return formula;
}

/** Serialise @p math as Level 3 infix for R and rxode2.  "" when null. */
inline std::string formulaToInfix(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  return formulaToDialect(math, Dialect::R);
}

/**
 * Serialise @p math for targets whose model code is C++, with `a^b` written
 * as `pow(a, b)`.  Returns "" when @p math is null.
 */
inline std::string formulaToInfixC(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  return formulaToDialect(math, Dialect::C);
}

/** Serialise @p math for Julia, which differs from R only in `ceil`. */
inline std::string formulaToInfixJulia(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  return formulaToDialect(math, Dialect::Julia);
}

/** True when a piecewise occurs anywhere below @p node. */
inline bool hasPiecewise(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* node)
{
  if (node == NULL) return false;
  if (node->getType() == AST_FUNCTION_PIECEWISE) return true;
  for (unsigned int i = 0; i < node->getNumChildren(); ++i)
  {
    if (hasPiecewise(node->getChild(i))) return true;
  }
  return false;
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
  std::string formula = formulaToDialect(math, Dialect::Matlab);

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
 * piecewise never reaches here: ubiquity's conditional is a statement, not an
 * expression, so the writer hoists each one into a secondary parameter first
 * (see hoistPiecewise() in convertReactions.cpp).
 *
 * Anything with no ubiquity spelling -- root, factorial, delay, and the
 * trigonometric functions -- is emitted as a plain `name(args)` call
 * and flagged by ubiquityUnsupported() so the caller can warn.  ubiquity will
 * reject those files, which is the honest outcome: silently emitting
 * something that parses but computes the wrong thing would be worse.
 */
inline std::string formulaToUbiquityNode(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math);

/** Names of constructs in @p math that formulaToUbiquity cannot translate. */
inline void ubiquityUnsupportedNode(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* node,
                                std::vector<std::string>& found)
{
  if (node == NULL) return;

  switch (node->getType())
  {
    case AST_FUNCTION_ROOT:      found.push_back("root");      break;
    case AST_FUNCTION_FACTORIAL: found.push_back("factorial"); break;
    case AST_FUNCTION_DELAY:     found.push_back("delay");     break;
    case AST_LOGICAL_OR:         found.push_back("or");        break;
    case AST_LOGICAL_NOT:        found.push_back("not");       break;
    case AST_LOGICAL_XOR:        found.push_back("xor");       break;
    case AST_FUNCTION:
    {
      // A named call libsbml did not recognise; delay arrives here too when
      // the csymbol was not resolved.  The inverse trigonometric calls
      // expandMathML() writes are spelled alike in R and C, so they pass.
      const std::string name = node->getName() ? node->getName() : "unknown function";
      if (name != "asin" && name != "acos" && name != "atan" &&
          name != "asinh" && name != "acosh" && name != "atanh")
        found.push_back(name);
      break;
    }
    default: break;
  }

  for (unsigned int i = 0; i < node->getNumChildren(); ++i)
  {
    ubiquityUnsupportedNode(node->getChild(i), found);
  }
}

/** Run @p f over expandMathML() of a copy of @p math. */
template <typename F>
inline void withExpandedMath(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math, F f)
{
  if (math == NULL) { f(NULL); return; }
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* copy = math->deepCopy();
  LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* root = expandMathML(copy);
  if (root != copy) delete copy;
  f(root);
  delete root;
}

inline void ubiquityUnsupported(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math,
                                std::vector<std::string>& found)
{
  withExpandedMath(math, [&](const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n) {
    ubiquityUnsupportedNode(n, found);
  });
}

inline std::string formulaToUbiquity(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* math)
{
  std::string out;
  withExpandedMath(math, [&](const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n) {
    out = formulaToUbiquityNode(n);
  });
  return out;
}

inline std::string formulaToUbiquityNode(const LIBSBML_CPP_NAMESPACE_QUALIFIER ASTNode* n)
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
      return "(SIMINT_LOGN[" + formulaToUbiquityNode(n->getChild(1)) +
             "]/SIMINT_LOGN[" + formulaToUbiquityNode(n->getChild(0)) + "])";
    }
  }

  if (unary != NULL && kids == 1)
  {
    return std::string(unary) + "[" + formulaToUbiquityNode(n->getChild(0)) + "]";
  }
  if (binary != NULL && kids == 2)
  {
    return std::string(binary) + "[" + formulaToUbiquityNode(n->getChild(0)) +
           "][" + formulaToUbiquityNode(n->getChild(1)) + "]";
  }
  // A relational chain, a > b > c, is the AND of its adjacent pairs; with one
  // operand it is vacuously true.
  if (n->isRelational() && kids != 2)
  {
    if (kids < 2) return "1";
    std::string acc;
    for (unsigned int i = 0; i + 1 < kids; ++i)
    {
      const std::string pair = std::string(binary ? binary : "SIMINT_NE") + "[" +
                               formulaToUbiquity(n->getChild(i)) + "][" +
                               formulaToUbiquity(n->getChild(i + 1)) + "]";
      acc = acc.empty() ? pair : "SIMINT_AND[" + acc + "][" + pair + "]";
    }
    return acc;
  }
  // AND over more than two operands nests pairwise.
  if (binary != NULL && kids > 2)
  {
    std::string acc = formulaToUbiquityNode(n->getChild(0));
    for (unsigned int i = 1; i < kids; ++i)
    {
      acc = std::string(binary) + "[" + acc + "][" +
            formulaToUbiquityNode(n->getChild(i)) + "]";
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
        return "(-" + formulaToUbiquityNode(n->getChild(0)) + ")";
      }
      if (kids == 0) return std::string();

      std::string acc = "(" + formulaToUbiquityNode(n->getChild(0));
      for (unsigned int i = 1; i < kids; ++i)
      {
        acc += op + formulaToUbiquityNode(n->getChild(i));
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

  // ubiquity pastes anything outside its SIMINT_* table verbatim into both
  // the R and the C model it generates, so a spelling has to be valid in
  // both.  No name for infinity, NaN or the ceiling function is; IEEE
  // division and floor() are.
  if (n->isNumber() && n->isNaN())         return "(0.0/0.0)";
  if (n->isNumber() && n->isInfinity())    return "(1.0/0.0)";
  if (n->isNumber() && n->isNegInfinity()) return "(-(1.0/0.0))";
  if (n->getType() == AST_FUNCTION_CEILING && kids == 1)
  {
    return "(-floor(-(" + formulaToUbiquityNode(n->getChild(0)) + ")))";
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
    acc += formulaToUbiquityNode(n->getChild(i));
  }
  return acc + ")";
}

} // namespace r2sbml

#endif // R2SBML_FORMULA_TO_INFIX_H
