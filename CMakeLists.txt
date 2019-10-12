cmake_minimum_required( VERSION 3.11 FATAL_ERROR )

message( STATUS "==========================================================================\n" )
message( STATUS "project 'VkInterceptor' generation started" )

project( "VkInterceptor" LANGUAGES C CXX )
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
# Vulkan

FetchContent_Declare( ExternalVulkanHeaders
	GIT_REPOSITORY		https://github.com/KhronosGroup/Vulkan-Headers.git
	GIT_TAG				"v1.1.115"
	SOURCE_DIR			"external/Vulkan-Headers"
)
	
FetchContent_GetProperties( ExternalVulkanHeaders )
if (NOT ExternalVulkanHeaders_POPULATED)
	message( STATUS "downloading Vulkan-Headers" )
	FetchContent_Populate( ExternalVulkanHeaders )
endif ()
	
add_library( "Vulkan-lib" INTERFACE )
target_include_directories( "Vulkan-lib" INTERFACE "external/Vulkan-Headers/include" )
target_compile_definitions( "Vulkan-lib" INTERFACE "" )

#==============================================================================
# Interceptor

file( GLOB_RECURSE SOURCES "src/*.*" )
message( STATUS "SOURCES: ${SOURCES}" )
add_executable( "VkInterceptor" ${SOURCES} )
set_property( TARGET "VkInterceptor" PROPERTY FOLDER "" )
source_group( TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SOURCES} )

# Debug
if (PROJECTS_SHARED_CXX_FLAGS_DEBUG)
	target_compile_options( "VkInterceptor" PUBLIC $<$<CONFIG:Debug>: ${PROJECTS_SHARED_CXX_FLAGS_DEBUG}> )
endif()
if (PROJECTS_SHARED_DEFINES_DEBUG)
	target_compile_definitions( "VkInterceptor" PUBLIC $<$<CONFIG:Debug>: ${PROJECTS_SHARED_DEFINES_DEBUG}> )
endif()
if (PROJECTS_SHARED_LINKER_FLAGS_DEBUG)
	set_target_properties( "VkInterceptor" PROPERTIES LINK_FLAGS_DEBUG ${PROJECTS_SHARED_LINKER_FLAGS_DEBUG} )
endif()

# Release
if (PROJECTS_SHARED_CXX_FLAGS_RELEASE)
	target_compile_options( "VkInterceptor" PUBLIC $<$<CONFIG:Release>: ${PROJECTS_SHARED_CXX_FLAGS_RELEASE}> )
endif()
if (PROJECTS_SHARED_DEFINES_RELEASE)
	target_compile_definitions( "VkInterceptor" PUBLIC $<$<CONFIG:Release>: ${PROJECTS_SHARED_DEFINES_RELEASE}> )
endif()
if (PROJECTS_SHARED_LINKER_FLAGS_RELEASE)
	set_target_properties( "VkInterceptor" PROPERTIES LINK_FLAGS_RELEASE ${PROJECTS_SHARED_LINKER_FLAGS_RELEASE} )
endif()

# Profile
if (PROJECTS_SHARED_DEFINES_PROFILE)
	target_compile_definitions( "VkInterceptor" PUBLIC $<$<CONFIG:Profile>: ${PROJECTS_SHARED_DEFINES_PROFILE}> )
endif()
if (PROJECTS_SHARED_LINKER_FLAGS_PROFILE)
	set_target_properties( "VkInterceptor" PROPERTIES LINK_FLAGS_PROFILE ${PROJECTS_SHARED_LINKER_FLAGS_PROFILE} )
endif()
if (PROJECTS_SHARED_CXX_FLAGS_PROFILE)
	target_compile_options( "VkInterceptor" PUBLIC $<$<CONFIG:Profile>: ${PROJECTS_SHARED_CXX_FLAGS_PROFILE}> )
endif()

set_target_properties( "VkInterceptor" PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED YES )

#==============================================================================

message( STATUS "project 'VkInterceptor' generation ended" )
message( STATUS "\n==========================================================================" )
