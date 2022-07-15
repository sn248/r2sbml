#! /bin/sh

: ${R_HOME=$(R RHOME)}
RSCRIPT_BIN=${R_HOME}/bin/Rscript
NCORES=`${RSCRIPT_BIN} -e "cat(min(2, parallel::detectCores(logical = FALSE)))"`

cd src

#### CMAKE CONFIGURATION ####
. ./scripts/cmake_config.sh

# Compile libsbml from source ####################################################
echo "######### Compiling libsbml from source #####################"
sh ./scripts/libsbml_download.sh ${RSCRIPT_BIN}
dot() { file=$1; shift; . "$file"; }
dot ./scripts/r_config.sh ""
${RSCRIPT_BIN} --vanilla -e 'getRversion() > "4.0.0"' | grep TRUE > /dev/null
if [ $? -eq 0 ]; then
  CMAKE_ADD_AR="-D CMAKE_AR=${AR}"
  CMAKE_ADD_RANLIB="-D CMAKE_RANLIB=${RANLIB}"
else
  CMAKE_ADD_AR=""
  CMAKE_ADD_RANLIB=""
fi
mkdir libsbml-build
mkdir libsbml
cd libsbml-build
${CMAKE_BIN} \
  -D CMAKE_INSTALL_PREFIX=../libsbml \
  -D CMAKE_BUILD_TYPE=Release \
  -D WITH_SWIG=OFF \
  ${CMAKE_ADD_AR} ${CMAKE_ADD_RANLIB} ../libsbml-src
make -j${NCORES}
make install
cd ..
mv libsbml/lib* libsbml/lib
##mv ceres/lib/* ../inst/

# Cleanup
sh ./scripts/libsbml_cleanup.sh
