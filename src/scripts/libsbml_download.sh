#! /bin/sh

RSCRIPT_BIN=$1
LIBSBML_VERSION=5.21.0

## The libsbml source is bundled with the package as libsbml-mod-<version>.tar.gz
## rather than fetched at build time, so installing needs no network access.
## That archive is an official libsbml release stripped of its language
## bindings, documentation, examples and test suites;
## tools/strip_libsbml_tarball.sh regenerates it.  Do not delete it after
## extraction: unlike a downloaded copy, it is part of the package sources.
echo "Extracting bundled libsbml-${LIBSBML_VERSION} source..."
${RSCRIPT_BIN} -e "utils::untar(tarfile = 'libsbml-mod-${LIBSBML_VERSION}.tar.gz')"
if [ $? -ne 0 ]; then
    echo "Could not extract the bundled libsbml tar file"
    exit 1
fi

mv libsbml-${LIBSBML_VERSION} libsbml-src
