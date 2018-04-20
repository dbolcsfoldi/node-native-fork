cmake_minimum_required(VERSION 3.0)

#include(ExternalProject)
#find_package(Git REQUIRED)
#
#ExternalProject_Add(
#    doctest
#    PREFIX ${CMAKE_BINARY_DIR}/doctest
#    GIT_REPOSITORY https://github.com/onqtam/doctest.git
#    TIMEOUT 10
#    UPDATE_COMMAND ${GIT_EXECUTABLE} pull
#    CONFIGURE_COMMAND ""
#    BUILD_COMMAND ""
#    INSTALL_COMMAND ""
#    LOG_DOWNLOAD ON
#)

# Expose required variable (DOCTEST_INCLUDE_DIR) to parent scope
#ExternalProject_Get_Property(doctest source_dir)
#set(DOCTEST_INCLUDE_DIR ${source_dir}/doctest CACHE INTERNAL "Path to include folder for doctest")

# Test target
set (CMAKE_CXX_STANDARD 11)
link_directories(/usr/local/lib)
add_executable(hipc test/hipc-test.cc)
add_test(NAME jest COMMAND npm test)
target_include_directories(hipc PUBLIC inc /usr/local/include)
target_link_libraries(hipc uv json-c)
