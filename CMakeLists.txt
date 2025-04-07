# SPDX-License-Identifier: MIT
# Copyright (c) iftlg developers

cmake_minimum_required(VERSION 3.12)
project(iftlg)

if((NOT DEFINED CMAKE_BUILD_TYPE) OR (NOT CMAKE_BUILD_TYPE) OR (CMAKE_BUILD_TYPE STREQUAL ""))
set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

if((NOT DEFINED MY_LIBRARY_PROCESSOR) OR (NOT MY_LIBRARY_PROCESSOR) OR (MY_LIBRARY_PROCESSOR STREQUAL ""))
set(MY_LIBRARY_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686")
set(MY_LIBRARY_PROCESSOR intel32)
endif()
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
set(MY_LIBRARY_PROCESSOR intel64)
endif()
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm")
set(MY_LIBRARY_PROCESSOR arm32)
endif()
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "arm64")
set(MY_LIBRARY_PROCESSOR arm64)
endif()
endif()

if((NOT DEFINED CUR_GIT_TAG) OR (NOT CUR_GIT_TAG) OR (CUR_GIT_TAG STREQUAL ""))
	set(CUR_GIT_TAG Unknown)
	find_package(Git)
	if(GIT_FOUND)
		if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
			execute_process(COMMAND ${GIT_EXECUTABLE} describe --abbrev=0 --tags
			WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
			OUTPUT_VARIABLE TMP_GIT_TAG
			RESULT_VARIABLE TMP_RES
			OUTPUT_STRIP_TRAILING_WHITESPACE)
			if(TMP_RES EQUAL "0")
				set(CUR_GIT_TAG ${TMP_GIT_TAG})
			endif()
		endif()
	endif()
endif()

set(MY_LIBRARY_RC_COMMENTS "Source code for the latest version of this product is located on the World Wide Web at https://github.com/uyjulian/iftlg")
set(MY_LIBRARY_RC_FILEDESCRIPTION "TLG Plugin for Susie Image Viewer")
set(MY_LIBRARY_RC_FILEVERSION "${CUR_GIT_TAG}")
set(MY_LIBRARY_RC_INTERNALNAME "iftlg")
set(MY_LIBRARY_RC_LEGALCOPYRIGHT "Copyright (C) iftlg developers; This product is licensed under the MIT license.")
set(MY_LIBRARY_RC_ORIGINALFILENAME "iftlg.spi")
set(MY_LIBRARY_RC_PRODUCTNAME "TLG Plugin for Susie Image Viewer")
set(MY_LIBRARY_RC_PRODUCTVERSION "${CUR_GIT_TAG}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dllres.rc.cmakein ${CMAKE_CURRENT_BINARY_DIR}/dllres.rc)
set(MY_LIBRARY_NAME ${PROJECT_NAME})
list(APPEND MY_LIBRARY_DEFINITIONS
	-DUNICODE
	-D_UNICODE
	-DNDEBUG
	-DWIN32
	-D_WIN32
	-D_WINDOWS 
)
list(APPEND MY_LIBRARY_CFLAGS
	-Wno-unused-value
	-Wno-format
	-flto
)
list(APPEND MY_LIBRARY_CXXFLAGS
	${MY_LIBRARY_CFLAGS}
	-fpermissive
)
list(APPEND MY_LIBRARY_LDFLAGS
	-static
	-static-libstdc++
	-static-libgcc
	-municode
	-Wl,--subsystem,windows
	-Wl,--kill-at
	-fPIC
)
list(APPEND MY_LIBRARY_LIBS
)
list(APPEND LIBTLG_SRC
	external/libtlg/LoadTLG.cpp
	external/libtlg/handlestream.cpp
	external/libtlg/memstream.cpp
	external/libtlg/slide.cpp
	external/libtlg/stream.cpp
	external/libtlg/tvpgl.c
)
list(APPEND MY_LIBRARY_SRC
	extractor.cpp
	spi00in.c
	${CMAKE_CURRENT_BINARY_DIR}/dllres.rc
	${LIBTLG_SRC}
)
list(APPEND MY_LIBRARY_INC
	${CMAKE_CURRENT_SOURCE_DIR}
	external/libtlg
)

add_library(${MY_LIBRARY_NAME} MODULE)
if(NOT ((${CMAKE_SYSTEM_PROCESSOR} STREQUAL "i686") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")))
set_target_properties(${MY_LIBRARY_NAME} PROPERTIES OUTPUT_NAME ${MY_LIBRARY_NAME}_${MY_LIBRARY_PROCESSOR})
endif()
set_target_properties(${MY_LIBRARY_NAME} PROPERTIES PREFIX "")
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "amd64")
set_target_properties(${MY_LIBRARY_NAME} PROPERTIES SUFFIX ".sph")
else()
set_target_properties(${MY_LIBRARY_NAME} PROPERTIES SUFFIX ".spi")
endif()
set_target_properties(${MY_LIBRARY_NAME} PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_sources(${MY_LIBRARY_NAME} PRIVATE ${MY_LIBRARY_SRC})
target_compile_options(${MY_LIBRARY_NAME} PUBLIC $<$<COMPILE_LANGUAGE:C>:${MY_LIBRARY_CFLAGS}>)
target_compile_options(${MY_LIBRARY_NAME} PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${MY_LIBRARY_CXXFLAGS}>)
target_compile_definitions(${MY_LIBRARY_NAME} PUBLIC ${MY_LIBRARY_DEFINITIONS})
target_include_directories(${MY_LIBRARY_NAME} PUBLIC ${MY_LIBRARY_INC})
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13.0")
target_link_options(${MY_LIBRARY_NAME} PUBLIC ${MY_LIBRARY_LDFLAGS})
endif()
target_link_libraries(${MY_LIBRARY_NAME} ${MY_LIBRARY_LIBS})

if((NOT DEFINED MY_ARCHIVE_NAME) OR (NOT MY_ARCHIVE_NAME) OR (MY_ARCHIVE_NAME STREQUAL ""))
if((NOT DEFINED USE_ARCHIVE_HAS_GIT_TAG) OR (NOT USE_ARCHIVE_HAS_GIT_TAG) OR (USE_ARCHIVE_HAS_GIT_TAG STREQUAL ""))
set(USE_ARCHIVE_HAS_GIT_TAG FALSE)
endif()
if(USE_ARCHIVE_HAS_GIT_TAG)
set(MY_ARCHIVE_NAME ${MY_LIBRARY_NAME}.${MY_LIBRARY_PROCESSOR}.${CUR_GIT_TAG}.7z)
else()
set(MY_ARCHIVE_NAME ${MY_LIBRARY_NAME}.${MY_LIBRARY_PROCESSOR}.7z)
endif()
endif()
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MY_ARCHIVE_NAME}
	COMMAND rm -f ${CMAKE_CURRENT_BINARY_DIR}/${MY_ARCHIVE_NAME} && 7z a ${CMAKE_CURRENT_BINARY_DIR}/${MY_ARCHIVE_NAME} $<TARGET_FILE:${MY_LIBRARY_NAME}>
	DEPENDS ${MY_LIBRARY_NAME}
)
add_custom_target(archive
	DEPENDS ${MY_ARCHIVE_NAME}
)
