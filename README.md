# r2sbml
R interface to the SBML library

This is a work in progress. The aim of this project is to provide an interface to read in SBML models in `R`. Eventually, I want to be able to read in an SBML file and convert it into a model which can be simulated in R/MATLAB/Julia. 
Currently the package only contains example `R` scripts from `libsbml` library.

## Installation
Installation of this package from source requires that libsbml is installed on your system (for Unix and macOS). On Windows, the library is downloaded from its rwinlib location and installed. There are no additional steps on Windows.

###  Installation of libsbml on macOS from source
Installation of libsbml from source is easy. Download the source files from https://github.com/sbmlteam/libsbml/archive/refs/tags/v5.19.5.tar.gz

Extract and install the library using the following commands in the terminal
```
tar -xzf libsbml-5.19.5.tar.gz 
cd libSBML-5.19.0-Source
./configure
make
sudo make install
```
This should install the library at default location `/usr/local/lib` and the header files at `usr/local/include`

Check that the library is present at its location using the following command at the terminal. On my system it shows the following output
```
libsbml-static.a
libsbml.5.18.0.dylib
libsbml.5.19.0.dylib
libsbml.5.dylib
libsbml.a
libsbml.dylib
libsbml.la
libsbml2matlab.dylib
libsbmlj.jnilib
```

The presence of `libsbml-static.a` and `libsbml.a` should indicate successful installation of the libsbml library. If you have `pkg-config` installed, you can also check the installation of the library using the following command
```
pkg-config --libs libsbml
```
which should give the following output
```
-L/usr/local/lib -lsbml
```
You can also install libSBML using `macports` on macOS. See more instructions [here](https://ports.macports.org/port/libsbml/).

### Installation of libsbml on Unix systems
Installation from source can done on most Unix systems following exactly the same steps as note above for macOS. In addition, distribution packages are available for the different linux OS. For Ubuntu, the following commands can be used in the terminal
```
sudo apt-get update
sudo apt-get install libsbml5 libsbml5cli libsbml5-dev
```
Installation of the library can be checked using the `pkg-config` library as discussed above.
