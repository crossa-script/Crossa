cmake_minimum_required(VERSION 3.22)

project(Crossa LANGUAGES CXX)

add_executable(
    crossa
    include/crossa/utils/Log.h
    include/crossa/utils/PrintUtils.h
    include/crossa/cli/CrossaApplication.h
    include/crossa/compiler/source/SourceFile.h
    include/crossa/compiler/source/SourceLoader.h
    include/crossa/compiler/lexer/TokenType.h
    include/crossa/compiler/lexer/Token.h
    include/crossa/compiler/lexer/Lexer.h
    include/crossa/compiler/ast/TypeReference.h
    include/crossa/compiler/ast/Expression.h
    include/crossa/compiler/ast/Statement.h
    include/crossa/compiler/ast/Declaration.h
    include/crossa/compiler/ast/SourceUnit.h
    include/crossa/compiler/ast/AstPrinter.h
    include/crossa/compiler/parser/Parser.h
    include/crossa/compiler/ir/Program.h
    include/crossa/runtime/ExecutionEngine.h
    src/main.cpp
    src/cli/CrossaApplication.cpp
    src/compiler/source/SourceFile.cpp
    src/compiler/source/SourceLoader.cpp
    src/compiler/lexer/TokenType.cpp
    src/compiler/lexer/Token.cpp
    src/compiler/lexer/Lexer.cpp
    src/compiler/ast/TypeReference.cpp
    src/compiler/ast/Expression.cpp
    src/compiler/ast/Statement.cpp
    src/compiler/ast/Declaration.cpp
    src/compiler/ast/SourceUnit.cpp
    src/compiler/ast/AstPrinter.cpp
    src/compiler/parser/Parser.cpp
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
