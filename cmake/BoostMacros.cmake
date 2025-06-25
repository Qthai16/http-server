# copy from apache thrift

set(BOOST_MINREV 1.56)

macro(REQUIRE_BOOST_HEADERS)
  find_package(Boost ${BOOST_MINREV} QUIET REQUIRED)
  if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost ${BOOST_MINREV} or later is required to build sources in ${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  if (DEFINED Boost_INCLUDE_DIRS)
    # pre-boost 1.70.0 aware cmake, otherwise it is using targets
    include_directories(SYSTEM "${Boost_INCLUDE_DIRS}")
  endif()
endmacro()

macro(REQUIRE_BOOST_LIBRARIES libs)
  message(STATUS "Locating boost libraries required by sources in ${CMAKE_CURRENT_SOURCE_DIR}")
  find_package(Boost ${BOOST_MINREV} REQUIRED COMPONENTS ${${libs}})
  if (NOT Boost_FOUND)
    message(FATAL_ERROR "Boost ${BOOST_MINREV} or later is required to build sources in ${CMAKE_CURRENT_SOURCE_DIR}, or use -DBUILD_TESTING=OFF")
  endif()
endmacro()
