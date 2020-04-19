if(TARGET Filament)
    return()
endif()

set (Filament_DIR ${Filament_DIR} CACHE PATH "set the root where filament release is")

message("Filament_DIR: ${Filament_DIR}/")

set(Filament_INCLUDE_DIRS "${Filament_DIR}/include")

if (APPLE)
  set(Filament_LIB_DIR "${Filament_DIR}/lib/x86_64")
endif()
if (WIN32)
  set(Filament_LIB_DIR "${Filament_DIR}/lib/x86_64/mt")
endif()

message("FOUND LIB DIR: ${Filament_LIB_DIR}/")

find_library(Filament_LIBRARY NAMES filament PATHS "${Filament_LIB_DIR}/" ${Filament_LIB_DIR})
find_library(FilamentBackend_LIBRARY NAMES backend PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentBlueGL_LIBRARY NAMES bluegl PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentBridge_LIBRARY NAMES filabridge PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentFlat_LIBRARY NAMES filaflat PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentGeometry_LIBRARY NAMES geometry PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentIBL_LIBRARY NAMES ibl PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentMat_LIBRARY NAMES filamat PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentShaders_LIBRARY NAMES shaders PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentSmol_LIBRARY NAMES smol-v PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
find_library(FilamentUtils_LIBRARY NAMES utils PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
set(Filament_LIBRARIES
  ${Filament_LIBRARY}
  ${FilamentBackend_LIBRARY}
  ${FilamentBlueGL_LIBRARY}
  ${FilamentBridge_LIBRARY}
  ${FilamentFlat_LIBRARY}
  ${FilamentGeometry_LIBRARY}
  ${FilamentIBL_LIBRARY}
  ${FilamentMat_LIBRARY}
  ${FilamentShaders_LIBRARY}
  ${FilamentSmol_LIBRARY}
  ${FilamentUtils_LIBRARY}
  )
find_library(FilamentMoltenVK_LIBRARY NAMES MoltenVK PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
if(EXISTS ${FilamentMoltenVK_LIBRARY})
  list(APPEND Filament_LIBRARIES ${FilamentMoltenVK_LIBRARY})
endif()
find_library(FilamentVulkan_LIBRARY NAMES vulkan.1 PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
if(EXISTS ${FilamentVulkan_LIBRARY})
  list(APPEND Filament_LIBRARIES ${FilamentVulkan_LIBRARY})
endif()
find_library(FilamentBlueVK_LIBRARY NAMES bluevk PATHS "${Filament_LIB_DIR}/" NO_DEFAULT_PATH)
if(EXISTS ${FilamentBlueVK_LIBRARY})
  list(APPEND Filament_LIBRARIES ${FilamentBlueVK_LIBRARY})
endif()

find_file(Filament_RESGEN resgen
  HINTS ${Filament_DIR}/bin
  HINTS ${Filament_DIR}/bin)

find_file(Filament_MATC matc
  HINTS ${Filament_DIR}/bin
  HINTS ${Filament_DIR}/bin)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Filament
  REQUIRED_VARS Filament_INCLUDE_DIRS Filament_LIBRARY Filament_RESGEN Filament_MATC)

add_library(Filament INTERFACE)
target_link_libraries(Filament INTERFACE ${Filament_LIBRARIES})
target_include_directories(Filament SYSTEM INTERFACE ${Filament_INCLUDE_DIRS})
if(APPLE)
    target_link_libraries(Filament INTERFACE
            "-framework Foundation"
            "-framework Cocoa"
            "-framework IOKit"
            "-framework Metal"
            "-framework QuartzCore"
            "-framework CoreVideo"
            "-framework OpenGL"
            )
endif()
