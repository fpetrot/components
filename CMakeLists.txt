cmake_minimum_required(VERSION 2.6)
project(Rabbits_Components)

find_package(Rabbits REQUIRED)

add_subdirectory(components)
add_subdirectory(plugins)

rabbits_add_dynlib(components)
