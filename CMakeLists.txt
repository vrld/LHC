cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}")

project(LHC)
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}")

if (CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -ansi -lm -pthread")
endif (CMAKE_COMPILER_IS_GNUCC)

set(ALL_FOUND TRUE)

option(WITH_GNU_READLINE "Use GNU readline for input" ON)

# PortAudio
find_library(PA_LIBRARY NAMES portaudio)
find_path(PA_INCLUDE_DIR portaudio.h)
set(PA_FOUND FALSE)
if (PA_LIBRARY AND PA_INCLUDE_DIR)
	include_directories(${PA_INCLUDE_DIR})
	set(LIBS ${LIBS} ${OPENAL_LIBRARY} ${PA_LIBRARY})
else (PA_LIBRARY AND PA_INCLUDE_DIR)
	message("Cannot find PortAudio")
	set(ALL_FOUND FALSE)
endif (PA_LIBRARY AND PA_INCLUDE_DIR)

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
	include_directories(${FFTW3_INCLUDE_DIR})
	set(LIBS ${LIBS} ${FFTW3_LIBRARY})
else (FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)
	message("Cannot find FFTW3")
	set(ALL_FOUND FALSE)
endif (FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)

# libsndfile
find_path(SNDFILE_INCLUDE_DIR sndfile.h)
find_library(SNDFILE_LIBRARY NAMES sndfile)

if (SNDFILE_INCLUDE_DIR AND SNDFILE_LIBRARY)
	include_directories(${SNDFILE_INCLUDE_DIR})
	set(LIBS ${LIBS} ${SNDFILE_LIBRARY})
else (SNDFILE_INCLUDE_DIR AND SNDFILE_LIBRARY)
	message("Cannot find libsoundfile")
	set(ALL_FOUND FALSE)
endif (SNDFILE_INCLUDE_DIR AND SNDFILE_LIBRARY)


# readline
if (WITH_GNU_READLINE)
	find_path(READLINE_INCLUDE_DIR readline/readline.h)
	find_library(READLINE_LIBRARY NAMES readline) 

	if (READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
		include_directories(${READLINE_INCLUDE_DIR})
		set(LIBS ${LIBS} ${READLINE_LIBRARY})
	else (READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
		message("Cannot find GNU Readline")
		set(ALL_FOUND FALSE)
	endif (READLINE_INCLUDE_DIR AND READLINE_LIBRARY)
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
		sounddata.c
		soundfile.c
		player.c
		)
	add_executable(lhc ${SRCS})
	target_link_libraries(lhc ${LIBS})
else (ALL_FOUND)
	message (SEND_ERROR "stopping.")
endif(ALL_FOUND)
