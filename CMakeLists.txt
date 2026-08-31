cmake_minimum_required(VERSION 3.22)

project(Crossa LANGUAGES CXX)

option(
    CROSSA_ENABLE_NETWORK_INTEGRATION
    "Enable integration tests that call JSONPlaceholder"
    OFF
)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

add_executable(
    crossa
    include/crossa/utils/Log.h
    include/crossa/utils/PrintUtils.h
    include/crossa/cli/CrossaApplication.h
    include/crossa/cli/CrossaVersion.h
    include/crossa/cli/doctor/DoctorChecks.h
    include/crossa/cli/doctor/DoctorCommand.h
    include/crossa/cli/doctor/DoctorResult.h
    include/crossa/cli/doctor/DoctorRunner.h
    include/crossa/cli/doctor/DoctorStatus.h
    include/crossa/cli/doctor/ProcessRunner.h
    include/crossa/compiler/source/SourceFile.h
    include/crossa/compiler/source/SourceLocation.h
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
    include/crossa/compiler/project/ProjectLinker.h
    include/crossa/compiler/generators/kotlin/KotlinGeneratedSource.h
    include/crossa/compiler/generators/kotlin/KotlinGenerator.h
    include/crossa/compiler/generators/kotlin/KotlinIdentifierEscaper.h
    include/crossa/compiler/generators/kotlin/KotlinExpressionEmitter.h
    include/crossa/compiler/generators/kotlin/KotlinSourceWriter.h
    include/crossa/compiler/generators/kotlin/KotlinStatementEmitter.h
    include/crossa/compiler/generators/kotlin/KotlinTypeMapper.h
    include/crossa/compiler/generators/native/NativeProgramGenerator.h
    include/crossa/packaging/android/AndroidBuildRequirements.h
    include/crossa/packaging/android/AndroidProjectGenerator.h
    include/crossa/compiler/ir/IrExpression.h
    include/crossa/compiler/ir/IrStatement.h
    include/crossa/compiler/ir/IrDeclaration.h
    include/crossa/compiler/ir/IrLowerer.h
    include/crossa/compiler/ir/IrPrinter.h
    include/crossa/compiler/types/SemanticType.h
    include/crossa/compiler/semantic/SemanticScope.h
    include/crossa/compiler/semantic/TypedExpression.h
    include/crossa/compiler/semantic/TypedStatement.h
    include/crossa/compiler/semantic/TypedDeclaration.h
    include/crossa/compiler/semantic/TypedSourceUnit.h
    include/crossa/compiler/semantic/SemanticAnalyzer.h
    include/crossa/compiler/semantic/SemanticModelPrinter.h
    include/crossa/compiler/ir/Program.h
    include/crossa/runtime/RuntimeValue.h
    include/crossa/runtime/RequestHandle.h
    include/crossa/runtime/errors/CrossaError.h
    include/crossa/runtime/errors/CrossaException.h
    include/crossa/runtime/objects/NativeList.h
    include/crossa/runtime/objects/NativeModel.h
    include/crossa/runtime/result/CrossaState.h
    include/crossa/runtime/ExecutionFrame.h
    include/crossa/runtime/ExecutionMode.h
    include/crossa/runtime/IrInterpreter.h
    include/crossa/runtime/ExecutionEngine.h
    src/main.cpp
    src/cli/CrossaApplication.cpp
    src/cli/CrossaVersion.cpp
    src/cli/doctor/DoctorChecks.cpp
    src/cli/doctor/DoctorCommand.cpp
    src/cli/doctor/DoctorRunner.cpp
    src/cli/doctor/ProcessRunner.cpp
    src/compiler/source/SourceFile.cpp
    src/compiler/source/SourceLocation.cpp
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
    src/compiler/project/ProjectLinker.cpp
    src/compiler/generators/kotlin/KotlinGeneratedSource.cpp
    src/compiler/generators/kotlin/KotlinGenerator.cpp
    src/compiler/generators/kotlin/KotlinIdentifierEscaper.cpp
    src/compiler/generators/kotlin/KotlinExpressionEmitter.cpp
    src/compiler/generators/kotlin/KotlinSourceWriter.cpp
    src/compiler/generators/kotlin/KotlinStatementEmitter.cpp
    src/compiler/generators/kotlin/KotlinTypeMapper.cpp
    src/compiler/generators/native/NativeProgramGenerator.cpp
    src/packaging/android/AndroidBuildRequirements.cpp
    src/packaging/android/AndroidProjectGenerator.cpp
    src/compiler/ir/IrExpression.cpp
    src/compiler/ir/IrStatement.cpp
    src/compiler/ir/IrDeclaration.cpp
    src/compiler/ir/IrLowerer.cpp
    src/compiler/ir/IrPrinter.cpp
    src/compiler/types/SemanticType.cpp
    src/compiler/semantic/SemanticScope.cpp
    src/compiler/semantic/TypedExpression.cpp
    src/compiler/semantic/TypedStatement.cpp
    src/compiler/semantic/TypedDeclaration.cpp
    src/compiler/semantic/TypedSourceUnit.cpp
    src/compiler/semantic/SemanticAnalyzer.cpp
    src/compiler/semantic/SemanticModelPrinter.cpp
    src/compiler/ir/Program.cpp
    src/runtime/RuntimeValue.cpp
    src/runtime/RequestHandle.cpp
    src/runtime/errors/CrossaError.cpp
    src/runtime/errors/CrossaException.cpp
    src/runtime/objects/NativeList.cpp
    src/runtime/objects/NativeModel.cpp
    src/runtime/ExecutionFrame.cpp
    src/runtime/IrInterpreter.cpp
    src/runtime/ExecutionEngine.cpp
    src/utils/Log.cpp
    src/utils/PrintUtils.cpp
    include/crossa/compiler/ast/CrossaRequestExpression.h
    include/crossa/compiler/ast/JsonExpression.h
    include/crossa/compiler/ir/IrCrossaRequestExpression.h
    include/crossa/compiler/ir/IrJsonExpression.h
    include/crossa/compiler/semantic/TypedCrossaRequestExpression.h
    include/crossa/compiler/semantic/TypedJsonExpression.h
    include/crossa/network/HttpHeader.h
    include/crossa/network/HttpMethod.h
    include/crossa/network/NetworkConfiguration.h
    include/crossa/network/NetworkPolicy.h
    include/crossa/network/NetworkEngine.h
    include/crossa/network/NetworkInterceptor.h
    include/crossa/network/json/JsonParser.h
    include/crossa/network/json/JsonSerializer.h
    include/crossa/network/json/JsonValue.h
    include/crossa/network/request/PreparedRequest.h
    include/crossa/network/request/RequestBuilder.h
    include/crossa/network/request/RequestSpec.h
    include/crossa/network/response/HttpResponse.h
    include/crossa/network/response/ResponseDecoder.h
    include/crossa/network/transport/CurlTransport.h
    include/crossa/network/utils/UrlUtils.h
    include/crossa/runtime/RuntimeConfiguration.h
    include/crossa/runtime/NativeOperationCatalog.h
    include/crossa/runtime/NativeRuntime.h
    include/crossa/bindings/shared-abi/CrossaAbi.h
    include/crossa/bindings/shared-abi/CrossaRuntimeContext.h
    include/crossa/runtime/scheduler/SchedulerOptions.h
    include/crossa/runtime/scheduler/ScheduledTask.h
    include/crossa/runtime/scheduler/TaskScheduler.h
    src/compiler/ast/CrossaRequestExpression.cpp
    src/compiler/ast/JsonExpression.cpp
    src/compiler/ir/IrCrossaRequestExpression.cpp
    src/compiler/ir/IrJsonExpression.cpp
    src/compiler/semantic/TypedCrossaRequestExpression.cpp
    src/compiler/semantic/TypedJsonExpression.cpp
    src/network/HttpHeader.cpp
    src/network/HttpMethod.cpp
    src/network/NetworkConfiguration.cpp
    src/network/NetworkPolicy.cpp
    src/network/NetworkEngine.cpp
    src/network/NetworkInterceptor.cpp
    src/network/json/JsonParser.cpp
    src/network/json/JsonSerializer.cpp
    src/network/json/JsonValue.cpp
    src/network/request/PreparedRequest.cpp
    src/network/request/RequestBuilder.cpp
    src/network/request/RequestSpec.cpp
    src/network/response/HttpResponse.cpp
    src/network/response/ResponseDecoder.cpp
    src/network/transport/CurlTransport.cpp
    src/network/utils/UrlUtils.cpp
    src/runtime/RuntimeConfiguration.cpp
    src/runtime/NativeOperationCatalog.cpp
    src/runtime/NativeRuntime.cpp
    src/bindings/shared-abi/CrossaAbi.cpp
    src/bindings/shared-abi/CrossaRuntimeContext.cpp
    src/runtime/scheduler/SchedulerOptions.cpp
    src/runtime/scheduler/ScheduledTask.cpp
    src/runtime/scheduler/TaskScheduler.cpp
)

target_compile_features(crossa PRIVATE cxx_std_20)

target_include_directories(
    crossa
    PRIVATE
        include
)

target_link_libraries(
    crossa
    PRIVATE
        CURL::libcurl
        Threads::Threads
)

enable_testing()

add_executable(
    crossa_runtime_tests
    tests/native-runtime-tests.cpp
    src/compiler/ir/IrDeclaration.cpp
    src/compiler/ir/Program.cpp
    src/compiler/source/SourceLocation.cpp
    src/compiler/types/SemanticType.cpp
    src/network/json/JsonParser.cpp
    src/network/json/JsonSerializer.cpp
    src/network/json/JsonValue.cpp
    src/network/response/ResponseDecoder.cpp
    src/runtime/RequestHandle.cpp
    src/runtime/RuntimeValue.cpp
    src/runtime/errors/CrossaError.cpp
    src/runtime/errors/CrossaException.cpp
    src/runtime/objects/NativeList.cpp
    src/runtime/objects/NativeModel.cpp
    src/runtime/scheduler/ScheduledTask.cpp
    src/runtime/scheduler/SchedulerOptions.cpp
    src/runtime/scheduler/TaskScheduler.cpp
    src/utils/Log.cpp
    src/utils/PrintUtils.cpp
)

target_compile_features(crossa_runtime_tests PRIVATE cxx_std_20)
target_include_directories(crossa_runtime_tests PRIVATE include)
target_link_libraries(crossa_runtime_tests PRIVATE Threads::Threads)

add_test(NAME crossa_runtime_tests COMMAND crossa_runtime_tests)

add_executable(
    crossa_language_tests
    tests/language-tests.cpp
    src/compiler/source/SourceFile.cpp
    src/compiler/source/SourceLocation.cpp
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
    src/compiler/ast/CrossaRequestExpression.cpp
    src/compiler/ast/JsonExpression.cpp
    src/compiler/parser/Parser.cpp
    src/compiler/project/ProjectLinker.cpp
    src/compiler/generators/kotlin/KotlinGeneratedSource.cpp
    src/compiler/generators/kotlin/KotlinGenerator.cpp
    src/compiler/generators/kotlin/KotlinIdentifierEscaper.cpp
    src/compiler/generators/kotlin/KotlinExpressionEmitter.cpp
    src/compiler/generators/kotlin/KotlinSourceWriter.cpp
    src/compiler/generators/kotlin/KotlinStatementEmitter.cpp
    src/compiler/generators/kotlin/KotlinTypeMapper.cpp
    src/compiler/generators/native/NativeProgramGenerator.cpp
    src/packaging/android/AndroidProjectGenerator.cpp
    src/packaging/android/AndroidBuildRequirements.cpp
    src/compiler/types/SemanticType.cpp
    src/compiler/semantic/SemanticScope.cpp
    src/compiler/semantic/TypedExpression.cpp
    src/compiler/semantic/TypedStatement.cpp
    src/compiler/semantic/TypedDeclaration.cpp
    src/compiler/semantic/TypedSourceUnit.cpp
    src/compiler/semantic/SemanticAnalyzer.cpp
    src/compiler/semantic/SemanticModelPrinter.cpp
    src/compiler/semantic/TypedCrossaRequestExpression.cpp
    src/compiler/semantic/TypedJsonExpression.cpp
    src/compiler/ir/IrExpression.cpp
    src/compiler/ir/IrStatement.cpp
    src/compiler/ir/IrDeclaration.cpp
    src/compiler/ir/IrLowerer.cpp
    src/compiler/ir/IrPrinter.cpp
    src/compiler/ir/IrCrossaRequestExpression.cpp
    src/compiler/ir/IrJsonExpression.cpp
    src/compiler/ir/Program.cpp
    src/utils/Log.cpp
    src/utils/PrintUtils.cpp
)

target_compile_features(crossa_language_tests PRIVATE cxx_std_20)
target_include_directories(crossa_language_tests PRIVATE include)
target_link_libraries(crossa_language_tests PRIVATE Threads::Threads)

add_test(NAME crossa_language_tests COMMAND crossa_language_tests)
add_test(NAME crossa_script_test COMMAND crossa test.cra)
add_test(
    NAME crossa_conditionals_test
    COMMAND crossa test tests/conditionals.cra
)
add_test(
    NAME crossa_test_runner_pass_test
    COMMAND crossa test tests/test-runner/pass.cra
)
add_test(
    NAME crossa_test_runner_failure_test
    COMMAND crossa test tests/test-runner/failure.cra
)
set_tests_properties(
    crossa_test_runner_pass_test
    crossa_test_runner_failure_test
    PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(
    crossa_test_runner_failure_test
    PROPERTIES WILL_FAIL TRUE
)
add_test(
    NAME crossa_local_network_integration_test
    COMMAND
        bash
        ${CMAKE_SOURCE_DIR}/scripts/run-local-network-tests.sh
        $<TARGET_FILE:crossa>
)
set_tests_properties(
    crossa_local_network_integration_test
    PROPERTIES SKIP_RETURN_CODE 77
)
add_test(
    NAME crossa_check_nested_project_test
    COMMAND crossa check examples/imports/runPosts.cra
)
add_test(
    NAME crossa_explicit_run_test
    COMMAND crossa run tests/import-project/entry/runImports.cra
)
add_test(
    NAME crossa_explicit_test_command_test
    COMMAND crossa test tests/test-runner/pass.cra
)
add_test(
    NAME crossa_script_import_test
    COMMAND crossa tests/import-project/entry/runImports.cra
)
add_test(
    NAME crossa_script_nested_project_test
    COMMAND crossa examples/imports/repositories/postsRepository.cra
)
add_test(
    NAME crossa_script_http_methods_test
    COMMAND crossa tests/all-http-methods.cra
)
add_test(
    NAME crossa_script_config_test
    COMMAND crossa tests/config.cra
)
add_test(
    NAME crossa_script_missing_import_test
    COMMAND crossa tests/import-errors/missing/missingImportEntry.cra
)
set_tests_properties(
    crossa_script_test
    crossa_conditionals_test
    crossa_local_network_integration_test
    crossa_check_nested_project_test
    crossa_explicit_run_test
    crossa_explicit_test_command_test
    crossa_script_import_test
    crossa_script_nested_project_test
    crossa_script_http_methods_test
    crossa_script_config_test
    crossa_script_missing_import_test
    PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
set_tests_properties(
    crossa_script_missing_import_test
    PROPERTIES WILL_FAIL TRUE
)

if(CROSSA_ENABLE_NETWORK_INTEGRATION)
    add_test(
        NAME crossa_network_jsonplaceholder_test
        COMMAND crossa run tests/network-jsonplaceholder.cra
    )
    add_test(
        NAME crossa_network_imported_posts_test
        COMMAND crossa run examples/imports/runPosts.cra
    )
    set_tests_properties(
        crossa_network_jsonplaceholder_test
        crossa_network_imported_posts_test
        PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()
