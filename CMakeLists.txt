project (bitfighter)
cmake_minimum_required (VERSION 3.1.0)

# CMake 3.0 policy that says you need to do something like this (NEW):
#    set_target_properties(master_lib master PROPERTIES COMPILE_DEFINITIONS_DEBUG "TNL_DEBUG")
#
# instead of this (OLD):
#    set_target_properties(master_lib master PROPERTIES COMPILE_DEFINITIONS $<$<CONFIG:Debug>:TNL_DEBUG>)
#
# Set to OLD behavior until minimum cmake version >= 2.8.10
if(POLICY CMP0043)
	cmake_policy(SET CMP0043 OLD)
endif()


#
# CMake options
#

option(USE_GLES2 "Force usage of OpenGL ES 2 for the bitfighter client.  Requires SDL2." NO)
option(USE_LUAJIT_IN_TREE "Force usage of in-tree LuaJIT." NO)
option(LUAJIT_DISABLE_JIT "Disable JIT compiler in LuaJIT." NO)
option(MASTER_MINIMAL "Build master server without MySQL (only SQLite)." NO)
option(ALURE_DISABLE_MP3 "Disable dynamic loading of libmpg123.  Disables mp3 completely." NO)
option(NO_THREADS "Disable usage of threads in TNL.  May cause issues." NO)
option(NO_SCREENSHOTS "Disable in-game screenhots." NO)
option(NO_STATS "Disable local stats saving with SQlite.")
option(NO_VOICECHAT "Disable in-game voicechat." NO)
option(NO_MUSIC "Disable in-game music." NO)
option(NO_AUDIO "Disable all in-game sound." NO)

# NO_AUDIO implies disabling music and voicechat, too
if(NO_AUDIO)
	message(STATUS "HEREHEREHRE")
	set(NO_VOICECHAT YES CACHE BOOL "Disable in-game voicechat." FORCE)
	set(NO_MUSIC YES CACHE BOOL "Disable in-game music." FORCE)
endif()


#
# CMake system management
#

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

# Require use c++11 compiler
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Don't use compiler extensions like -std=gnu++11
# set(CMAKE_CXX_EXTENSIONS OFF)

# Use appropriate platform alterations
if(WIN32)
	include(${CMAKE_SOURCE_DIR}/cmake/Platform/Win32.cmake)
elseif(APPLE)
	include(${CMAKE_SOURCE_DIR}/cmake/Platform/Apple.cmake)
elseif(UNIX)
	include(${CMAKE_SOURCE_DIR}/cmake/Platform/Linux.cmake)
endif()


# No creating project files in-source.  Bad, evil things happen
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" _insource)
if (_insource)
	message(SEND_ERROR "No building in-source.  You must run CMake from the \"build\" directory.")
	message(FATAL_ERROR "Remove the file CMakeCache.txt in ${CMAKE_SOURCE_DIR} first.")
endif()


# Default to Release for a build type
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()


#
# Library searching and dependencies
#

# SDL2 is default and should be found on all platforms except possibly Linux
find_package(SDL2)

if(SDL2_FOUND)
	set(SDL_INCLUDE_DIR ${SDL2_INCLUDE_DIR})
	set(SDL_LIBRARY ${SDL2_LIBRARY})
endif()


# Searches for OpenGL and enables GLES if GL isnt found in the system
message(STATUS "Determining system GL provider")
if(NOT USE_GLES2)
	message(STATUS "Atempting to use standard OpenGL")
	find_package(OpenGL)
	set(USE_STANDARD_GL 1)
	set(GL_LIBRARY ${OPENGL_LIBRARY})
	set(GL_INCLUDE_DIR ${OPENGL_INCLUDE_DIR})
endif()

if(USE_GLES2)
	message(STATUS "Atempting to use OpenGL ES 2")
	add_definitions(-DBF_USE_GLES2)
	add_definitions(-DBF_NO_CONSOLE)
	find_package(OpenGLES2)
	set(GL_LIBRARY ${OPENGLES2_LIBRARY})
	set(GL_INCLUDE_DIR ${OPENGLES2_INCLUDE_DIR})
endif()


# Other needed libraries that don't have in-tree fallback options
if(NOT NO_THREADS)
	find_package(Threads REQUIRED)
endif()
find_package(PhysFS REQUIRED)
if(NO_SCREENSHOTS)
	add_definitions(-DBF_NO_SCREENSHOTS)
else()
	find_package(PNG)
endif()
find_package(MySQL)

if(NOT NO_AUDIO)
	find_package(OpenAL)
endif()

if(NOT NO_VOICECHAT)
	find_package(OGG)
	find_package(Speex)
else()
	add_definitions(-DBF_NO_VOICECHAT)
endif()

if(NOT NO_MUSIC)
	find_package(OGG)
	find_package(Vorbis)
	find_package(ModPlug)
else()
	add_definitions(-DBF_NO_MUSIC)
endif()


# Now look for libraries that have an in-tree fallback option
# Many of these may have special overrides to properly adapt to the in-tree versions

# Special function to look for a system lib, but fall back to the in-tree version
function(FIND_WITH_FALLBACK packageName capsName dirName)
	if(NOT FORCE_IN_TREE_${capsName})
		find_package(${packageName})
	endif()

	if( NOT ${capsName}_FOUND )
		set(${capsName}_LIBRARIES ${dirName})
		set(${capsName}_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/${dirName}")
	endif()

	# export the results
	set(${capsName}_FOUND "${${capsName}_FOUND}" PARENT_SCOPE)
	set(${capsName}_INCLUDE_DIR "${${capsName}_INCLUDE_DIR}" PARENT_SCOPE)
	set(${capsName}_LIBRARY "${${capsName}_LIBRARY}" PARENT_SCOPE)
	set(${capsName}_LIBRARIES "${${capsName}_LIBRARIES}" PARENT_SCOPE)
endfunction()


# ALURE
if(NOT NO_AUDIO)
	find_with_fallback(ALURE ALURE alure)
	if(NOT ALURE_FOUND)
		if(NOT OPENAL_FOUND)
			message(WARNING "Audio system components are missing.  Bitfighter will be compiled without audio")
			add_definitions(-DBF_NO_AUDIO)
			set(OPENAL_INCLUDE_DIR "")
			set(ALURE_LIB "")
		else()
			message(STATUS "Using in-tree ALURE")
			add_definitions(-DALURE_STATIC_LIBRARY)
			add_subdirectory(alure)
			set(ALURE_LIB alure)
		endif()
	endif()
else()
	add_definitions(-DBF_NO_AUDIO)
	set(ALURE_LIB "")
endif()


if(NOT NO_STATS)
	# sqlite3
	find_with_fallback(Sqlite SQLITE3 sqlite)
	if(NOT SQLITE3_FOUND)
		message(STATUS "Using in-tree sqlite3")
		add_subdirectory(sqlite)
	endif()
else()
	add_definitions(-DBF_NO_STATS)
endif()


# libtomcrypt
find_with_fallback(TomCrypt TOMCRYPT tomcrypt)
if(NOT TOMCRYPT_FOUND)
	message(STATUS "Using in-tree libtomcrypt")
	set(TOMCRYPT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/tomcrypt/src/headers")
	add_subdirectory(tomcrypt)
endif()


# Boost
find_with_fallback(Boost Boost boost)
if(NOT Boost_FOUND)
	message(STATUS "Using in-tree Boost c++ headers")
	set(BOOST_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/boost")
endif()


# Clipper
find_with_fallback(Clipper CLIPPER clipper)
if(NOT CLIPPER_FOUND)
	message(STATUS "Using in-tree Clipper")
	add_subdirectory(clipper)
endif()


# poly2tri
find_with_fallback(Poly2tri POLY2TRI poly2tri)
if(NOT POLY2TRI_FOUND)
	message(STATUS "Using in-tree poly2tri")
	add_subdirectory(poly2tri)
endif()


# LuaJIT / Lua
find_with_fallback(LuaJit LUAJIT lua)
if(LUAJIT_FOUND AND NOT USE_LUAJIT_IN_TREE)
	set(LUA_LIB ${LUAJIT_LIBRARIES})
	set(LUA_INCLUDE_DIR ${LUAJIT_INCLUDE_DIR})
else()
	message(STATUS "Using in-tree LuaJIT")
	# Use internal LuaJIT
	set(LUA_LIB luajit)
	set(LUA_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/lua/luajit/src")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/lua/${LUA_LIB}")
	add_subdirectory("${CMAKE_SOURCE_DIR}/lua/${LUA_LIB}")
endif()


# mysql++ is for compiling master
if(EXISTS "${CMAKE_SOURCE_DIR}/mysql++")
	add_subdirectory(mysql++)
endif()


# gtest is for compiling the bitfighter_test suite
if(EXISTS "${CMAKE_SOURCE_DIR}/gtest")
	add_subdirectory(gtest EXCLUDE_FROM_ALL)
endif()


# Other internal-only sub-projects
add_subdirectory(tnl)
add_subdirectory(tnlping)
add_subdirectory(master)
add_subdirectory(updater)  # Windows-only


# Our main sources!
add_subdirectory(zap)
