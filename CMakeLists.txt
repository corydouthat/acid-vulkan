
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Needed for SDL
set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(Vulkan REQUIRED)

project (acid-vulkan)

if (NOT TARGET acid-vulkan)
	add_library(acid-vulkan INTERFACE
		#including files here will force Visual Studio to show library
		"src/phVkDescriptors.hpp"
		"src/phVkEngine.hpp"
	)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set target include directories
target_include_directories(acid-vulkan INTERFACE
	src
)

# Add dependencies (has dedicated CMakeLists.txt)
add_subdirectory(dependencies dependencies)

# Target link libraries
target_link_libraries(acid-vulkan 
	INTERFACE Vulkan::Vulkan	# TODO: optional?
	INTERFACE SDL3::SDL3		# TODO: optional?
	INTERFACE dependencies
)

# Example to include a local main function / testbench
add_executable(acid-vulkan-testbench "testbench/main.cpp")
target_link_libraries(acid-vulkan-testbench PRIVATE acid-vulkan)
