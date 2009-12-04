cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

project(LHC)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}")

if (CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -ansi -lm -pthread")
endif (CMAKE_COMPILER_IS_GNUCC)

set(ALL_FOUND TRUE)

option(WITH_GNU_READLINE "Use GNU readline for input" ON)
set(SAMPLE_CHUNK_LENGTH 16384 CACHE STRING "Length of a sample chunk in byte")
set(SAMPLE_BUFFER_COUNT 2 CACHE STRING "Number of buffers for playing signals")

# openAL
find_package(OpenAL)
find_library(ALUT_LIBRARY NAMES alut)
find_path(ALUT_INCLUDE_DIR AL/alut.h)
set(ALUT_FOUND FALSE)
if (ALUT_LIBRARY AND ALUT_INCLUDE_DIR)
    set(ALUT_FOUND TRUE)
endif (ALUT_LIBRARY AND ALUT_INCLUDE_DIR)

if (ALUT_FOUND AND OPENAL_FOUND)
    include_directories(${OPENAL_INCLUDE_DIRS})
    include_directories(${ALUT_INCLUDE_DIR})
    set(LIBS ${LIBS} ${OPENAL_LIBRARY} ${ALUT_LIBRARY})
else (ALUT_FOUND AND OPENAL_FOUND)
    set(ALL_FOUND FALSE)
endif (ALUT_FOUND AND OPENAL_FOUND)

# lua
find_package(Lua51)
if (LUA51_FOUND)
    include_directories(${LUA_INCLUDE_DIR})
    set(LIBS ${LIBS} ${LUA_LIBRARIES})
else (LUA51_FOUND)
    set(ALL_FOUND FALSE)
endif (LUA51_FOUND)

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
        signal.c
        queue.c
    )
    add_executable(lhc ${SRCS})
    target_link_libraries(lhc ${LIBS})
else (ALL_FOUND)
    message (error "cannot find all required libraries!")
endif(ALL_FOUND)
