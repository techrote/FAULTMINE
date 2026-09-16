cmake_minimum_required(VERSION 3.25)

project(FAULTMINE VERSION 0.3.0 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "FAULTMINE supports Windows x64 only.")
endif()

if(NOT MSVC)
    message(FATAL_ERROR "FAULTMINE requires MSVC.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "FAULTMINE requires a 64-bit build.")
endif()

include(CTest)

function(faultmine_project_defaults target_name)
    target_compile_features(${target_name} PUBLIC cxx_std_20)
    set_target_properties(${target_name} PROPERTIES
        CXX_EXTENSIONS OFF
    )

    target_compile_definitions(${target_name} PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    target_compile_options(${target_name} PRIVATE
        /W4
        /WX
        /permissive-
        /Zc:preprocessor
        /Zc:__cplusplus
        /EHsc
    )
endfunction()

add_library(faultmine_core STATIC
    src/core/core.cpp
    src/core/determinism.cpp
    src/core/genome.cpp
    src/core/image.cpp
    src/core/json.cpp
    src/core/json.hpp
    src/core/pipeline.cpp
    src/core/sha256.cpp
    src/core/starter_operators.cpp
)

target_include_directories(faultmine_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

faultmine_project_defaults(faultmine_core)

add_library(faultmine_wic STATIC
    src/io/wic_io.cpp
)

target_include_directories(faultmine_wic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(faultmine_wic
    PUBLIC
        faultmine_core
    PRIVATE
        ole32
        windowscodecs
)

faultmine_project_defaults(faultmine_wic)

add_executable(faultmine WIN32
    src/app/main.cpp
    src/platform/win32/main_window.cpp
)

set_target_properties(faultmine PROPERTIES OUTPUT_NAME "FAULTMINE")

target_include_directories(faultmine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(faultmine
    PRIVATE
        faultmine_core
        user32
)

faultmine_project_defaults(faultmine)

add_executable(faultmine_render
    src/tools/render_cli.cpp
)

set_target_properties(faultmine_render PROPERTIES OUTPUT_NAME "FAULTMINE-render")

target_link_libraries(faultmine_render
    PRIVATE
        faultmine_core
        faultmine_wic
)

faultmine_project_defaults(faultmine_render)

if(BUILD_TESTING)
    add_executable(faultmine_tests
        tests/core_smoke.cpp
    )

    target_link_libraries(faultmine_tests PRIVATE faultmine_core)
    faultmine_project_defaults(faultmine_tests)
    add_test(NAME deterministic_core_contracts COMMAND faultmine_tests)

    add_executable(faultmine_image_tests
        tests/image_pipeline_main.cpp
        tests/image_pipeline_tests.cpp
    )

    target_link_libraries(faultmine_image_tests
        PRIVATE
            faultmine_core
            faultmine_wic
    )
    faultmine_project_defaults(faultmine_image_tests)
    add_test(NAME canonical_image_pipeline_contracts COMMAND faultmine_image_tests)
endif()
