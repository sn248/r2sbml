# Build against mingw-w64 build of libsbml
VERSION <- commandArgs(TRUE)
if(!file.exists(sprintf("../windows/libsbml-%s/include/sbml/SBMLTypes.h", VERSION))){
  if(getRversion() < "3.3.0") setInternet2()
  download.file(sprintf("https://github.com/rwinlib/libsbml/archive/v%s.zip", VERSION),
                "lib.zip", quiet = TRUE)
  dir.create("../windows", showWarnings = FALSE)
  unzip("lib.zip", exdir = "../windows")
  unlink("lib.zip")
}
