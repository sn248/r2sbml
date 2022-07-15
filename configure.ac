## -*- mode: autoconf; autoconf-indentation: 4; -*-
##
##  r2sbml configure.ac
##
##  r2sbml -- R interface to libsbml SBML library
##
##  Copyright (C) 2022  Satyaprakash Nayak
##
## This file is licensed under the GPL-2 or later just like most of my
## Open Source code, and is granted an exemption (should it be needed)
## for inclusion into ceres

# require at least autoconf 2.69
AC_PREREQ([2.69])
AC_INIT([r2sbml],[0.1.0])

## Set R_HOME, respecting an environment variable if set
: ${R_HOME=$(R RHOME)}
if test -z "${R_HOME}"; then
   AC_MSG_ERROR([Could not determine R_HOME.])
fi

CXX=`"${R_HOME}/bin/R" CMD config CXX`
if test -z "${CXX}"; then
    AC_MSG_ERROR([No C++ compiler available])
fi
CXXFLAGS=`"${R_HOME}/bin/R" CMD config CXXFLAGS`
CPPFLAGS=`"${R_HOME}/bin/R" CMD config CPPFLAGS`
##CXX17FLAGS=`"${R_HOME}/bin/R" CMD config CXX17FLAGS`
AC_LANG(C++)
AC_REQUIRE_CPP
AC_PROG_CXX

## Default to build from source
need_to_build="yes"

if test x"${need_to_build}" != x"no"; then
    AC_PATH_PROG(have_cmake, cmake, no)
    if test x"${have_cmake}" == x"no"; then
       . src/scripts/cmake_config.sh
       if test -z "${CMAKE_BIN}"; then
          ## also error to end configure here
          AC_MSG_ERROR([Could not find 'cmake'.])
       fi
    fi
    ## 'uname -m' on M1 give x86_64 which is ... not helping
    machine=`"${R_HOME}/bin/Rscript" -e 'cat(Sys.info()[["machine"]])'`
    AC_MSG_RESULT([using ceres via local cmake build on ${machine} ])
    tools/cmake_call.sh
    ## cmake_call.sh installs into libsbml/lib, headers are copied
    libsbml_include="-I../inst/include"
    libsbml_libs="../inst/libsbml-static.a"

fi

## Now use all the values
AC_SUBST([SBML_INCLUDE],["$libsbml_include"])
AC_SUBST([SBML_LIBS],["$libsbml_libs"])
AC_CONFIG_FILES([src/Makevars])
AC_OUTPUT
