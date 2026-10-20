cmake_minimum_required(VERSION 3.16)
# Public version shown by About and used by release packaging.
project(goliath-qt VERSION 0.41.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(GOLIATH_STRICT_WARNINGS
    "Enable additional compiler warnings for validation builds" OFF)
option(GOLIATH_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors" OFF)
option(GOLIATH_ENABLE_SANITIZERS
    "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(GOLIATH_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR
            "GOLIATH_ENABLE_SANITIZERS requires a GCC- or Clang-compatible "
            "AddressSanitizer and UndefinedBehaviorSanitizer toolchain")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        include(CheckCXXSourceCompiles)
        include(CMakePushCheckState)

        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_FLAGS
            "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all")
        list(APPEND CMAKE_REQUIRED_LINK_OPTIONS
            -fsanitize=address,undefined)
        unset(GOLIATH_SANITIZERS_AVAILABLE CACHE)
        check_cxx_source_compiles(
            "int main() { return 0; }"
            GOLIATH_SANITIZERS_AVAILABLE)
        cmake_pop_check_state()

        if(NOT GOLIATH_SANITIZERS_AVAILABLE)
            message(FATAL_ERROR
                "The selected compiler cannot compile and link an "
                "AddressSanitizer/UndefinedBehaviorSanitizer executable")
        endif()
    else()
        message(FATAL_ERROR
            "GOLIATH_ENABLE_SANITIZERS is unsupported by the selected compiler")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)
find_package(SDL3 REQUIRED CONFIG)

string(TIMESTAMP GOLIATH_BUILD_DATE "%Y-%m-%d" UTC)
set(GOLIATH_BUILD_REVISION "" CACHE STRING
    "Build revision shown by About Goliath (auto-detected when empty)")
set(GOLIATH_PROJECT_URL "https://github.com/epsx/Goliath" CACHE STRING
    "Published Goliath project URL shown by About Goliath")
set(GOLIATH_LICENSE_URL "https://www.gnu.org/licenses/gpl-3.0.html" CACHE STRING
    "Published Goliath license URL shown by About Goliath")
set(GOLIATH_RELEASES_API_URL
    "https://api.github.com/repos/epsx/Goliath/releases?per_page=20" CACHE STRING
    "GitHub releases API URL used by the manual update checker")

if(NOT GOLIATH_BUILD_REVISION)
    set(GOLIATH_BUILD_REVISION "local-uncommitted")
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
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=normal
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                RESULT_VARIABLE GOLIATH_GIT_STATUS_RESULT
                OUTPUT_VARIABLE GOLIATH_GIT_CHANGES
                ERROR_QUIET
            )
            if(GOLIATH_GIT_STATUS_RESULT EQUAL 0 AND NOT GOLIATH_GIT_CHANGES)
                set(GOLIATH_BUILD_REVISION "${GOLIATH_GIT_REVISION}")
            endif()
        endif()
    endif()
endif()

function(goliath_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
        if(GOLIATH_STRICT_WARNINGS)
            target_compile_options(${target} PRIVATE
                /permissive-
                /w14242
                /w14254
                /w14263
                /w14265
                /w14287
                /w14296
                /w14311
                /w14826
            )
        endif()
        if(GOLIATH_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
        if(GOLIATH_STRICT_WARNINGS)
            target_compile_options(${target} PRIVATE
                -Wpedantic
                -Wconversion
                -Wsign-conversion
                -Wshadow
                -Wformat=2
            )
        endif()
        if(GOLIATH_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(goliath_enable_sanitizers target)
    if(NOT GOLIATH_ENABLE_SANITIZERS)
        return()
    endif()

    target_compile_options(${target} PRIVATE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
        -fno-sanitize-recover=all
    )

    get_target_property(goliath_target_type ${target} TYPE)
    if(NOT goliath_target_type STREQUAL "STATIC_LIBRARY")
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined
        )
    endif()
endfunction()

add_library(goliath-core STATIC
    src/game/audio_export.cpp
    src/game/animated_gif.cpp
    src/game/command_dat.cpp
    src/game/command_notation.cpp
    src/game/game_model.cpp
    src/game/game_integrity.cpp
    src/game/library_rescan_comparison.cpp
    src/game/game_library_state.cpp
    src/game/game_playtime.cpp
    src/game/game_profile.cpp
    src/game/game_profile_runtime.cpp
    src/game/game_profile_summary.cpp
    src/ui/library_details_presentation.cpp
    src/ui/library_view_logic.cpp
    src/game/save_data_manager.cpp
    src/game/video_settings.cpp
    src/common/file_transaction.cpp
    src/common/filesystem_safety.cpp
    src/common/diagnostic_report.cpp
    src/common/goliath_common.cpp
    src/common/qt_path.cpp
    src/common/debug_logger.cpp
    src/common/log_file.cpp
    src/common/notification_store.cpp
    src/common/window_geometry.cpp
    src/common/sha1.cpp
    src/update/update_alert.cpp
    src/update/release_feed.cpp
    src/update/update_schedule.cpp
    src/game/hash_cache.cpp
    src/game/crc32_cache.cpp
    src/game/sha1_cache.cpp
    src/game/filesystem_io.cpp
    src/game/geolith_verification.cpp
    src/game/neogeo_metadata.cpp
    src/game/neocd_verification.cpp
    src/audio/audio_devices.cpp
    src/ini/ini_document.cpp
    src/input/input_maps.cpp
    src/input/controller_profiles.cpp
    src/input/joystick_listener.cpp
    src/input/sdl_joystick.cpp
    src/game/jollygood_benchmark.cpp
    src/game/jollygood_process.cpp
    src/game/jollygood_startup_diagnostics.cpp
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

if(GOLIATH_STRICT_WARNINGS)
    if(MSVC)
        set(goliath_third_party_warning_options /W0)
    else()
        set(goliath_third_party_warning_options -w)
    endif()
    set_source_files_properties(
        third_party/miniz.c
        third_party/miniz_tdef.c
        third_party/miniz_tinfl.c
        third_party/miniz_zip.c
        third_party/tinyxml2.cpp
        PROPERTIES COMPILE_OPTIONS "${goliath_third_party_warning_options}"
    )
elseif(NOT MSVC)
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
goliath_enable_sanitizers(goliath-core)

add_executable(goliath-qt
    assets/app_icon.qrc
    src/main.cpp
    src/ui/about_dialog.cpp
    src/ui/bios_verification_dialog.cpp
    src/ui/update_check.cpp
    src/ui/command_dialog.cpp
    src/ui/main_window.cpp
    src/ui/main_window_dialogs.cpp
    src/ui/main_window_diagnostics.cpp
    src/ui/main_window_events.cpp
    src/ui/main_window_data.cpp
    src/ui/main_window_runtime.cpp
    src/ui/main_window_updates.cpp
    src/ui/main_window_notifications.cpp
    src/ui/game_screenshot.cpp
    src/ui/game_gallery.cpp
    src/ui/library_item_delegate.cpp
    src/ui/main_window_ui.cpp
    src/ui/main_window_theme.cpp
    src/ui/main_window_toolbar.cpp
    src/ui/main_window_details_ui.cpp
    src/ui/main_window_library_ui.cpp
    src/ui/main_window_library_interaction.cpp
    src/ui/main_window_library_actions.cpp
    src/ui/main_window_library_controls.cpp
    src/ui/main_window_library_state.cpp
    src/ui/main_window_library_details.cpp
    src/ui/main_window_library_view.cpp
    src/ui/library_verification_dialog.cpp
    src/ui/widgets/command_notation_view.cpp
    src/ui/widgets/engraved_controls.cpp
    src/ui/widgets/main_window_panel_widgets.cpp
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
    src/ui/controller_profiles_dialog.cpp
    src/ui/audio_export_dialog.cpp
    src/ui/benchmark_dialog.cpp
    src/ui/game_profile_dialog.cpp
    src/ui/logging_dialog.cpp
    src/ui/notification_center_dialog.cpp
    src/ui/verification_report_dialog.cpp
    src/ui/lithogen_dialog.cpp
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
    Qt6::Network
)
if(WIN32)
    target_sources(goliath-qt PRIVATE src/ui/windows_desktop_capture.cpp)
    target_link_libraries(goliath-qt PRIVATE user32 d3d11 dxgi dwmapi)
endif()
if(UNIX AND NOT APPLE)
    option(GOLIATH_REQUIRE_WAYLAND_CAPTURE
        "Fail configure when Wayland portal/PipeWire development files are missing" OFF)
    find_package(Qt6 QUIET COMPONENTS DBus)
    find_package(PkgConfig QUIET)
    if(Qt6DBus_FOUND AND PkgConfig_FOUND)
        pkg_check_modules(PIPEWIRE QUIET IMPORTED_TARGET libpipewire-0.3)
    endif()
    if(Qt6DBus_FOUND AND PIPEWIRE_FOUND)
        target_sources(goliath-qt PRIVATE src/ui/wayland_gif_capture.cpp
            src/ui/wayland_gif_hotkey.cpp)
        target_link_libraries(goliath-qt PRIVATE Qt6::DBus PkgConfig::PIPEWIRE)
        target_compile_definitions(goliath-qt PRIVATE GOLIATH_WAYLAND_CAPTURE=1)
    elseif(GOLIATH_REQUIRE_WAYLAND_CAPTURE)
        message(FATAL_ERROR "Wayland GIF capture requires Qt6 DBus, pkgconf and libpipewire")
    endif()
    option(GOLIATH_REQUIRE_X11_CAPTURE "Fail configure when X11 development files are missing" OFF)
    find_package(X11 QUIET)
    if(X11_FOUND)
        target_link_libraries(goliath-qt PRIVATE X11::X11)
        target_compile_definitions(goliath-qt PRIVATE GOLIATH_X11_CAPTURE=1)
    elseif(GOLIATH_REQUIRE_X11_CAPTURE)
        message(FATAL_ERROR "Install libx11-dev to build X11 GIF capture")
    endif()
endif()

target_compile_definitions(goliath-qt PRIVATE
    GOLIATH_VERSION="${PROJECT_VERSION}"
    GOLIATH_BUILD_DATE="${GOLIATH_BUILD_DATE}"
    GOLIATH_BUILD_REVISION="${GOLIATH_BUILD_REVISION}"
    GOLIATH_PROJECT_URL="${GOLIATH_PROJECT_URL}"
    GOLIATH_LICENSE_URL="${GOLIATH_LICENSE_URL}"
    GOLIATH_RELEASES_API_URL="${GOLIATH_RELEASES_API_URL}"
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
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/metadata"
        "$<TARGET_FILE_DIR:goliath-qt>/metadata"
    COMMENT "Copying Goliath license and baseline metadata materials"
)

goliath_enable_warnings(goliath-qt)
goliath_enable_sanitizers(goliath-qt)

if(WIN32)
    enable_language(RC)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/app_icon.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/goliath-app.rc"
        @ONLY
    )
    set_target_properties(goliath-qt PROPERTIES WIN32_EXECUTABLE TRUE)
    target_sources(goliath-qt PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/goliath-app.rc"
    )
endif()

# MSYS2's Qt libraries already use the dynamic MinGW runtimes. Keep Goliath on
# the same runtime instance by default; embedding another complete winpthread
# runtime can deadlock Qt worker threads after toolchain updates.
option(GOLIATH_STATIC_MINGW_RUNTIME
    "Statically link the MinGW GCC, C++ and winpthread runtimes" OFF)

if(MINGW AND GOLIATH_STATIC_MINGW_RUNTIME)
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
        tests/test_temp_directory.hpp
        tests/test_main.cpp
        tests/test_goliath_config.cpp
        tests/test_ini_document.cpp
        tests/test_bios_verify.cpp
        tests/test_input_maps.cpp
        tests/test_controller_profiles.cpp
        tests/test_capture_hotkeys.cpp
        tests/test_audio.cpp
        tests/test_audio_export.cpp
        tests/test_animated_gif.cpp
        tests/test_command_dat.cpp
        tests/test_command_notation.cpp
        tests/test_jollygood_benchmark.cpp
        tests/test_jollygood_bios.cpp
        tests/test_settings_fields.cpp
        tests/test_sha1.cpp
        tests/test_sha1_cache.cpp
        tests/test_db_scanner.cpp
        tests/test_game_model.cpp
        tests/test_game_integrity.cpp
        tests/test_library_rescan_comparison.cpp
        tests/test_game_library_state.cpp
        tests/test_game_system.cpp
        tests/test_game_profiles.cpp
        tests/test_game_playtime.cpp
        tests/test_jollygood_startup_diagnostics.cpp
        tests/test_library_details_presentation.cpp
        tests/test_library_view_logic.cpp
        tests/test_gallery_frame.cpp
        tests/test_log_file.cpp
        tests/test_notification_store.cpp
        tests/test_diagnostic_report.cpp
        tests/test_save_data_manager.cpp
        tests/test_theme.cpp
        tests/test_window_geometry.cpp
        tests/test_project_legal.cpp
        tests/test_qt_path.cpp
        tests/test_update_alert.cpp
        tests/test_release_feed.cpp
        tests/test_update_schedule.cpp
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
        Qt6::Gui
    )

    goliath_enable_warnings(goliath-qt-tests)
    goliath_enable_sanitizers(goliath-qt-tests)

    if(MINGW AND GOLIATH_STATIC_MINGW_RUNTIME)
        # Match the GUI target using the same filtered static archive.
        target_link_options(goliath-qt-tests PRIVATE
            -static-libgcc -static-libstdc++
            -Wl,--whole-archive
            "${GOLIATH_FILTERED_WINPTHREAD_ARCHIVE}"
            -Wl,--no-whole-archive
        )
    endif()

    if(MINGW)
        # Place -mconsole after Qt so it wins if Qt's import lib implies -mwindows.
        target_link_libraries(goliath-qt-tests PRIVATE -mconsole)
    endif()

    set_target_properties(goliath-qt-tests PROPERTIES WIN32_EXECUTABLE FALSE)

    add_test(NAME goliath-qt-tests COMMAND goliath-qt-tests)
endif()
