cmake_minimum_required( VERSION 3.11 FATAL_ERROR )

project( "VK_LAYER_AZ_sync_analyzer" LANGUAGES C CXX )

message( STATUS "==========================================================================\n" )
message( STATUS "project '${PROJECT_NAME}' generation started" )

set_property( GLOBAL PROPERTY USE_FOLDERS ON )
message( STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} (${CMAKE_CXX_COMPILER_VERSION})" )
message( STATUS "target system: ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_VERSION})" )
message( STATUS "host system: ${CMAKE_HOST_SYSTEM_NAME} (${CMAKE_HOST_SYSTEM_VERSION})" )

include( FetchContent )
#set( FETCHCONTENT_FULLY_DISCONNECTED ON CACHE BOOL "don't download externals" )
set( FETCHCONTENT_UPDATES_DISCONNECTED ON CACHE BOOL "don't update externals" )
mark_as_advanced( FETCHCONTENT_BASE_DIR FETCHCONTENT_FULLY_DISCONNECTED )
mark_as_advanced( FETCHCONTENT_QUIET FETCHCONTENT_UPDATES_DISCONNECTED )

set( CMAKE_DEBUG_POSTFIX "" )
set( CMAKE_RELEASE_POSTFIX "" )
set( CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "installation directory" )
set( MAIN_BINARY_DIR "${CMAKE_BINARY_DIR}/bin" CACHE INTERNAL "" FORCE )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MAIN_BINARY_DIR}" )

if (TRUE)
	include( "cmake/compilers.cmake" )
else()
	include( "cmake/compilers_minimal.cmake" )
endif()

#==============================================================================
# Graphviz

set( VSA_ENABLE_GRAPHVIZ ON CACHE BOOL "use graphviz (optional)" )
include( "cmake/graphviz.cmake" )

#==============================================================================
# Vulkan

FetchContent_Declare( ExternalVulkanHeaders
	GIT_REPOSITORY		https://github.com/KhronosGroup/Vulkan-Headers.git
	GIT_TAG				"v1.1.108"
	SOURCE_DIR			"${CMAKE_CURRENT_SOURCE_DIR}/external/Vulkan-Headers"
)
	
FetchContent_GetProperties( ExternalVulkanHeaders )
if (NOT ExternalVulkanHeaders_POPULATED)
	message( STATUS "downloading Vulkan-Headers" )
	FetchContent_Populate( ExternalVulkanHeaders )
endif ()
	
add_library( "Vulkan-lib" INTERFACE )
add_dependencies( "Vulkan-lib" ExternalVulkanHeaders )
target_include_directories( "Vulkan-lib" INTERFACE "external/Vulkan-Headers/include" )

if (WIN32)
	target_compile_definitions( "Vulkan-lib" INTERFACE "VK_USE_PLATFORM_WIN32_KHR=1" )
endif ()
target_compile_definitions( "Vulkan-lib" INTERFACE "VK_NO_PROTOTYPES=1" )

#==============================================================================
# Sync analyzer

file( GLOB_RECURSE SOURCES "src/*.*" )
file( GLOB_RECURSE STL_SOURCES "stl/*.*" )
add_library( ${PROJECT_NAME} SHARED ${SOURCES} ${STL_SOURCES} )
set_property( TARGET ${PROJECT_NAME} PROPERTY FOLDER "" )
source_group( TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SOURCES} ${STL_SOURCES} )
target_link_libraries( ${PROJECT_NAME} "Vulkan-lib" )
target_include_directories( ${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )
target_compile_definitions( ${PROJECT_NAME} PUBLIC "VSA_LAYER_NAME=\"${PROJECT_NAME}\"" )

# Debug
if (PROJECTS_SHARED_CXX_FLAGS_DEBUG)
	target_compile_options( ${PROJECT_NAME} PUBLIC $<$<CONFIG:Debug>: ${PROJECTS_SHARED_CXX_FLAGS_DEBUG}> )
endif()
if (PROJECTS_SHARED_DEFINES_DEBUG)
	target_compile_definitions( ${PROJECT_NAME} PUBLIC $<$<CONFIG:Debug>: ${PROJECTS_SHARED_DEFINES_DEBUG}> )
endif()
if (PROJECTS_SHARED_LINKER_FLAGS_DEBUG)
	set_target_properties( ${PROJECT_NAME} PROPERTIES LINK_FLAGS_DEBUG ${PROJECTS_SHARED_LINKER_FLAGS_DEBUG} )
endif()

# Release
if (PROJECTS_SHARED_CXX_FLAGS_RELEASE)
	target_compile_options( ${PROJECT_NAME} PUBLIC $<$<CONFIG:Release>: ${PROJECTS_SHARED_CXX_FLAGS_RELEASE}> )
endif()
if (PROJECTS_SHARED_DEFINES_RELEASE)
	target_compile_definitions( ${PROJECT_NAME} PUBLIC $<$<CONFIG:Release>: ${PROJECTS_SHARED_DEFINES_RELEASE}> )
endif()
if (PROJECTS_SHARED_LINKER_FLAGS_RELEASE)
	set_target_properties( ${PROJECT_NAME} PROPERTIES LINK_FLAGS_RELEASE ${PROJECTS_SHARED_LINKER_FLAGS_RELEASE} )
endif()

# Profile
if (PROJECTS_SHARED_DEFINES_PROFILE)
	target_compile_definitions( ${PROJECT_NAME} PUBLIC $<$<CONFIG:Profile>: ${PROJECTS_SHARED_DEFINES_PROFILE}> )
endif()
if (PROJECTS_SHARED_LINKER_FLAGS_PROFILE)
	set_target_properties( ${PROJECT_NAME} PROPERTIES LINK_FLAGS_PROFILE ${PROJECTS_SHARED_LINKER_FLAGS_PROFILE} )
endif()
if (PROJECTS_SHARED_CXX_FLAGS_PROFILE)
	target_compile_options( ${PROJECT_NAME} PUBLIC $<$<CONFIG:Profile>: ${PROJECTS_SHARED_CXX_FLAGS_PROFILE}> )
endif()

set_target_properties( ${PROJECT_NAME} PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES )

if (${VSA_ENABLE_GRAPHVIZ})
	target_link_libraries( ${PROJECT_NAME} "GraphViz-lib" )
endif ()

#==============================================================================
# Copy json

add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/src/${PROJECT_NAME}.json"
		"$<TARGET_FILE_DIR:${PROJECT_NAME}>/${PROJECT_NAME}.json"
	VERBATIM )

#==============================================================================

message( STATUS "project '${PROJECT_NAME}' generation ended" )
message( STATUS "\n==========================================================================" )
