cmake_minimum_required(VERSION 3.22)

project(Crossa LANGUAGES CXX)

add_executable(
    crossa
    include/crossa/utils/Log.h
    include/crossa/utils/PrintUtils.h
    include/crossa/compiler/source/SourceFile.h
    include/crossa/compiler/source/SourceLoader.h
    include/crossa/compiler/ir/Program.h
    include/crossa/runtime/ExecutionEngine.h
    src/main.cpp
    src/compiler/source/SourceFile.cpp
    src/compiler/source/SourceLoader.cpp
    src/compiler/ir/Program.cpp
    src/runtime/ExecutionEngine.cpp
    src/utils/Log.cpp
    src/utils/PrintUtils.cpp
)

target_compile_features(crossa PRIVATE cxx_std_20)

target_include_directories(
    crossa
    PRIVATE
        include
)
