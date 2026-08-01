#### R CONFIGURATION ####

R_ARCH_BIN=$1

CC=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CC`
echo set CC=$CC
export CC

CPPFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CPPFLAGS`
CFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CFLAGS`
CPICFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CPICFLAGS`

CFLAGS="$CPPFLAGS $CPICFLAGS $CFLAGS"
echo set CFLAGS=$CFLAGS
export CFLAGS

## R 4.3 deprecated the CXX11* config variables and R 4.6 made them defunct:
## "R CMD config CXX11FLAGS" now prints an error to stderr and returns nothing,
## which silently left CXX and CXXFLAGS empty and made cmake fall back to its
## own compiler defaults.  Use the plain CXX* names and only fall back to the
## CXX11* ones for R < 4.3.
CXX_FULL=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CXX 2>/dev/null`
if test -z "$CXX_FULL"; then
    CXX_FULL=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CXX11 2>/dev/null`
fi

## "R CMD config CXX" returns the compiler together with its standard flag
## (e.g. "g++ -std=gnu++20"); cmake wants the binary on its own, so split the
## first word off and carry the remainder in CXXFLAGS.
CXX=`echo "$CXX_FULL" | cut -d' ' -f1`
CXXSTD=`echo "$CXX_FULL" | cut -s -d' ' -f2-`
echo set CXX=$CXX
export CXX

CXXFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CXXFLAGS 2>/dev/null`
CXXPICFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CXXPICFLAGS 2>/dev/null`
if test -z "$CXXFLAGS"; then
    CXXFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CXX11FLAGS 2>/dev/null`
    CXXPICFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CXX11PICFLAGS 2>/dev/null`
fi

CXXFLAGS="$CXXSTD $CPPFLAGS $CXXPICFLAGS $CXXFLAGS"
echo set CXXFLAGS=$CXXFLAGS
export CXXFLAGS

LDFLAGS=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config LDFLAGS`
echo set LDFLAGS=$LDFLAGS
export LDFLAGS

if test -z "$CXX"; then
    echo >&2 "Could not detect C++ compiler with R CMD config."
fi

${R_HOME}/bin${R_ARCH_BIN}/Rscript --vanilla -e 'getRversion() > "4.0.0"' | grep TRUE > /dev/null
if [ $? -eq 0 ]; then
	AR=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config AR`
	AR=`which $AR`

	RANLIB=`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config RANLIB`
	RANLIB=`which $RANLIB`
fi
