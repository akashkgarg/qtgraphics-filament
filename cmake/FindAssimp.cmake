if(TARGET Assimp)
    return()
endif()

set (Assimp_DIR ${Assimp_DIR} CACHE PATH "set the root where assimp is")

# Find the headers
find_path (Assimp_INCLUDE_DIR
  assimp/scene.h
  PATHS "${Assimp_DIR}/include"
  )

find_library(Assimp_LIBRARY assimp
  HINTS "${Assimp_DIR}/lib/" 
  PATHS "${Assimp_DIR}/lib/")

find_library(IrrXml_LIBRARY IrrXML
  HINTS "${Assimp_DIR}/lib/"
  PATHS "${Assimp_DIR}/lib/")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Assimp
  REQUIRED_VARS Assimp_INCLUDE_DIR Assimp_LIBRARY IrrXml_LIBRARY
        )

add_library(Assimp INTERFACE)
target_link_libraries(Assimp INTERFACE ${Assimp_LIBRARY} ${IrrXml_LIBRARY})
target_include_directories(Assimp INTERFACE ${Assimp_INCLUDE_DIRS})
