#!/bin/sh

cp libsbml/lib/*.a ../inst/

cp -R libsbml/include/* ../inst/include/

rm -fr libsbml-src libsbml-build libsbml
