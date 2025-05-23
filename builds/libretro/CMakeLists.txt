cmake_minimum_required(VERSION 3.16...3.31 FATAL_ERROR)

project(retro_common VERSION 1.0 LANGUAGES C)

# Check availability of libretro-common submodule
file(GLOB RETRO_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/*)
list(LENGTH RETRO_COMMON_DIR RETRO_COMMON_DIR_LEN)
if(RETRO_COMMON_DIR_LEN EQUAL 0)
	message(FATAL_ERROR "libretro-common is empty. Run \"git submodule update --init\" and try again.")
endif()

add_library(retro_common STATIC
	libretro-common/rthreads/rthreads.c
)

target_include_directories(retro_common PUBLIC
	libretro-common/include)

find_library(PTHREAD_LIBRARY pthread)
if(PTHREAD_LIBRARY)
	target_link_libraries(retro_common "${PTHREAD_LIBRARY}")
endif()

if(BUILD_SHARED_LIBS)
	set_property(TARGET retro_common PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

mark_as_advanced(RETRO_COMMON_DIR RETRO_COMMON_DIR_LEN)
