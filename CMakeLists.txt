include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.conan.txt)

cmake_minimum_required (VERSION 3.15)
project(fea_state_machines VERSION 1.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GoogleTest)
include(FetchContent)

set(CMAKE_CXX_STANDARD 17)

# Conan search path.
set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

# Output binary to predictable location (fixes cyclic dependency issues).
set(BINARY_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_OUT_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_OUT_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BINARY_OUT_DIR})

foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
	string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${BINARY_OUT_DIR})
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${BINARY_OUT_DIR})
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${BINARY_OUT_DIR})
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

# Organize unrelated targets to clean IDE hierarchy.
set(DEPENDENCY_FOLDER "Dependencies")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER ${DEPENDENCY_FOLDER})

# Compile Options
function(set_compile_options REQUIRED_ARG EXPOSURE)
	if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		target_compile_options(${REQUIRED_ARG} ${EXPOSURE} -Wall -Wextra -Wpedantic -Werror -Wno-gnu-zero-variadic-macro-arguments)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		target_compile_options(${REQUIRED_ARG} ${EXPOSURE} -Wall -Wextra -Wpedantic -Werror)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
	  # using Intel C++
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		target_compile_definitions(${REQUIRED_ARG} ${EXPOSURE} NOMINMAX UNICODE _UNICODE)
		target_compile_options(${REQUIRED_ARG} ${EXPOSURE} /Zc:__cplusplus /Zc:alignedNew /permissive- /FAs /W4 /WX /utf-8)
		# set_target_properties(${REQUIRED_ARG} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
endfunction()

# clang-format
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.clang-format ${CMAKE_CURRENT_BINARY_DIR}/.clang-format COPYONLY)


# Main Project
file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.hpp")
add_library(${PROJECT_NAME} INTERFACE)

# To see files in IDE
target_sources(${PROJECT_NAME} INTERFACE
	"$<BUILD_INTERFACE:${HEADER_FILES}>"
)

# Interface
target_include_directories(${PROJECT_NAME} INTERFACE
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
set_compile_options(${PROJECT_NAME} INTERFACE)


# Install Package Configuration
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}_targets)

install(EXPORT ${PROJECT_NAME}_targets
	NAMESPACE ${PROJECT_NAME}::
	FILE ${PROJECT_NAME}-config.cmake
	DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")


# Tests
option(FEA_STATE_MACHINES_TESTS "Build and run tests." On)
if (${FEA_STATE_MACHINES_TESTS})
	enable_testing()

	# Tests external dependencies.
	FetchContent_Declare(fea_benchmark
		GIT_REPOSITORY https://github.com/p-groarke/fea_benchmark.git
	)
	FetchContent_MakeAvailable(fea_benchmark)
	set_target_properties(fea_benchmark_tests PROPERTIES FOLDER ${DEPENDENCY_FOLDER})

	find_package(GTest CONFIG REQUIRED)


	# Test Project
	set(TEST_NAME ${PROJECT_NAME}_tests)
	file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp" "tests/*.c" "tests/*.hpp" "tests/*.h" "tests/*.tpp")
	add_executable(${TEST_NAME} ${TEST_SOURCES})
	set_compile_options(${TEST_NAME} PRIVATE)
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TEST_NAME})

	target_link_libraries(${TEST_NAME} PRIVATE ${PROJECT_NAME} GTest::GTest fea_benchmark)
	gtest_discover_tests(${TEST_NAME})

endif() # FEA_STATE_MACHINES_TESTS
