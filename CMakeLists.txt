cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

project(LHC)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}")

if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -ansi -lm -pthread")
endif (CMAKE_COMPILER_IS_GNUCC)

set(ALL_FOUND TRUE)

option(WITH_GNU_READLINE "Use GNU readline for input" ON)
set(SAMPLE_BUFFER_SIZE 1024 CACHE STRING "Length of a sample buffer")
set(SAMPLE_BUFFER_COUNT 2 CACHE STRING "Number of buffers for playing signals. Has to be >= 2")
set(FILTER_WINDOW_SIZE 32768 CACHE STRING "Length of a sample buffer")

math(EXPR FILTER_WINDOW_SIZE_OK "${FILTER_WINDOW_SIZE} % ${SAMPLE_BUFFER_SIZE}")
if (NOT (FILTER_WINDOW_SIZE_OK EQUAL 0))
    math(EXPR RFWS "${SAMPLE_BUFFER_SIZE} * 32")
    message(SEND_ERROR "Chosse the filter window size to be a multiple of the sampe buffer size (${RFWS} for example).")
endif (NOT (FILTER_WINDOW_SIZE_OK EQUAL 0))

# PortAudio
find_library(PA_LIBRARY NAMES portaudio)
find_path(PA_INCLUDE_DIR portaudio.h)
set(PA_FOUND FALSE)
if (PA_LIBRARY AND PA_INCLUDE_DIR)
    set(PA_FOUND TRUE)
endif (PA_LIBRARY AND PA_INCLUDE_DIR)

if (PA_FOUND)
    include_directories(${PA_INCLUDE_DIR})
    set(LIBS ${LIBS} ${OPENAL_LIBRARY} ${PA_LIBRARY})
else (PA_FOUND)
    message("Cannot find PortAudio")
    set(ALL_FOUND FALSE)
endif (PA_FOUND)

# lua
find_package(Lua51)
if (LUA51_FOUND)
    include_directories(${LUA_INCLUDE_DIR})
    set(LIBS ${LIBS} ${LUA_LIBRARIES})
else (LUA51_FOUND)
    message("Cannot find Lua 5.1")
    set(ALL_FOUND FALSE)
endif (LUA51_FOUND)

# FFTW3
find_path(FFTW3_INCLUDE_DIR fftw3.h)
find_library(FFTW3_LIBRARY NAMES fftw3) 

if (FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)
    set(FFTW3_FOUND TRUE)
endif (FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)

if (FFTW3_FOUND)
    include_directories(${FFTW3_INCLUDE_DIR})
    set(LIBS ${LIBS} ${FFTW3_LIBRARY})
else (FFTW3_FOUND)
    message("Cannot find FFTW3")
    set(ALL_FOUND FALSE)
endif (FFTW3_FOUND)

# readline
if (WITH_GNU_READLINE)
    find_path(READLINE_INCLUDE_DIR readline/readline.h)
    find_library(READLINE_LIBRARY NAMES readline) 

    if (READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
        set(READLINE_FOUND TRUE)
    endif (READLINE_INCLUDE_DIR AND READLINE_LIBRARY)

    if (READLINE_FOUND)
        include_directories(${READLINE_INCLUDE_DIR})
        set(LIBS ${LIBS} ${READLINE_LIBRARY})
    else (READLINE_FOUND)
        message("Cannot find GNU Readline")
        set(ALL_FOUND FALSE)
    endif (READLINE_FOUND)
endif (WITH_GNU_READLINE)


macro(add_subdir var dir)
    foreach (file ${ARGN})
        set (${var} ${${var}} ${dir}/${file})
    endforeach (file)
    source_group(${dir} FILES ${var})
endmacro(add_subdir)

if (ALL_FOUND)
    configure_file(src/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    add_subdir(SRCS src
        lhc.c
        generators.c
        filter_helper.c
        signal.c
        signal_operators.c
        signal_player.c
        signal_filters.c
        queue.c
    )
    add_executable(lhc ${SRCS})
    target_link_libraries(lhc ${LIBS})
else (ALL_FOUND)
    message (SEND_ERROR "stopping.")
endif(ALL_FOUND)
