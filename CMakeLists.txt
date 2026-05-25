cmake_minimum_required(VERSION 3.16)

# Set compiler before project() to ensure it's used
# For MSYS2, support different toolchains (clang64, mingw32, mingw64, etc.)
if(WIN32)
    # If no compiler specified, try to find one
    if(NOT CMAKE_C_COMPILER)
        find_program(CLANG_COMPILER
            NAMES clang.exe clang
            PATHS
                C:/msys64/clang64/bin
                /clang64/bin
                ENV PATH
        )
        if(CLANG_COMPILER)
            set(CMAKE_C_COMPILER "${CLANG_COMPILER}" CACHE FILEPATH "C compiler" FORCE)
            message(STATUS "Using Clang compiler: ${CMAKE_C_COMPILER}")
        else()
            message(WARNING "Clang compiler not found. You may need to specify it manually:")
            message(WARNING "  cmake -DCMAKE_C_COMPILER=C:/msys64/clang64/bin/clang.exe ..")
        endif()
    endif()
    
    # Detect toolchain from compiler path and set PKG_CONFIG_PATH accordingly
    if(CMAKE_C_COMPILER)
        get_filename_component(COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
        get_filename_component(TOOLCHAIN_ROOT "${COMPILER_DIR}" DIRECTORY)
        
        set(TOOLCHAIN_PKG_CONFIG_PATH "${TOOLCHAIN_ROOT}/lib/pkgconfig")
        if(EXISTS "${TOOLCHAIN_PKG_CONFIG_PATH}")
            set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH};${TOOLCHAIN_PKG_CONFIG_PATH}")
            message(STATUS "Detected MSYS2 toolchain: ${TOOLCHAIN_ROOT}")
            message(STATUS "Added pkg-config path: ${TOOLCHAIN_PKG_CONFIG_PATH}")
        endif()
    endif()
endif()

project(WeatherClockGTK VERSION 1.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

# Find required packages
find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED gtk4)
pkg_check_modules(LIBSOUP REQUIRED libsoup-3.0)
pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)

# Source files
set(SOURCES
    src/main.c
    src/clock.c
    src/weather.c
    src/config.c
    src/ui.c
    src/i18n.c
)

# Create executable
add_executable(weatherclock ${SOURCES})

# Include directories
target_include_directories(weatherclock PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GTK4_INCLUDE_DIRS}
    ${LIBSOUP_INCLUDE_DIRS}
    ${JSON_GLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(weatherclock PRIVATE
    ${GTK4_LIBRARIES}
    ${LIBSOUP_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
)

# Link directories
target_link_directories(weatherclock PRIVATE
    ${GTK4_LIBRARY_DIRS}
    ${LIBSOUP_LIBRARY_DIRS}
    ${JSON_GLIB_LIBRARY_DIRS}
)

# Compiler flags from pkg-config
target_compile_options(weatherclock PRIVATE
    ${GTK4_CFLAGS_OTHER}
    ${LIBSOUP_CFLAGS_OTHER}
    ${JSON_GLIB_CFLAGS_OTHER}
)

# Windows-specific settings
if(WIN32)
    set_target_properties(weatherclock PROPERTIES
        WIN32_EXECUTABLE FALSE
        OUTPUT_NAME "weatherclock"
    )
endif()

# Application ID, desktop entry, and icons
set(APP_ID com.weatherclock.app)
set(ICON_SOURCE ${CMAKE_SOURCE_DIR}/data/icons/${APP_ID}.png)

if(NOT EXISTS ${ICON_SOURCE})
    message(FATAL_ERROR "Application icon not found: ${ICON_SOURCE}")
endif()

set(GENERATED_ICONS_DIR ${CMAKE_BINARY_DIR}/generated-icons/hicolor)
foreach(ICON_SIZE 48 128 256 512)
    set(ICON_DEST_DIR ${GENERATED_ICONS_DIR}/${ICON_SIZE}x${ICON_SIZE}/apps)
    file(MAKE_DIRECTORY ${ICON_DEST_DIR})
    configure_file(${ICON_SOURCE} ${ICON_DEST_DIR}/${APP_ID}.png COPYONLY)
endforeach()

install(TARGETS weatherclock
    RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_SOURCE_DIR}/data/${APP_ID}.desktop
        DESTINATION share/applications
)

install(DIRECTORY ${GENERATED_ICONS_DIR}/
        DESTINATION share/icons/hicolor
        FILES_MATCHING PATTERN "${APP_ID}.png"
)

