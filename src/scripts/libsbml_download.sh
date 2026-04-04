#! /bin/sh

RSCRIPT_BIN=$1
LIBSBML_VERSION=5.21.0

## Download libsbml source from GitHub
echo "Downloading libsbml-${LIBSBML_VERSION} from GitHub..."
${RSCRIPT_BIN} -e "download.file(url = 'https://github.com/sbmlteam/libsbml/archive/refs/tags/v${LIBSBML_VERSION}.tar.gz', destfile = 'libsbml-${LIBSBML_VERSION}.tar.gz', method = 'libcurl')"
if [ $? -ne 0 ]; then
    echo "Downloading libsbml from GitHub failed!"
    exit 1
fi

${RSCRIPT_BIN} -e "utils::untar(tarfile = 'libsbml-${LIBSBML_VERSION}.tar.gz')"
if [ $? -ne 0 ]; then
    echo "Could not extract the libsbml tar file"
    exit 1
fi

mv libsbml-${LIBSBML_VERSION} libsbml-src
rm -f libsbml-${LIBSBML_VERSION}.tar.gz
