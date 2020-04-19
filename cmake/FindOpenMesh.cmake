# Find the OPENMESH Library

# OPENMESH_INCLUDE_DIR - base headers location
# OPENMESH_LIBRARY - base libraries
# OPENMESH_FOUND - true if found

# Mark the key variables as advanced so that they don't convolute the cmake
# gui. These variables are dervied from the OPENMESH_Inc_Dir and OpenMesh_Lib_Dir
# variables set by the user. 
MARK_AS_ADVANCED (OPENMESH_INCLUDE_DIR OPENMESH_LIBRARY)

# Find the headers
find_path (OPENMESH_INCLUDE_DIR
  OpenMesh/Core/Mesh/Handles.hh
  PATHS ${OpenMesh_Inc_Dir}
  )

# Find the libraries
find_library (OPENMESH_LIBRARY OpenMeshCore
  HINTS ${OPENMESH_INCLUDE_DIR}/../lib
  PATHS ${OpenMesh_Lib_Dir}
  )

# handle the QUIETLY and REQUIRED arguments and set OPENMESH_FOUND to TRUE if 
# all listed variables are TRUE
include (FindPackageHandleStandardArgs) 
find_package_handle_standard_args (
  OpenMesh
  "

  OpenMesh not found.
  Please adjust values for OpenMesh_Inc_Dir and OpenMesh_Lib_dir.
  OpenMesh_Inc_Dir - is the path to the location of the OpenMesh include
                    directory.
  OpenMesh_Lib_Dir - is the path to the location of the OpenMesh library. If this
                    is located in the path: \"OpenMesh_Inc_Dir/../lib\", then 
                    you don't need to set this variable.

  "
  OPENMESH_LIBRARY OPENMESH_INCLUDE_DIR
  )

# Set the OPENMESH_LIBRARIES variable once we have found the required OpenMesh
# libraries. 
if (OPENMESH_FOUND) 
  set (OPENMESH_LIBRARY ${OPENMESH_LIBRARY})
endif (OPENMESH_FOUND)
