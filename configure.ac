##
##  r2sbml configure.ac
##
##  r2sbml -- R interface to SBML
##
##  Copyright (C) 2020  Satyaprakash Nayak
##
##  r2sbml is free software: you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation, either version 2 of the License, or
##  (at your option) any later version.
##
##  r2sbml is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with r2sbml.  If not, see <http://www.gnu.org/licenses/>.

# require at least autoconf 2.69
AC_PREREQ([2.69])

# Process this file with autoconf to produce a configure script
AC_INIT([r2sbml],[0.1.0])

# Ensure C++ is set up as R expects
: ${R_HOME=`R RHOME`}
if test -z "${R_HOME}"; then
    AC_MSG_ERROR([Could not determine R_HOME.])
fi
CXX=`"${R_HOME}/bin/R" CMD config CXX`
if test -z "${CXX}"; then
    AC_MSG_ERROR([No C++ compiler available])
fi
CXXFLAGS=`"${R_HOME}/bin/R" CMD config CXXFLAGS`
CPPFLAGS=`"${R_HOME}/bin/R" CMD config CPPFLAGS`
CXX11FLAGS=`"${R_HOME}/bin/R" CMD config CXX11FLAGS`
AC_LANG(C++)
AC_REQUIRE_CPP
AC_PROG_CXX

## Detect libxml2 include path via pkg-config (needed to compile against
## libsbml headers, which include libxml headers from the install tree)
AC_DEFUN([AC_PROG_PKGCONFIG], [AC_CHECK_PROG(PKGCONFIG,pkg-config,yes)])
AC_PROG_PKGCONFIG

LIBXML2_CPPFLAGS=""
if test x"${PKGCONFIG}" = x"yes"; then
    if pkg-config --exists libxml2; then
        LIBXML2_CPPFLAGS=`pkg-config --cflags libxml2`
    fi
fi
if test -z "${LIBXML2_CPPFLAGS}"; then
    if test -d "/usr/include/libxml2"; then
        LIBXML2_CPPFLAGS="-I/usr/include/libxml2"
    fi
fi

## Always build libsbml from source using cmake
need_to_build="yes"
if test x"${need_to_build}" != x"no"; then
    AC_PATH_PROG(have_cmake, cmake, no)
    # CRAN fix: use POSIX-compliant single = in test; == is a bashism
    if test x"${have_cmake}" = x"no"; then
        . src/scripts/cmake_config.sh
    fi
    tools/cmake_call.sh
    SBML_INCLUDE="-I\$(srcdir)/libsbml-install/include/ ${LIBXML2_CPPFLAGS}"
    SBML_LIBS="-L\$(srcdir)/libsbml-install/lib/ -lsbml-static -lxml2 -lbz2"
fi

# Now substitute these variables in src/Makevars.in to create src/Makevars
AC_SUBST([SBML_INCLUDE],["$SBML_INCLUDE"])
AC_SUBST([SBML_LIBS],["$SBML_LIBS"])

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
echo
echo "Final src/Makevars"
cat src/Makevars
