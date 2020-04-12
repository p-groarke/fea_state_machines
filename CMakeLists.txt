include(${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.conan.txt)

cmake_minimum_required (VERSION 3.10)
project(fea_state_machines VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(GoogleTest)
include(ExternalProject)

set(CMAKE_CXX_STANDARD 17)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)
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

file(GLOB_RECURSE HEADER_FILES "${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}/*.hpp")
add_library(${PROJECT_NAME} INTERFACE)
target_sources(${PROJECT_NAME} INTERFACE
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	$<BUILD_INTERFACE:${HEADER_FILES}>
)
target_include_directories(${PROJECT_NAME} INTERFACE
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	target_compile_options(${PROJECT_NAME} INTERFACE -Wall -Wextra -Wpedantic -Werror)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	target_compile_options(${PROJECT_NAME} INTERFACE -Wall -Wextra -Wpedantic -Werror)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
	# using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	target_compile_options(${PROJECT_NAME} INTERFACE /W4 /WX /EHsc /Zc:__cplusplus /FAs /permissive-)
	target_compile_definitions(${PROJECT_NAME} INTERFACE NOMINMAX)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/.clang-format ${CMAKE_CURRENT_BINARY_DIR}/.clang-format COPYONLY)

# Install Package Configuration
install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}_targets)

install(EXPORT ${PROJECT_NAME}_targets
	NAMESPACE ${PROJECT_NAME}::
	FILE ${PROJECT_NAME}-config.cmake
	DESTINATION "${CMAKE_INSTALL_DATADIR}/cmake/${PROJECT_NAME}"
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")


# Tests
option(BUILD_TESTING "Build and run tests." Off)
if (${BUILD_TESTING})
	enable_testing()

	set(TEST_NAME ${PROJECT_NAME}_tests)
	file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp" "tests/*.c" "tests/*.hpp" "tests/*.h" "tests/*.tpp")
	add_executable(${TEST_NAME} ${TEST_SOURCES})

	find_package(GTest CONFIG REQUIRED)
	target_link_libraries(${TEST_NAME} PRIVATE ${PROJECT_NAME} GTest::GTest)

	gtest_discover_tests(${TEST_NAME})
	add_dependencies(${TEST_NAME} ${PROJECT_NAME})

	if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-gnu-zero-variadic-macro-arguments)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		target_compile_options(${TEST_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
		# using Intel C++
	elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		target_compile_options(${TEST_NAME} PRIVATE /W4 /WX /EHsc /Zc:__cplusplus /FAs /permissive- /bigobj)
		target_compile_definitions(${TEST_NAME} PRIVATE NOMINMAX)
		set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TEST_NAME})
	endif()

	##
	# External dependencies.
	##
	set(LIB_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include)
	target_include_directories(${TEST_NAME} PRIVATE ${LIB_INCLUDE_DIR})

	ExternalProject_Add(bench_util_ext
		GIT_REPOSITORY https://github.com/p-groarke/bench_util.git
		CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release
			-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}
			-DCMAKE_PREFIX_PATH=${CMAKE_CURRENT_BINARY_DIR}
			-DBUILD_TESTING=Off
		UPDATE_COMMAND ""
	)

	add_dependencies(${TEST_NAME} bench_util_ext)
	set_target_properties(bench_util_ext PROPERTIES FOLDER ${DEPENDENCY_FOLDER})

endif() # BUILD_TESTING
