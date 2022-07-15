#! /bin/sh

RSCRIPT_BIN=$1

# Uncompress gflags source
${RSCRIPT_BIN} -e "utils::untar(tarfile = 'libSBML-5.19.0-core-plus-packages-src.tar.gz')"
mv libSBML-5.19.0-Source libsbml-src
