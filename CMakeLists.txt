cmake_minimum_required(VERSION 2.6)
project(Rabbits_Components)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Rabbits REQUIRED)
find_package(libfdt REQUIRED)

add_subdirectory(components)
add_subdirectory(plugins)
add_subdirectory(backends)

rabbits_add_dynlib(components)
target_link_libraries(components ${LIBFDT_LIBRARIES})
