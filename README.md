# r2sbml
R interface to the SBML library

This is a work in progress. The aim of this project is to provide an interface to read in SBML models in `R`. Eventually, I want to be able to read in an SBML file and convert it into a model which can be simulated in R/MATLAB/Julia. 
Currently the package only contains example `R` scripts from `libsbml` library.


## Installation

The following is relevant only when building the package from source files on macOS or Linux. On windows, the package installation should work fine without needing any additional libraries (thanks to rwinlib!).

### Dependencies - libxml2
Since `libSBML` depends on `libXML2`, installation of this package also requires that you have `libXML2` installed on your machine (macOS and Linux). 

Installation of `libXML2` can be checked on macOS by entering the following command on the terminal
```
xml2-config --cflags
```
which gives
```
-I/Library/Developer/CommandLineTools/SDKs/MacOSX10.15.sdk/usr/include
```
on my machine.

Also, the following command
```
xml2-config --libs
```
gives the following result
```
-L/Library/Developer/CommandLineTools/SDKs/MacOSX10.15.sdk/usr/lib -lxml2 -lz -lpthread -licucore -lm
```

On Ubuntu, you can use `pkg-config` to check the availability of the package, i.e., entering the following command
```
pkg-config --cflags libxml-2.0
```
should tell you where the header files for `libxml2` are. On my Ubuntu system, it gives me
```
-I/usr/include/libxml2
```
and similarly, the library can be found by
```
pkg-config  --libs libxml-2.0
```
which on my Ubuntu system gives
```
-lxml2
```

### Dependencies - libbzip2 and libzlib
`libbzip2` and `libzlib` are required to read `SBML` files in compressed `.bz2` or `.zip` formats respectively. They are usually installed on Unix systems. The package will install even if these dependencies are not found, but with no support for reading `SBML` in `.zip`, `.gz` or `.bz2` compressed formats.

### Building package from source
Assuming that `libXML2` is installed on your machine. This package uses the `cmake` based build system for installation of the package and has been tested on macOS (10.13) and Ubuntu (20.04)
