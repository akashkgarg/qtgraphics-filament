cmake_minimum_required (VERSION 2.8.12)
project (qtgraphics_filament)

# version number and definitions 
#-------------------------------------------------------------------------------

set (qtgraphics_filament_VERSION_MAJOR 0)
set (qtgraphics_filament_VERSION_MINOR 0)
set (qtgraphics_filament_VERSION_DEV 1)
set (qtgraphics_filament_VERSION
  "${qtgraphics_filament_VERSION_MAJOR}.${qtgraphics_filament_VERSION_MINOR}.${qtgraphics_filament_VERSION_DEV}")

# add definitions to use in project.
add_definitions (-DVERSION_MAJOR="${qtgraphics_filament_VERSION_MAJOR}")
add_definitions (-DVERSION_MINOR="${qtgraphics_filament_VERSION_MINOR}")
add_definitions (-DVERSION_DEV="${qtgraphics_filament_VERSION_DEV}")
add_definitions (-DVERSION="${qtgraphics_filament_VERSION}")

IF (WIN32)
  add_definitions(-D_USE_MATH_DEFINES)  # for openmesh
  add_definitions(-DNOMINMAX) # for openmesh
ENDIF(WIN32)

# Default values for CMAKE_BUILD_TYPE
#-------------------------------------------------------------------------------

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Debug CACHE STRING
    "Choose the type of build, options are: Debug, Release, RelWithDebInfo" FORCE)
endif (NOT CMAKE_BUILD_TYPE)

if ((NOT CMAKE_BUILD_TYPE MATCHES Debug) AND 
    (NOT CMAKE_BUILD_TYPE MATCHES Release) AND
    (NOT CMAKE_BUILD_TYPE MATCHES RelWithDebInfo))
  message (FATAL_ERROR "Invalid CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif ((NOT CMAKE_BUILD_TYPE MATCHES Debug) AND 
    (NOT CMAKE_BUILD_TYPE MATCHES Release) AND
    (NOT CMAKE_BUILD_TYPE MATCHES RelWithDebInfo))

# Compiler Flags 
#-------------------------------------------------------------------------------

option(USE_CLANG "build application with clang" ON) # ON is the default

if (USE_CLANG) 
  SET (CMAKE_C_COMPILER "clang")
  SET (CMAKE_CXX_COMPILER "clang++")
endif (USE_CLANG)

# use c++14, required by filament
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

IF (NOT WIN32)
  # Add common flags.
  set (CMAKE_CXX_FLAGS
      "-Wall -Wextra -fno-common -mfix-and-continue -stdlib=libc++"
      CACHE STRING "" FORCE)

  # set debug flags
  set (CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG" CACHE STRING "" FORCE)

  # set release flags
  set (CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG" CACHE STRING "" FORCE)
ENDIF (NOT WIN32)

# Additional options
#-------------------------------------------------------------------------------

# Extra modules for cmake
#-------------------------------------------------------------------------------

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

# Find Necessary Packages
#-------------------------------------------------------------------------------

find_package(Filament REQUIRED)

find_package (OpenGL REQUIRED)

find_package (Assimp REQUIRED)

find_package(Qt5 COMPONENTS Core Widgets Gui Concurrent OpenGL)

if (NOT Qt5_FOUND)
  message("Skipping filament_qt, could not find Qt5")
  return()
endif()

# Don't use concurrency when loading files.
add_definitions (-DQT_NO_CONCURRENT)

if (Qt5_FOUND)
  # Process the Qt ui files.
  # Qt5_WRAP_UI (UiSourcesOutput mainwindow.ui)

  # Process the Qt moc files automatically
  set (CMAKE_AUTOMOC TRUE)
  set (CMAKE_AUTOUIC TRUE)
  set (CMAKE_AUTORCC TRUE)

  # Process the QRC resources files.
  #QT5_ADD_RESOURCES (RccSourcesOutput ${PROJECT_SOURCE_DIR}/resources.qrc)
endif (Qt5_FOUND)

include_directories(${PROJECT_SOURCE_DIR}/src ${Filament_DIR})
include_directories(SYSTEM ${Assimp_INCLUDE_DIR})

# On macOS, we must use QtPlatformHeaders to grab the pointer to the main window's native OpenGL context (NSOpenGLContext)
# http://doc.qt.io/qt-5/qtplatformheaders-index.html
if( APPLE )
	# QtPlatformHeaders is a header-only module, leaving us with no choice but to get its location in this hacky way
	# FIXME: This is not necessary when using qmake, but is when using CMake?
	set( _qt5_platform_headers ${_qt5_root_dir}/../../include )
	get_filename_component( _qt5_platform_headers ${_qt5_platform_headers} ABSOLUTE )
	message( "Found QtPlatformHeaders: " ${_qt5_platform_headers} )
	include_directories(${_qt5_platform_headers} )

	# FIXME: For some reason this is necessary on macOS?
	# find_library( FOUNDATION Foundation REQUIRED )
	# target_link_libraries(qtgraphics_filament ${FOUNDATION} )
endif()

# Auto-generated files/assets
#------------------------------------------------------------------------------

set(GENERATION_ROOT "${CMAKE_BINARY_DIR}/generated")
set(RESOURCE_DIR    "${GENERATION_ROOT}/resources")
set(MATERIAL_DIR    "${GENERATION_ROOT}/material")
set(ENVMAP_DIR      "${GENERATION_ROOT}/envmaps")

file(MAKE_DIRECTORY ${GENERATION_ROOT})
file(MAKE_DIRECTORY ${MATERIAL_DIR})
file(MAKE_DIRECTORY ${ENVMAP_DIR})

set(RESGEN_SOURCE ${RESOURCE_DIR}/resources.c)

set (MatSources materials/bakedTextureLit.mat materials/transparent.mat)

# generate materials
foreach (mat_src ${MatSources})
  get_filename_component(localname "${mat_src}" NAME_WE)
  get_filename_component(fullname "${mat_src}" ABSOLUTE)
  set(output_path "${MATERIAL_DIR}/${localname}.filamat")
  add_custom_command(
    OUTPUT ${output_path}
    COMMAND ${Filament_MATC} -o ${output_path} ${fullname}
    MAIN_DEPENDENCY ${mat_src}
    DEPENDS ${Filament_MATC}
    COMMENT "Compiling material ${mat_src} to ${output_path}"
    )
  list(APPEND RESOURCE_BINS ${output_path})
endforeach()

# generate binary embeddable resources
add_custom_command(
  OUTPUT ${RESGEN_SOURCE}
  COMMAND ${Filament_RESGEN} -cx ${RESOURCE_DIR} -p resources ${RESOURCE_BINS}
  DEPENDS ${RESOURCE_BINS}
  COMMENT "Aggregating resources"
)

include_directories(${GENERATION_ROOT})

#------------------------------------------------------------------------------
# Compile target

add_executable(qtgraphics_filament "")
set_target_properties(qtgraphics_filament PROPERTIES OUTPUT_NAME qtgraphics_filament)
target_sources(qtgraphics_filament PRIVATE
        ${RESGEN_SOURCE}
        main.cpp
        CocoaGLContext.mm
        filament_renderer.cpp
        camera.cc
        )

target_link_libraries(qtgraphics_filament
  Qt5::Core
  Qt5::Gui
  Qt5::Widgets
  Qt5::Concurrent
  Qt5::OpenGL
  Filament
  ${Assimp_LIBRARY}
  ${IrrXml_LIBRARY})


# Subdirectories
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
