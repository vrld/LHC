cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -ansi -Wno-unused-parameter -Wno-unused-variable -lm")

project(LHC)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}")

set(ALL_FOUND TRUE)

# openAL
find_package(OpenAL)
find_library(ALUT_LIBRARY alut)
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

macro(add_subdir var dir)
    foreach (file ${ARGN})
        set (${var} ${${var}} ${dir}/${file})
    endforeach (file)
    source_group(${dir} FILES ${var})
endmacro(add_subdir)

if (ALL_FOUND)
    add_subdir(SRCS src
        lhc.c
        generators.c
        signal.c
    )
    add_executable(lhc ${SRCS})
    target_link_libraries(lhc ${LIBS})
else (ALL_FOUND)
    message (error "cannot find all required libraries!")
endif(ALL_FOUND)
