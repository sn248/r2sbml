#! /bin/sh

: ${R_HOME=$(R RHOME)}
RSCRIPT_BIN=${R_HOME}/bin/Rscript
NCORES=`${RSCRIPT_BIN} -e "cat(min(2, parallel::detectCores(logical = FALSE)))"`

cd src
SRC_DIR=$(pwd)

# Always remove build artefacts on exit (even on error) so R CMD check does
# not see cmake-generated Makefiles or downloaded libsbml sources.
# Uses absolute path so the trap fires correctly even if CWD changed (e.g.
# when cmake_call.sh exits from inside libsbml-build/).
cleanup() { rm -fr "${SRC_DIR}/libsbml-src" "${SRC_DIR}/libsbml-build"; }
trap cleanup EXIT

#### CMAKE CONFIGURATION ####
. ./scripts/cmake_config.sh

# Download and extract libsbml source
sh ./scripts/libsbml_download.sh ${RSCRIPT_BIN}
if [ $? -ne 0 ]; then
    exit 1
fi

# CRAN compliance patches ########################################################
#
# CRAN policy (Writing R Extensions §1.6.4) forbids compiled code that calls
# entry points which may terminate R, write to stdout/stderr instead of the
# console, use Fortran I/O, system RNGs, or [v]sprintf.
#
# All patches use 'perl -pi -e' / 'perl -ni -e' (not 'sed -i') for portability
# across Linux and macOS (which use different -i behaviour).

# crypt.h: srand() and rand() used to seed ZIP encryption header randomness.
# Replace srand() with a no-op and rand() with a constant.
perl -pi -e 's/        srand\(\(unsigned\)\(time\(NULL\) \^ ZCR_SEED2\)\);/        \/* CRAN: srand removed *\//g' \
    libsbml-src/src/sbml/compress/crypt.h

perl -pi -e 's/        c = \(rand\(\) >> 7\) & 0xff;/        c = 0x42; \/* CRAN: rand removed *\//g' \
    libsbml-src/src/sbml/compress/crypt.h

# L3Parser.cpp: YYFPRINTF is a bison debug macro defined as fprintf.
# Redefine it as a no-op so the stderr symbol is not referenced.
perl -pi -e 's|#  define YYFPRINTF fprintf|#  define YYFPRINTF(file, ...) /* CRAN: stderr removed */|g' \
    libsbml-src/src/sbml/math/L3Parser.cpp

# ASTNode.cpp: printNodeLevels() uses cout to print debug info.
perl -pi -e 's|        cout << "level:" << it->first << " " << SBML_formulaToL3String\(it->second\) << endl;|        /* CRAN: cout removed */|g' \
    libsbml-src/src/sbml/math/ASTNode.cpp

# SBMLRateRuleConverter.cpp:
#   - Remove active debug cout << statements (all single-line).
#   - Replace sprintf with snprintf to remove __sprintf_chk symbol.
perl -ni -e 'print unless /^[ \t]+cout\s*<</' \
    libsbml-src/src/sbml/conversion/SBMLRateRuleConverter.cpp
perl -pi -e 's/sprintf\(number, /snprintf(number, sizeof(number), /g' \
    libsbml-src/src/sbml/conversion/SBMLRateRuleConverter.cpp

# XMLOutputStream.cpp:
#   - Factory functions XMLOutputStream_createAsStdout* write to std::cout.
#     Return NULL (safe: callers already handle NULL from new(nothrow)).
#   - Replace sprintf with snprintf for the date/time formatting.
#   -0777 slurps the whole file so the multiline match works.
perl -0777 -pi -e \
    's/return new\(nothrow\) XMLOutputStream\(std::cout.*?;\n/return NULL; \/* CRAN: std::cout removed *\/\n/sg' \
    libsbml-src/src/sbml/xml/XMLOutputStream.cpp
perl -pi -e 's/sprintf\(formattedDateAndTime,/snprintf(formattedDateAndTime, 17,/g' \
    libsbml-src/src/sbml/xml/XMLOutputStream.cpp

# Model.cpp: three sprintf calls that generate internal IDs.
# Replace with snprintf using sizeof(buffer).
perl -pi -e 's/sprintf\(newId, /snprintf(newId, sizeof(newId), /g' \
    libsbml-src/src/sbml/Model.cpp

# SBMLInferUnitsConverter.cpp and SBMLUnitsConverter.cpp:
# sprintf to format a counter into a small char buffer.
perl -pi -e 's/sprintf\(number, /snprintf(number, sizeof(number), /g' \
    libsbml-src/src/sbml/conversion/SBMLInferUnitsConverter.cpp

perl -pi -e 's/sprintf\(number, /snprintf(number, sizeof(number), /g' \
    libsbml-src/src/sbml/conversion/SBMLUnitsConverter.cpp

# End of CRAN patches #############################################################

dot() { file=$1; shift; . "$file"; }
dot ./scripts/r_config.sh ""

${RSCRIPT_BIN} --vanilla -e 'getRversion() > "4.0.0"' | grep TRUE > /dev/null
if [ $? -eq 0 ]; then
  CMAKE_ADD_AR="-D CMAKE_AR=${AR}"
  CMAKE_ADD_RANLIB="-D CMAKE_RANLIB=${RANLIB}"
else
  CMAKE_ADD_AR=""
  CMAKE_ADD_RANLIB=""
fi

# Detect compiler type and set appropriate warning suppression flags.
# Split into:
#   EXTRA_C_WARN_FLAGS   – flags valid for C only (not C++)
#   EXTRA_CXX_WARN_FLAGS – flags valid for C++ only
#   EXTRA_WARN_FLAGS     – flags valid for both C and C++
#
# -Wno-deprecated-non-prototype (clang): K&R-style prototypes; C-only concept.
# -Wno-old-style-definition (GCC): K&R function definitions; C-only.
# -Wno-format-truncation/-overflow/-stringop-overflow (GCC): C/C++ both fine,
#   but unknown on clang → GCC shared flags.
# -Wno-overloaded-virtual (clang/GCC): C++ only; libsbml's XMLErrorLog
#   overloads trigger this warning.
if echo "${CC}" | grep -qi clang || ${CC%% *} --version 2>&1 | grep -qi clang; then
    EXTRA_C_WARN_FLAGS="-Wno-tautological-constant-out-of-range-compare -Wno-tautological-undefined-compare -Wno-switch -Wno-deprecated-non-prototype"
    EXTRA_CXX_WARN_FLAGS="-Wno-tautological-constant-out-of-range-compare -Wno-tautological-undefined-compare -Wno-switch -Wno-overloaded-virtual"
    EXTRA_WARN_FLAGS=""
else
    EXTRA_C_WARN_FLAGS="-Wno-format-truncation -Wno-format-overflow -Wno-stringop-overflow -Wno-old-style-definition"
    EXTRA_CXX_WARN_FLAGS="-Wno-format-truncation -Wno-format-overflow -Wno-stringop-overflow -Wno-overloaded-virtual"
    EXTRA_WARN_FLAGS=""
fi

mkdir libsbml-build
cd libsbml-build
${CMAKE_BIN} \
    -D CMAKE_BUILD_TYPE=Release \
    -D BUILD_SHARED_LIBS=OFF \
    -D CMAKE_INSTALL_LIBDIR=lib \
    -D CMAKE_INSTALL_PREFIX=../libsbml-install \
    -D BUILD_TESTING=OFF \
    -D WITH_LIBXML=ON \
    -D WITH_ZLIB=ON \
    -D WITH_BZIP2=ON \
    -D ENABLE_SPATIAL=OFF \
    -D CMAKE_CXX_STANDARD=14 \
    -D CMAKE_CXX_STANDARD_REQUIRED=OFF \
    -D CMAKE_C_FLAGS="${CFLAGS} ${EXTRA_C_WARN_FLAGS} -std=gnu17" \
    -D CMAKE_CXX_FLAGS="${CXXFLAGS} ${EXTRA_CXX_WARN_FLAGS}" \
    ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../libsbml-src

make -j${NCORES}
make install
if [ $? -ne 0 ]; then
    echo "Make install failed!"
    exit 1
fi
cd ..

# Fix CRLF line endings and missing trailing newlines in installed headers (CRAN)
find libsbml-install/include -name "*.h" | while read f; do
    perl -pi -e 's/\r\n/\n/g; s/\r/\n/g' "$f"
    last=$(tail -c1 "$f")
    [ -n "$last" ] && printf '\n' >> "$f"
done

# libsbml-src and libsbml-build are removed by the EXIT trap above.
