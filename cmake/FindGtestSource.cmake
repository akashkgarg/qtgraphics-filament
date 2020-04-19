# - Try to find the gtest sources
# Once done this will define
#
#  GTEST_FOUND - system has GTEST
#  GTEST_INCLUDE_DIR - the GTEST include directory
#  GTEST_SOURCES - the GTEST source files

set (GTEST_ROOT_DIR ${GTEST_ROOT_DIR} CACHE PATH "set the root where gtest src location")

if(GTEST_INCLUDE_DIR AND GTEST_SOURCES)
   set(GTEST_FOUND TRUE)
else(GTEST_INCLUDE_DIR AND GTEST_SOURCES)

FIND_PATH(GTEST_INCLUDE_PATH gtest/gtest.h
   /usr/include
   /usr/local/include
   ${PROJECT_SOURCE_DIR}/gtest-1.7.0/include/
   ${PROJECT_SOURCE_DIR}/gtest/
   ${PROJECT_SOURCE_DIR}/gtest/include/
   ${GTEST_ROOT_DIR}/include/
)

if(GTEST_INCLUDE_PATH)
   set(GTEST_FOUND TRUE)
   set(GTEST_INCLUDE_DIR ${GTEST_INCLUDE_PATH})
   set(GTEST_ROOT_DIR ${GTEST_INCLUDE_PATH}/../)
else(GTEST_INCLUDE_PATH) 
  message("Value of GTEST_INCLUDE_PATH ${GTEST_INCLUDE_PATH}")
endif(GTEST_INCLUDE_PATH)

mark_as_advanced(GTEST_INCLUDE_PATH)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(GtestSource DEFAULT_MSG GTEST_INCLUDE_DIR)

endif(GTEST_INCLUDE_DIR AND GTEST_SOURCES)
