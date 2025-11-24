cmake_minimum_required(VERSION 3.16)

# Set compiler before project() to ensure it's used
# For MSYS2 clang64, explicitly set the compiler path
if(WIN32)
    # Always try to use clang64 compiler
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

# Set PKG_CONFIG_PATH for clang64 packages on Windows
if(WIN32)
    set(CLANG64_PKG_CONFIG_PATH "C:/msys64/clang64/lib/pkgconfig")
    if(EXISTS "${CLANG64_PKG_CONFIG_PATH}")
        set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH};${CLANG64_PKG_CONFIG_PATH}")
        message(STATUS "Added clang64 pkg-config path: ${CLANG64_PKG_CONFIG_PATH}")
    endif()
endif()

project(WeatherClockGTK VERSION 1.0.0 LANGUAGES C)

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
    main.c
)

# Create executable
add_executable(weatherclock ${SOURCES})

# Include directories
target_include_directories(weatherclock PRIVATE
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

# Installation (optional)
install(TARGETS weatherclock
    RUNTIME DESTINATION bin
)

