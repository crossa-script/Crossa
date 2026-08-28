cmake_minimum_required(VERSION 3.22)

project(Crossa LANGUAGES CXX)

add_executable(
    crossa
    include/crossa/utils/Log.h
    include/crossa/utils/PrintUtils.h
    src/main.cpp
    src/utils/Log.cpp
    src/utils/PrintUtils.cpp
)

target_compile_features(crossa PRIVATE cxx_std_20)

target_include_directories(
    crossa
    PRIVATE
        include
)
