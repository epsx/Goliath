cmake_minimum_required(VERSION 3.16)
# Development versioning follows Patch NN[A-Z] -> 0.NN.letter-index.
project(goliath-qt VERSION 0.30.2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(SDL3 REQUIRED CONFIG)

string(TIMESTAMP GOLIATH_BUILD_DATE "%Y-%m-%d" UTC)
set(GOLIATH_BUILD_REVISION "" CACHE STRING
    "Build revision shown by About Goliath (auto-detected when empty)")
set(GOLIATH_PROJECT_URL "https://github.com/epsx/Goliath" CACHE STRING
    "Published Goliath project URL shown by About Goliath")
set(GOLIATH_LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html" CACHE STRING
    "Published Goliath license URL shown by About Goliath")

if(NOT GOLIATH_BUILD_REVISION)
    set(GOLIATH_BUILD_REVISION "patch30b")
    find_package(Git QUIET)
    if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            RESULT_VARIABLE GOLIATH_GIT_RESULT
            OUTPUT_VARIABLE GOLIATH_GIT_REVISION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(GOLIATH_GIT_RESULT EQUAL 0 AND GOLIATH_GIT_REVISION)
            set(GOLIATH_BUILD_REVISION "${GOLIATH_GIT_REVISION}")
        endif()
    endif()
endif()

function(goliath_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

add_library(goliath-core STATIC
    src/game/audio_export.cpp
    src/game/game_model.cpp
    src/game/game_playtime.cpp
    src/game/game_profile.cpp
    src/game/game_profile_runtime.cpp
    src/game/save_data_manager.cpp
    src/game/video_settings.cpp
    src/common/filesystem_safety.cpp
    src/common/goliath_common.cpp
    src/common/debug_logger.cpp
    src/common/log_file.cpp
    src/common/window_geometry.cpp
    src/common/sha1.cpp
    src/game/sha1_cache.cpp
    src/game/filesystem_io.cpp
    src/game/neogeo_metadata.cpp
    src/game/neocd_verification.cpp
    src/audio/audio_devices.cpp
    src/ini/ini_document.cpp
    src/input/input_maps.cpp
    src/input/joystick_listener.cpp
    src/input/sdl_joystick.cpp
    src/game/jollygood_benchmark.cpp
    src/game/jollygood_process.cpp
    src/game/jollygood_launch.cpp
    src/game/jollygood_bios.cpp
    src/game/jollygood_executable.cpp
    src/game/geolith_capabilities.cpp
    src/game/bios_verify.cpp
    src/game/db_scanner.cpp
    third_party/miniz.c
    third_party/miniz_tdef.c
    third_party/miniz_tinfl.c
    third_party/miniz_zip.c
    third_party/tinyxml2.cpp
)

if(NOT MSVC)
    set_source_files_properties(
        third_party/miniz.c
        third_party/miniz_tdef.c
        third_party/miniz_tinfl.c
        third_party/miniz_zip.c
        PROPERTIES COMPILE_OPTIONS -Wno-type-limits
    )
endif()

target_include_directories(goliath-core PUBLIC
    src
)

target_include_directories(goliath-core SYSTEM PRIVATE
    third_party
)

target_link_libraries(goliath-core
    PUBLIC
        Qt6::Core
    PRIVATE
        SDL3::SDL3
)

if(WIN32)
    target_link_libraries(goliath-core PRIVATE ole32 uuid)
endif()

goliath_enable_warnings(goliath-core)

add_executable(goliath-qt
    assets/app_icon.qrc
    src/main.cpp
    src/ui/about_dialog.cpp
    src/ui/main_window.cpp
    src/ui/main_window_ui.cpp
    src/ui/main_window_library.cpp
    src/ui/widgets/title_bar.cpp
    src/common/theme.cpp
    src/common/paths.cpp
    src/audio/jollygood_audio.cpp
    src/ui/widgets/settings_field.cpp
    src/ui/widgets/optional_video_fields.cpp
    src/ui/tabs/video_tab.cpp
    src/ui/tabs/audio_tab.cpp
    src/ui/tabs/misc_tab.cpp
    src/ui/tabs/info_tab.cpp
    src/ui/settings_dialog.cpp
    src/ui/audio_export_dialog.cpp
    src/ui/benchmark_dialog.cpp
    src/ui/game_profile_dialog.cpp
    src/ui/logging_dialog.cpp
    src/ui/save_data_dialog.cpp
    src/game/detached_process_tracker.cpp
    src/game/rescan_worker.cpp
    src/ui/rescan_dialog.cpp
    src/ui/widgets/controller_button.cpp
    src/ui/widgets/game_input_mapping_widget.cpp
    src/ui/widgets/input_panel_widgets.cpp
    src/ui/tabs/core_tab.cpp
)

target_link_libraries(goliath-qt PRIVATE
    goliath-core
    Qt6::Widgets
)

target_compile_definitions(goliath-qt PRIVATE
    GOLIATH_VERSION="${PROJECT_VERSION}"
    GOLIATH_BUILD_DATE="${GOLIATH_BUILD_DATE}"
    GOLIATH_BUILD_REVISION="${GOLIATH_BUILD_REVISION}"
    GOLIATH_PROJECT_URL="${GOLIATH_PROJECT_URL}"
    GOLIATH_LICENSE_URL="${GOLIATH_LICENSE_URL}"
)

add_custom_command(TARGET goliath-qt POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "$<TARGET_FILE_DIR:goliath-qt>/LICENSE"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
        "$<TARGET_FILE_DIR:goliath-qt>/THIRD-PARTY-NOTICES.md"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/licenses"
        "$<TARGET_FILE_DIR:goliath-qt>/licenses"
    COMMENT "Copying Goliath and third-party license materials"
)

goliath_enable_warnings(goliath-qt)

if(WIN32)
    enable_language(RC)
    set_target_properties(goliath-qt PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(goliath-qt PRIVATE assets/app_icon.rc)
endif()

if(MINGW)
    # Work from a build-local copy so the installed toolchain archive remains
    # untouched while its unrelated DLL VERSION resource is excluded.
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" -print-file-name=libwinpthread.a
        OUTPUT_VARIABLE GOLIATH_WINPTHREAD_ARCHIVE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE GOLIATH_WINPTHREAD_LOOKUP_RESULT
    )
    if(NOT GOLIATH_WINPTHREAD_LOOKUP_RESULT EQUAL 0 OR
       NOT EXISTS "${GOLIATH_WINPTHREAD_ARCHIVE}")
        message(FATAL_ERROR "Unable to locate the static winpthread archive")
    endif()

    set(GOLIATH_FILTERED_WINPTHREAD_ARCHIVE
        "${CMAKE_CURRENT_BINARY_DIR}/libwinpthread-goliath.a")
    configure_file(
        "${GOLIATH_WINPTHREAD_ARCHIVE}"
        "${GOLIATH_FILTERED_WINPTHREAD_ARCHIVE}"
        COPYONLY
    )
    execute_process(
        COMMAND "${CMAKE_AR}" d
            "${GOLIATH_FILTERED_WINPTHREAD_ARCHIVE}" version.o
        RESULT_VARIABLE GOLIATH_WINPTHREAD_FILTER_RESULT
    )
    if(NOT GOLIATH_WINPTHREAD_FILTER_RESULT EQUAL 0)
        message(FATAL_ERROR "Unable to filter the winpthread VERSION resource")
    endif()
    execute_process(
        COMMAND "${CMAKE_RANLIB}" "${GOLIATH_FILTERED_WINPTHREAD_ARCHIVE}"
        RESULT_VARIABLE GOLIATH_WINPTHREAD_RANLIB_RESULT
    )
    if(NOT GOLIATH_WINPTHREAD_RANLIB_RESULT EQUAL 0)
        message(FATAL_ERROR "Unable to index the filtered winpthread archive")
    endif()

    # Force the complete filtered archive into the executable so references
    # introduced by implicit runtime libraries cannot become DLL dependencies.
    target_link_options(goliath-qt PRIVATE
        -static-libgcc -static-libstdc++
        -Wl,--whole-archive
        "${GOLIATH_FILTERED_WINPTHREAD_ARCHIVE}"
        -Wl,--no-whole-archive
    )
endif()

option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()

    add_executable(goliath-qt-tests
        tests/test_main.cpp
        tests/test_ini_document.cpp
        tests/test_bios_verify.cpp
        tests/test_input_maps.cpp
        tests/test_audio.cpp
        tests/test_audio_export.cpp
        tests/test_jollygood_benchmark.cpp
        tests/test_jollygood_bios.cpp
        tests/test_settings_fields.cpp
        tests/test_sha1.cpp
        tests/test_sha1_cache.cpp
        tests/test_db_scanner.cpp
        tests/test_game_model.cpp
        tests/test_game_system.cpp
        tests/test_game_profiles.cpp
        tests/test_game_playtime.cpp
        tests/test_library_view_logic.cpp
        tests/test_log_file.cpp
        tests/test_save_data_manager.cpp
        tests/test_theme.cpp
        tests/test_window_geometry.cpp
        tests/test_project_legal.cpp
        src/common/theme.cpp
    )

    target_include_directories(goliath-qt-tests SYSTEM PRIVATE
        tests/third_party
        third_party
    )

    # Keep Qt explicit so the MinGW -mconsole item below remains after it.
    target_link_libraries(goliath-qt-tests PRIVATE
        goliath-core
        Qt6::Core
    )

    goliath_enable_warnings(goliath-qt-tests)

    if(MINGW)
        # Match the GUI target using the same filtered static archive.
        target_link_options(goliath-qt-tests PRIVATE
            -static-libgcc -static-libstdc++
            -Wl,--whole-archive
            "${GOLIATH_FILTERED_WINPTHREAD_ARCHIVE}"
            -Wl,--no-whole-archive
        )

        # Place -mconsole after Qt so it wins if Qt's import lib implies -mwindows.
        target_link_libraries(goliath-qt-tests PRIVATE -mconsole)
    endif()

    set_target_properties(goliath-qt-tests PROPERTIES WIN32_EXECUTABLE FALSE)

    add_test(NAME goliath-qt-tests COMMAND goliath-qt-tests)
endif()
