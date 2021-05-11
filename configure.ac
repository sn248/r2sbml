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
##  along with RcppRedis.  If not, see <http://www.gnu.org/licenses/>.

##  using the configure.ac file for RcppRedis at template
# require at least autoconf 2.61
AC_PREREQ([2.71])

# Process this file with autoconf to produce a configure script
AC_INIT([r2sbml],[0.1.0])

# Ensure C++ is set up as R expects
: ${R_HOME=`R RHOME`}
if test -z "${R_HOME}"; then
    AC_MSG_ERROR([Could not determine R_HOME.])
fi
CXX=`"${R_HOME}/bin/R" CMD config CXX`
CXXFLAGS=`"${R_HOME}/bin/R" CMD config CXXFLAGS`
AC_LANG(C++)
AC_REQUIRE_CPP
AC_PROG_CXX

## check for pkg-config
AC_DEFUN([AC_PROG_PKGCONFIG], [AC_CHECK_PROG(PKGCONFIG,pkg-config,yes)])
AC_PROG_PKGCONFIG

## use pkg-config for libsbml since it ships with libsbml.pc
##
if test x"${PKGCONFIG}" == x"yes"; then
    ## check via pkg-config for libsbml
    if pkg-config --exists libsbml; then
        ## obtain cflags and obtain libs
        SBML_INCLUDE=`pkg-config --cflags libsbml`
        ## should give -I/usr/local/include

        ## sbml_libs=`pkg-config --libs libsbml` gives -L/usr/local/lib -lsbml
        ## but I need -L/usr/local/lib -lsbml-static so manually input this
        SBML_LIBS="-L/usr/local/lib -lsbml-static -lxml2"
        ## no need to build libsbml-static.a
        libsbml_tgt=""
    else
        AC_MSG_WARN([pkg-config exists but libsbml is not available.])
    fi
fi

# Now substitute these variables in src/Makevars.in to create src/Makevars
AC_SUBST(SBML_INCLUDE)
AC_SUBST(SBML_LIBS)

AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
echo
echo "Final src/Makevars"
cat src/Makevars
