#### R CONFIGURATION ####

R_ARCH_BIN=$1

## A compiler launcher -- ccache, sccache, distcc, icecc -- may lead
## "R CMD config CC"/"CXX", typically put there by a ~/.R/Makevars.  cmake
## takes the first word as the compiler, so a launcher left in place becomes
## the compiler and the real one is pushed into the flags, where it fails as
## "ccache: invalid option -- 'D'".  peel_launcher splits the launcher(s) off
## into LAUNCHER, as a cmake list, and leaves the rest in PEELED.
peel_launcher() {
    LAUNCHER=""
    PEELED="$1"
    while :; do
        _first=`echo "$PEELED" | cut -d' ' -f1`
        case `basename "$_first" 2>/dev/null` in
            ccache|sccache|distcc|icecc|buildcache)
                LAUNCHER="${LAUNCHER:+${LAUNCHER};}${_first}"
                PEELED=`echo "$PEELED" | cut -s -d' ' -f2-` ;;
            *) break ;;
        esac
    done
}

peel_launcher "`"${R_HOME}/bin${R_ARCH_BIN}/R" CMD config CC`"
CC=$PEELED
CC_LAUNCHER=$LAUNCHER
echo set CC=$CC
echo set CC_LAUNCHER=$CC_LAUNCHER
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
peel_launcher "$CXX_FULL"
CXX_FULL=$PEELED
CXX_LAUNCHER=$LAUNCHER
echo set CXX_LAUNCHER=$CXX_LAUNCHER

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
