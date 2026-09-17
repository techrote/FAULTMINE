cmake_minimum_required(VERSION 3.25)

project(FAULTMINE VERSION 0.13.0 LANGUAGES CXX)

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
    src/core/colour.cpp
    src/core/colour_operators.cpp
    src/core/core.cpp
    src/core/crossover.cpp
    src/core/determinism.cpp
    src/core/fault_catalogue.cpp
    src/core/genome.cpp
    src/core/genome_registry_access.cpp
    src/core/image.cpp
    src/core/json.cpp
    src/core/json.hpp
    src/core/laboratory.cpp
    src/core/logical_address.cpp
    src/core/memory_addressing_operators.cpp
    src/core/mutation.cpp
    src/core/pipeline.cpp
    src/core/proxy.cpp
    src/core/representation_bit_operators.cpp
    src/core/sha256.cpp
    src/core/starter_operators.cpp
    src/core/temporal.cpp
    src/core/temporal_timeline.cpp
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

add_library(faultmine_laboratory_host STATIC
    src/platform/win32/laboratory_worker_host.cpp
)

target_include_directories(faultmine_laboratory_host
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(faultmine_laboratory_host
    PUBLIC
        faultmine_core
)

faultmine_project_defaults(faultmine_laboratory_host)

add_library(faultmine_session STATIC
    src/app/editor.cpp
    src/app/lineage.cpp
    src/app/project_fm013.cpp
    src/app/session_fm013.cpp
    src/app/session_exploration.cpp
    src/app/specimen_tray.cpp
    src/app/specimen_tray_lineage.cpp
)

target_include_directories(faultmine_session
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(faultmine_session
    PUBLIC
        faultmine_core
)

faultmine_project_defaults(faultmine_session)

add_library(faultmine_export STATIC
    src/io/export_fm013.cpp
)

target_include_directories(faultmine_export
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(faultmine_export
    PUBLIC
        faultmine_core
        faultmine_session
        faultmine_wic
)

faultmine_project_defaults(faultmine_export)

add_library(faultmine_d3d11 STATIC
    src/render/d3d11/canvas_renderer.cpp
    src/render/d3d11/canvas_renderer.hpp
)

target_include_directories(faultmine_d3d11
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(faultmine_d3d11
    PUBLIC
        faultmine_core
        faultmine_session
    PRIVATE
        d3d11
        dxgi
        d3dcompiler
)

faultmine_project_defaults(faultmine_d3d11)

add_executable(faultmine WIN32
    src/app/main.cpp
    src/platform/win32/main_window_fm012.cpp
)

set_target_properties(faultmine PROPERTIES OUTPUT_NAME "FAULTMINE")

target_include_directories(faultmine
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(faultmine
    PRIVATE
        faultmine_core
        faultmine_wic
        faultmine_session
        faultmine_export
        faultmine_d3d11
        user32
        gdi32
        comdlg32
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

add_executable(faultmine_lab_worker
    src/worker/laboratory_worker_main.cpp
)

set_target_properties(faultmine_lab_worker PROPERTIES OUTPUT_NAME "FAULTMINE-lab-worker")

target_link_libraries(faultmine_lab_worker
    PRIVATE
        faultmine_core
        faultmine_wic
)

faultmine_project_defaults(faultmine_lab_worker)

add_executable(faultmine_lab
    src/tools/laboratory_cli.cpp
)

set_target_properties(faultmine_lab PROPERTIES OUTPUT_NAME "FAULTMINE-lab")

target_link_libraries(faultmine_lab
    PRIVATE
        faultmine_core
        faultmine_wic
        faultmine_laboratory_host
        faultmine_session
)

faultmine_project_defaults(faultmine_lab)

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

    add_executable(faultmine_session_tests
        tests/session_model_tests.cpp
    )

    target_link_libraries(faultmine_session_tests
        PRIVATE
            faultmine_core
            faultmine_session
    )
    faultmine_project_defaults(faultmine_session_tests)
    add_test(NAME session_proxy_contracts COMMAND faultmine_session_tests)

    add_executable(faultmine_addressing_tests
        tests/memory_addressing_tests.cpp
    )

    target_link_libraries(faultmine_addressing_tests PRIVATE faultmine_core)
    faultmine_project_defaults(faultmine_addressing_tests)
    add_test(NAME memory_addressing_contracts COMMAND faultmine_addressing_tests)

    add_executable(faultmine_representation_tests
        tests/representation_bit_tests.cpp
    )

    target_link_libraries(faultmine_representation_tests PRIVATE faultmine_core)
    faultmine_project_defaults(faultmine_representation_tests)
    add_test(NAME representation_bit_contracts COMMAND faultmine_representation_tests)

    add_executable(faultmine_colour_tests
        tests/colour_tests.cpp
    )

    target_link_libraries(faultmine_colour_tests PRIVATE faultmine_core)
    faultmine_project_defaults(faultmine_colour_tests)
    add_test(NAME colour_lut_contracts COMMAND faultmine_colour_tests)

    add_executable(faultmine_editor_project_tests
        tests/editor_project_tests.cpp
    )

    target_link_libraries(faultmine_editor_project_tests
        PRIVATE
            faultmine_core
            faultmine_session
    )
    faultmine_project_defaults(faultmine_editor_project_tests)
    add_test(NAME editor_project_contracts COMMAND faultmine_editor_project_tests)

    add_executable(faultmine_mutation_tests
        tests/mutation_tests.cpp
    )

    target_link_libraries(faultmine_mutation_tests
        PRIVATE
            faultmine_core
            faultmine_session
    )
    faultmine_project_defaults(faultmine_mutation_tests)
    add_test(NAME deterministic_mutation_tray_contracts COMMAND faultmine_mutation_tests)

    add_executable(faultmine_lineage_tests
        tests/lineage_crossover_tests.cpp
    )

    target_link_libraries(faultmine_lineage_tests
        PRIVATE
            faultmine_core
            faultmine_session
    )
    faultmine_project_defaults(faultmine_lineage_tests)
    add_test(NAME deterministic_crossover_lineage_contracts COMMAND faultmine_lineage_tests)

    add_executable(faultmine_temporal_tests
        tests/temporal_tests.cpp
    )

    target_link_libraries(faultmine_temporal_tests
        PRIVATE
            faultmine_core
            faultmine_session
    )
    faultmine_project_defaults(faultmine_temporal_tests)
    add_test(NAME deterministic_temporal_contracts COMMAND faultmine_temporal_tests)

    add_executable(faultmine_export_tests
        tests/export_tests.cpp
    )

    target_link_libraries(faultmine_export_tests
        PRIVATE
            faultmine_core
            faultmine_session
            faultmine_wic
            faultmine_export
    )
    faultmine_project_defaults(faultmine_export_tests)
    add_test(NAME canonical_export_contracts COMMAND faultmine_export_tests)

    add_executable(faultmine_laboratory_tests
        tests/laboratory_tests.cpp
    )

    target_link_libraries(faultmine_laboratory_tests
        PRIVATE
            faultmine_core
            faultmine_wic
            faultmine_laboratory_host
            faultmine_session
            faultmine_export
    )
    faultmine_project_defaults(faultmine_laboratory_tests)
    add_dependencies(faultmine_laboratory_tests faultmine_lab_worker)
    add_test(NAME isolated_laboratory_contracts COMMAND faultmine_laboratory_tests)
endif()
