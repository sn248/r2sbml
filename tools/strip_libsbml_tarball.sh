#! /bin/sh
# Strip an official libSBML release tarball down to the subset needed to build
# the static library for r2sbml, and repack it as libsbml-mod-<version>.tar.gz
# (placed in the current directory).
#
# This removes the language bindings, documentation, examples, development
# tooling, autotools build system, and the test suites, to keep the R package
# tarball small (~2 MB vs ~29 MB upstream).  What is kept:
#
#   CMakeLists.txt  CMakeModules/  common.cmake  *package.cmake
#   cmake_uninstall.cmake.in  VERSION.txt  LICENSE.txt  COPYING.txt
#   NEWS.txt  README.md
#   src/CMakeLists.txt  src/*.cmake  src/*.cmake.in
#   src/sbml/  (minus every test/ directory, minus the SBML Level 3 package
#               source trees that this build does not enable)
#
# The per-package "*-register.h" and "*-register.cxx" stubs under
# src/sbml/packages/ are kept even for removed packages: src/CMakeLists.txt
# picks them up with file(GLOB) and #includes them into a generated header.
# Their bodies are guarded by "#ifdef USE_<PKG>", which is never defined for a
# package that is not enabled, so the removed sources are never referenced.
#
# Only l3v2extendedmath is kept, because it is the one SBML Level 3 package
# libSBML compiles into the core library by default; the build in
# tools/cmake_call.sh enables no others.  If that ever changes, add the package
# to KEEP_PACKAGES below and regenerate the tarball.
#
# Usage:
#   tools/strip_libsbml_tarball.sh <version> [path-to-libsbml-<version>.tar.gz]
#
# If the tarball path is omitted, the official release is downloaded from
# GitHub.  Example:
#   tools/strip_libsbml_tarball.sh 5.21.0
#   mv libsbml-mod-5.21.0.tar.gz src/

set -e

VERSION=$1
TARBALL=$2

if [ -z "${VERSION}" ]; then
    echo "Usage: $0 <version> [path-to-libsbml-<version>.tar.gz]" >&2
    exit 1
fi

OUTDIR=$(pwd)
WORKDIR=$(mktemp -d)
trap 'rm -rf "${WORKDIR}"' EXIT

if [ -z "${TARBALL}" ]; then
    TARBALL="${WORKDIR}/libsbml-${VERSION}.tar.gz"
    echo "Downloading libsbml-${VERSION}.tar.gz from GitHub ..."
    curl -sL -o "${TARBALL}" \
        "https://github.com/sbmlteam/libsbml/archive/refs/tags/v${VERSION}.tar.gz"
fi

echo "Extracting ${TARBALL} ..."
tar -xzf "${TARBALL}" -C "${WORKDIR}"

SRCDIR="${WORKDIR}/libsbml-${VERSION}"
if [ ! -d "${SRCDIR}" ]; then
    echo "Expected top-level directory libsbml-${VERSION} not found in tarball" >&2
    exit 1
fi

# --- prune the top level -------------------------------------------------
# COPYING.txt, FUNDING.txt, LICENSE.txt, NEWS.txt, README.md and VERSION.txt
# are libsbml's DOCUMENTATION_FILES: its own install() step copies all six into
# share/libsbml, and cmake --install fails outright if any is missing.
KEEP="CMakeLists.txt CMakeModules common.cmake cmake_uninstall.cmake.in \
VERSION.txt LICENSE.txt COPYING.txt FUNDING.txt NEWS.txt README.md src"

for entry in "${SRCDIR}"/* "${SRCDIR}"/.[!.]*; do
    [ -e "${entry}" ] || continue
    name=$(basename "${entry}")
    # keep every *package.cmake: the root CMakeLists.txt file(GLOB)s them in
    case "${name}" in *package.cmake) continue ;; esac
    keep_it=no
    for k in ${KEEP}; do
        if [ "${name}" = "${k}" ]; then keep_it=yes; break; fi
    done
    if [ "${keep_it}" = "no" ]; then
        echo "  removing ${name}"
        rm -rf "${entry}"
    fi
done

# --- prune src/ ----------------------------------------------------------
# src/CMakeLists.txt calls add_subdirectory(bindings) unconditionally, so the
# directory has to survive, but everything it contains is reachable only
# through a WITH_<LANGUAGE> guard and every one of those is OFF here.  Keep
# the CMakeLists.txt and drop the ten language trees and the swig interface
# files; the file(GLOB) calls over swig/ simply come back empty.
echo "  removing src/bindings (keeping only its CMakeLists.txt)"
find "${SRCDIR}/src/bindings" -mindepth 1 -maxdepth 1 \
    ! -name CMakeLists.txt -exec rm -rf {} +
echo "  removing src/Makefile.in"
rm -f "${SRCDIR}/src/Makefile.in"

# --- prune the SBML Level 3 package sources ------------------------------
KEEP_PACKAGES="l3v2extendedmath"

for pkg in "${SRCDIR}"/src/sbml/packages/*; do
    [ -d "${pkg}" ] || continue
    name=$(basename "${pkg}")
    keep_it=no
    for k in ${KEEP_PACKAGES}; do
        if [ "${name}" = "${k}" ]; then keep_it=yes; break; fi
    done
    if [ "${keep_it}" = "no" ]; then
        echo "  removing src/sbml/packages/${name}"
        rm -rf "${pkg}"
    fi
done

# --- prune every test directory ------------------------------------------
find "${SRCDIR}/src" -type d -name test -prune -print \
    | sed "s|${SRCDIR}/|  removing |"
find "${SRCDIR}/src" -type d -name test -prune -exec rm -rf {} +

echo "Repacking libsbml-mod-${VERSION}.tar.gz ..."
tar -czf "${OUTDIR}/libsbml-mod-${VERSION}.tar.gz" -C "${WORKDIR}" "libsbml-${VERSION}"

SIZE=$(du -h "${OUTDIR}/libsbml-mod-${VERSION}.tar.gz" | cut -f1)
COUNT=$(tar -tzf "${OUTDIR}/libsbml-mod-${VERSION}.tar.gz" | wc -l)
echo "Done: libsbml-mod-${VERSION}.tar.gz (${SIZE}, ${COUNT} entries)"
