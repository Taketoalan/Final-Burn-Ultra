cmake_minimum_required(VERSION 3.16)
project(fbneo-ultra VERSION 1.56 LANGUAGES C CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
find_program(GLIB_COMPILE_RESOURCES NAMES glib-compile-resources REQUIRED)

set(RESOURCE_XML ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.gresource.xml)
set(RESOURCE_C ${CMAKE_CURRENT_BINARY_DIR}/fbneo-ultra-resources.c)

add_custom_command(
    OUTPUT ${RESOURCE_C}
    COMMAND ${GLIB_COMPILE_RESOURCES}
            --generate-source
            --sourcedir=${CMAKE_CURRENT_SOURCE_DIR}/resources
            --target=${RESOURCE_C}
            ${RESOURCE_XML}
    DEPENDS
        ${RESOURCE_XML}
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/style.css
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/fbneo-ultra.svg
    VERBATIM
)

add_executable(fbneo-ultra
    src/main.cpp
    src/main_window.cpp
    src/config.cpp
    src/database.cpp
    src/scanner.cpp
    ${RESOURCE_C}
)

target_include_directories(fbneo-ultra PRIVATE src)
target_link_libraries(fbneo-ultra PRIVATE PkgConfig::GTK3)

target_compile_options(fbneo-ultra PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra -Wpedantic>
)

install(TARGETS fbneo-ultra RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/fbneo-ultra.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES resources/fbneo-ultra.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
