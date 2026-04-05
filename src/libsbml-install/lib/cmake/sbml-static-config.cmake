get_filename_component(LIBSBML_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)
include(CMakeFindDependencyMacro)

list(APPEND CMAKE_MODULE_PATH "/home/quantilogy/Documents/GitHub/sn248/r2sbml/src/libsbml-install/share/cmake/Modules")

foreach (library BZ2::BZ2;ZLIB::ZLIB;LIBXML::LIBXML )

    string(FIND "${library}" "::" index)

    if (${index} GREATER 0)
      # found dependent library
      string(SUBSTRING "${library}" 0 ${index} DEPENDENT_NAME)
      message(VERBOSE "Looking for dependent library: ${DEPENDENT_NAME}")
      find_dependency(${DEPENDENT_NAME})
    endif()

endforeach()

if(NOT TARGET sbml-static)
    include("${LIBSBML_CMAKE_DIR}/sbml-static-targets.cmake")
endif()

set(LIBSBML_LIBRARIES sbml-static)
