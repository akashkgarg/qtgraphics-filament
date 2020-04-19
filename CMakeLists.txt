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

#------------------------------------------------------------------------------
# Compile target

add_executable(qtgraphics_filament "")
set_target_properties(qtgraphics_filament PROPERTIES OUTPUT_NAME qtgraphics_filament)
target_sources(qtgraphics_filament PRIVATE
        main.cpp
        CocoaGLContext.mm
        )

target_link_libraries(qtgraphics_filament
  Qt5::Core
  Qt5::Gui
  Qt5::Widgets
  Qt5::Concurrent
  Qt5::OpenGL
  Filament)


# Subdirectories
#-------------------------------------------------------------------------------

#-------------------------------------------------------------------------------
