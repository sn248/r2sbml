
## Process this file with autoconf to produce a configure script.
##
## Configure.ac for RcppSBML
##
## Copyright (C) 2010         Romain Francois and Dirk Eddelbuettel
## Copyright (C) 2014 - 2015  Dirk Eddelbuettel
##
## Licensed under GNU GPL 2 or later

# The version set here will propagate to other files from here
AC_INIT([Rcppsbml], 0.1.0)

# Checks for common programs using default macros
AC_PROG_CC

## Use gsl-config to find arguments for compiler and linker flags
##
## Check for non-standard programs: gsl-config(1)
AC_PATH_PROG([LIB_SBML], [libsbml])
## If gsl-config was found, let's use it
if test "${LIB_SBML}" != ""; then
    # Use gsl-config for header and linker arguments
    SBML_CFLAGS=`${LIB_SBML} --cflags`
    SBML_LIBS=`${LIB_SBML} --libs`
else
    AC_MSG_ERROR([libsbml not found, is SBML installed?])
fi

# Now substitute these variables in src/Makevars.in to create src/Makevars
AC_SUBST(LIB_SBML)
AC_SUBST(LIB_SBML)

AC_OUTPUT(src/Makevars)

