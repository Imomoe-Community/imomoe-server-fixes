# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(imomoe_server_fixes LANGUAGES CXX)

option(SERVER_FIXES_POLICY_TESTS_ONLY "Build only SDK-independent policy tests" OFF)
option(SERVER_FIXES_CALLBACK_TESTS "Build focused tests against production callbacks and matching S2SDK imports" OFF)

function(server_fixes_apply_paired_sourcesdk target)
	get_filename_component(source_sdk_root "${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}" DIRECTORY)
	get_filename_component(paired_s2sdk_source_root "${source_sdk_root}/../.." ABSOLUTE)
	if(NOT IS_DIRECTORY "${SERVER_FIXES_SOURCE_SDK_BUILD_DIR}")
		message(FATAL_ERROR "SERVER_FIXES_SOURCE_SDK_BUILD_DIR must point to the configured build of the matching S2SDK source tree")
	endif()
	set(source_sdk_cache "${SERVER_FIXES_SOURCE_SDK_BUILD_DIR}/CMakeCache.txt")
	if(NOT EXISTS "${source_sdk_cache}")
		message(FATAL_ERROR "SERVER_FIXES_SOURCE_SDK_BUILD_DIR must contain CMakeCache.txt from the paired SDK build")
	endif()
	file(STRINGS "${source_sdk_cache}" source_sdk_cache_entries REGEX "^sourcesdk_SOURCE_DIR:STATIC=")
	if(NOT source_sdk_cache_entries)
		message(FATAL_ERROR "The selected SDK build cache does not identify its SourceSDK source directory")
	endif()
	string(REGEX REPLACE "^sourcesdk_SOURCE_DIR:STATIC=" "" source_sdk_cached_root "${source_sdk_cache_entries}")
	file(REAL_PATH "${source_sdk_cached_root}" configured_source_sdk_root)
	file(REAL_PATH "${source_sdk_root}" expected_source_sdk_root)
	if(NOT configured_source_sdk_root STREQUAL expected_source_sdk_root)
		message(FATAL_ERROR "SERVER_FIXES_SOURCE_SDK_BUILD_DIR was not configured from the supplied matching SourceSDK source tree")
	endif()
	file(REAL_PATH "${SERVER_FIXES_SOURCE_SDK_BUILD_DIR}" configured_sdk_build_dir)
	file(REAL_PATH "${SERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR}" configured_proto_dir)
	file(REAL_PATH "${configured_sdk_build_dir}/external/sourcesdk/protos" expected_proto_dir)
	if(NOT configured_proto_dir STREQUAL expected_proto_dir)
		message(FATAL_ERROR "SERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR must be external/sourcesdk/protos from SERVER_FIXES_SOURCE_SDK_BUILD_DIR")
	endif()
	if(NOT IS_DIRECTORY "${source_sdk_root}/game/shared")
		message(FATAL_ERROR "SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR must belong to the complete matching SourceSDK source tree")
	endif()
	set(source_sdk_include_dirs
		"${SERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR}"
		"${paired_s2sdk_source_root}/external/parallel-hashmap"
		"${source_sdk_root}/thirdparty/protobuf/src"
		"${source_sdk_root}/game/server"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/game/server"
		"${source_sdk_root}/game/shared"
		"${source_sdk_root}/common"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/engine"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/entity2"
		"${source_sdk_root}/interfaces"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/mathlib"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/networksystem"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/steam"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/tier0"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/tier1"
		"${SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR}/kv3lib"
	)
	foreach(include_dir IN LISTS source_sdk_include_dirs)
		if(NOT IS_DIRECTORY "${include_dir}")
			message(FATAL_ERROR "The matched SourceSDK include directory is missing: ${include_dir}")
		endif()
	endforeach()
	find_library(SERVER_FIXES_TIER0_PERPROJECT_LIBRARY NAMES tier0_perproject
		HINTS
			"${SERVER_FIXES_SOURCE_SDK_BUILD_DIR}/external/sourcesdk"
			"${SERVER_FIXES_SOURCE_SDK_BUILD_DIR}/external/sourcesdk/Release"
		NO_DEFAULT_PATH)
	if(NOT SERVER_FIXES_TIER0_PERPROJECT_LIBRARY)
		message(FATAL_ERROR "The matching SourceSDK build must contain its static tier0_perproject library for CCommand")
	endif()
	target_include_directories(${target} SYSTEM PRIVATE ${source_sdk_include_dirs})
	if(MSVC)
		set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
		target_compile_definitions(${target} PRIVATE
			PLATFORM_64BITS X64BITS WIN32 _WIN32 _WINDOWS
			_ALLOW_KEYWORD_MACROS __STDC_LIMIT_MACROS
			_CRT_SECURE_NO_WARNINGS=1 _CRT_SECURE_NO_DEPRECATE=1 _CRT_NONSTDC_NO_DEPRECATE=1 _CRT_NOFORCE_MANIFEST=1
			_SHARED_LIB COMPILER_MSVC COMPILER_MSVC64 CORE
			"SE_NAME=\"Counter-Strike 2\"" "SE_GAME_DIR=\"csgo\""
			CSGO=23 CS2=25 _LIB TIER1_STATIC_LIB
		)
	else()
		target_compile_definitions(${target} PRIVATE
			PLATFORM_64BITS X64BITS POSIX _LINUX LINUX _SHARED_LIB CORE
			"SE_NAME=\"Counter-Strike 2\"" "SE_GAME_DIR=\"csgo\""
			CSGO=23 CS2=25 _LIB TIER1_STATIC_LIB
			bswap_16=__bswap_16 bswap_32=__bswap_32 bswap_64=__bswap_64
			_GLIBCXX_USE_CXX11_ABI=0
		)
		target_link_options(${target} PRIVATE -static-libstdc++ -static-libgcc)
	endif()
	target_link_libraries(${target} PRIVATE "${SERVER_FIXES_TIER0_PERPROJECT_LIBRARY}" "${SERVER_FIXES_TIER0_LIBRARY}")
endfunction()

function(server_fixes_add_policy_test target test_name source_file)
	add_executable(${target} "${source_file}")
	target_include_directories(${target} PRIVATE src)
	target_compile_features(${target} PRIVATE cxx_std_20)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /WX)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wconversion -Wshadow -Werror)
	endif()
	add_test(NAME ${test_name} COMMAND ${target})
endfunction()

if(SERVER_FIXES_POLICY_TESTS_ONLY)
	include(CTest)
	server_fixes_add_policy_test(server_fixes_chat_policy_test server_fixes_chat_policy tests/chat_policy_test.cpp)
	server_fixes_add_policy_test(server_fixes_chat_listener_test server_fixes_chat_listener tests/chat_listener_test.cpp)
	server_fixes_add_policy_test(server_fixes_hostage_policy_test server_fixes_hostage_policy tests/hostage_policy_test.cpp)
	server_fixes_add_policy_test(server_fixes_spawn_capacity_policy_test server_fixes_spawn_capacity_policy tests/spawn_capacity_policy_test.cpp)
	server_fixes_add_policy_test(server_fixes_spectator_compat_policy_test server_fixes_spectator_compat_policy tests/spectator_compat_policy_test.cpp)
	if(SERVER_FIXES_CALLBACK_TESTS)
		foreach(required_var SERVER_FIXES_PLUGIFY_INCLUDE_DIR SERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR
			SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR SERVER_FIXES_SOURCE_SDK_BUILD_DIR SERVER_FIXES_SOURCE_SDK_LIBRARY_DIR
			SERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR)
			if(NOT DEFINED ${required_var} OR NOT IS_DIRECTORY "${${required_var}}")
				message(FATAL_ERROR "${required_var} must point to its matching SDK include or library directory for callback tests")
			endif()
		endforeach()
		find_library(SERVER_FIXES_TIER0_LIBRARY NAMES tier0
			HINTS "${SERVER_FIXES_SOURCE_SDK_LIBRARY_DIR}" NO_DEFAULT_PATH)
		if(NOT SERVER_FIXES_TIER0_LIBRARY)
			message(FATAL_ERROR "The matching SourceSDK library directory must contain tier0.lib or libtier0.so for CCommand")
		endif()
		set(callback_test_include "${CMAKE_CURRENT_BINARY_DIR}/callback-test-include")
		file(MAKE_DIRECTORY "${callback_test_include}")
		file(WRITE "${callback_test_include}/plugin_export.h" "#pragma once\n#define PLUGIN_API\n")

		function(server_fixes_add_callback_test target test_name)
			add_executable(${target} ${ARGN}
				"${SERVER_FIXES_PLUGIFY_INCLUDE_DIR}/polyhook/polyhook.cpp"
				"${SERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR}/s2sdk/s2sdk.cpp"
			)
			target_include_directories(${target} PRIVATE src "${callback_test_include}"
				"${SERVER_FIXES_PLUGIFY_INCLUDE_DIR}" "${SERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR}")
			server_fixes_apply_paired_sourcesdk(${target})
			target_compile_features(${target} PRIVATE cxx_std_23)
			if(MSVC)
				target_compile_options(${target} PRIVATE /W4 /WX /EHsc /Zc:preprocessor)
			else()
				target_compile_options(${target} PRIVATE -Wall -Wextra -Wconversion -Wshadow -Werror)
			endif()
			add_test(NAME ${test_name} COMMAND ${target})
		endfunction()

		server_fixes_add_callback_test(server_fixes_spawn_capacity_callback_test server_fixes_spawn_capacity_callbacks
			tests/spawn_capacity_callback_test.cpp src/hostage_compat.cpp src/runtime_lifecycle.cpp)
		server_fixes_add_callback_test(server_fixes_game_ban_fix_callback_test server_fixes_game_ban_fix_callbacks tests/game_ban_fix_callback_test.cpp)
	endif()
	return()
endif()

foreach(required_var SERVER_FIXES_PLUGIFY_INCLUDE_DIR SERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR
	SERVER_FIXES_SOURCE_SDK_PUBLIC_INCLUDE_DIR SERVER_FIXES_SOURCE_SDK_BUILD_DIR SERVER_FIXES_SOURCE_SDK_LIBRARY_DIR
	SERVER_FIXES_SOURCE_SDK_PROTO_INCLUDE_DIR)
	if(NOT DEFINED ${required_var} OR NOT IS_DIRECTORY "${${required_var}}")
		message(FATAL_ERROR "${required_var} must point to the matching generated SDK include or library directory")
	endif()
endforeach()
find_library(SERVER_FIXES_TIER0_LIBRARY NAMES tier0
	HINTS "${SERVER_FIXES_SOURCE_SDK_LIBRARY_DIR}" NO_DEFAULT_PATH)
if(NOT SERVER_FIXES_TIER0_LIBRARY)
	message(FATAL_ERROR "The matching SourceSDK library directory must contain tier0.lib or libtier0.so for CCommand")
endif()

add_library(imomoe-server-fixes SHARED
	src/plugin.cpp
	src/game_ban_fix.cpp
	src/runtime_lifecycle.cpp
	src/hostage_compat.cpp
	src/spawn_capacity.cpp
	src/spectator_compat.cpp
	"${SERVER_FIXES_PLUGIFY_INCLUDE_DIR}/polyhook/polyhook.cpp"
	"${SERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR}/s2sdk/s2sdk.cpp"
)
target_include_directories(imomoe-server-fixes PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/src"
	"${CMAKE_CURRENT_BINARY_DIR}/exports"
	"${SERVER_FIXES_PLUGIFY_INCLUDE_DIR}"
	"${SERVER_FIXES_S2SDK_GENERATED_INCLUDE_DIR}"
)
server_fixes_apply_paired_sourcesdk(imomoe-server-fixes)
target_compile_features(imomoe-server-fixes PRIVATE cxx_std_23)
set_target_properties(imomoe-server-fixes PROPERTIES
	OUTPUT_NAME "imomoe-server-fixes"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
	set_target_properties(imomoe-server-fixes PROPERTIES
		LIBRARY_OUTPUT_DIRECTORY_${config} "${CMAKE_CURRENT_BINARY_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_CURRENT_BINARY_DIR}/bin"
	)
endforeach()
target_compile_definitions(imomoe-server-fixes PRIVATE
	PLUGIFY_IS_DEBUG=$<CONFIG:Debug>
	PLUGIFY_IS_RELEASE=$<CONFIG:Release>
	PLUGIFY_ARCH_BITS=$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,64,32>
	PLUGIFY_MODULE="imomoe_server_fixes"
)
include(GenerateExportHeader)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/exports")
generate_export_header(imomoe-server-fixes
	EXPORT_MACRO_NAME PLUGIN_API
	EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/exports/plugin_export.h"
)
if(MSVC)
	target_compile_options(imomoe-server-fixes PRIVATE /W4 /WX /EHsc /Zc:preprocessor)
else()
	target_compile_options(imomoe-server-fixes PRIVATE -Wall -Wextra -Wconversion -Wshadow -Werror)
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gamedata")
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/gamedata/imomoe_server_fixes.jsonc"
	"${CMAKE_CURRENT_BINARY_DIR}/gamedata/imomoe_server_fixes.jsonc"
	COPYONLY
)
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/imomoe_server_fixes.pplugin"
	"${CMAKE_CURRENT_BINARY_DIR}/imomoe_server_fixes.pplugin"
	COPYONLY
)
