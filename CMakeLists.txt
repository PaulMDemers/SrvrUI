cmake_minimum_required(VERSION 3.16)
project(SrvrUI LANGUAGES C)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Portable controls. OS-specific battery, listing, file search and menu models
# are intentionally excluded until their protocols have a portable boundary.
add_library(srvrui STATIC
    render/desk_render.c render/desk_font.c
    src/bitmap.c src/calendar.c src/check.c src/color_panel.c src/dialog.c
    src/document.c src/edit.c src/file_picker.c src/flow.c src/image.c
    src/list.c src/notice.c src/paint.c src/palette.c src/path.c
    src/prompt.c src/properties.c src/searchbar.c src/slider.c src/table.c
    src/tabs.c src/terminal.c src/terminal_view.c src/text.c src/text_search.c
    src/time_model.c src/ui.c src/widgets.c)
target_include_directories(srvrui PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(srvrui PUBLIC c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(srvrui PRIVATE -Wall -Wextra -Werror)
endif()

option(SRVRUI_BUILD_SDL3 "Build the optional SDL3 adapter and interactive showcase" ON)
option(SRVRUI_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(SRVRUI_SANITIZE AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(srvrui PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(srvrui PUBLIC -fsanitize=address,undefined)
endif()
if(SRVRUI_BUILD_SDL3)
    find_package(SDL3 3.2 CONFIG REQUIRED)
    add_library(srvrui_sdl3 STATIC adapters/sdl3/srvrui_sdl3.c)
    target_include_directories(srvrui_sdl3 PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/adapters/sdl3>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/srvrui>)
    target_link_libraries(srvrui_sdl3 PUBLIC srvrui SDL3::SDL3)
    if(UNIX)
        target_link_libraries(srvrui_sdl3 PRIVATE m)
    endif()
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(srvrui_sdl3 PRIVATE -Wall -Wextra -Werror)
    endif()
    add_executable(showcase examples/showcase.c)
    target_link_libraries(showcase PRIVATE srvrui_sdl3)
    if(APPLE)
        add_executable(SrvrUIShowcase MACOSX_BUNDLE examples/showcase.c)
        target_sources(SrvrUIShowcase PRIVATE assets/fonts/LICENSE-DejaVu.txt assets/licenses/LICENSE-SDL.txt)
        set_source_files_properties(assets/fonts/LICENSE-DejaVu.txt assets/licenses/LICENSE-SDL.txt
            PROPERTIES MACOSX_PACKAGE_LOCATION Resources/Licenses)
        target_link_libraries(SrvrUIShowcase PRIVATE srvrui_sdl3)
        set_target_properties(SrvrUIShowcase PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "dev.srvros.srvrui-showcase"
            MACOSX_BUNDLE_BUNDLE_NAME "SrvrUI Showcase"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
        install(TARGETS SrvrUIShowcase BUNDLE DESTINATION .)
    endif()
endif()

install(TARGETS srvrui EXPORT SrvrUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/srvros DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    PATTERN desk_battery.h EXCLUDE PATTERN desk_listing.h EXCLUDE
    PATTERN desk_file_search.h EXCLUDE PATTERN desk_menu.h EXCLUDE PATTERN desk_popup.h EXCLUDE)
if(SRVRUI_BUILD_SDL3)
    install(TARGETS srvrui_sdl3 EXPORT SrvrUITargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES adapters/sdl3/srvrui_sdl3.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/srvrui)
    install(TARGETS showcase RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
configure_package_config_file(cmake/SrvrUIConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/SrvrUIConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SrvrUI)
install(EXPORT SrvrUITargets NAMESPACE SrvrUI:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SrvrUI)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/SrvrUIConfig.cmake DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SrvrUI)
install(FILES assets/fonts/LICENSE-DejaVu.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SrvrUI)
if(SRVRUI_BUILD_SDL3)
    install(FILES assets/licenses/LICENSE-SDL.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/SrvrUI)
endif()

include(CTest)
if(BUILD_TESTING)
    foreach(name IN ITEMS widgets widget_text widget_edit widget_ui widget_theme)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE srvrui)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(test_${name} PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()
    if(SRVRUI_BUILD_SDL3)
        add_executable(test_sdl3 tests/test_sdl3.c)
        target_link_libraries(test_sdl3 PRIVATE srvrui_sdl3)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(test_sdl3 PRIVATE -UNDEBUG -Wall -Wextra -Werror)
        endif()
        add_test(NAME sdl3_adapter COMMAND test_sdl3)
        set_tests_properties(sdl3_adapter PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
        add_test(NAME showcase_smoke COMMAND showcase --smoke ${CMAKE_CURRENT_BINARY_DIR}/review)
        set_tests_properties(showcase_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME widget_boundary COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_widget_boundary.py $<TARGET_FILE:srvrui> --nm ${CMAKE_NM})
    # In a matched OS checkout, catch drift in the shared contract snapshots.
    set(os_shared "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/include/srvros")
    if(EXISTS "${os_shared}/key_event.h")
        foreach(header IN ITEMS key_event calendar image_limits)
            add_test(NAME contract_${header} COMMAND ${CMAKE_COMMAND} -E compare_files
                "${CMAKE_CURRENT_SOURCE_DIR}/include/srvros/${header}.h"
                "${os_shared}/${header}.h")
        endforeach()
    endif()
    add_test(NAME widget_boundary_checker COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_widget_boundary.py)
endif()
