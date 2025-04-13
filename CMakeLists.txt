# CMakeList.txt : CMake project for acid-vulkan, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

find_package(Vulkan REQUIRED FATAL_ERROR)

project (acid-vulkan)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add source to this project's executable.
add_executable(acid-vulkan 
	main.cpp
	src/phvk_types.h
	src/phvk_engine.h
	src/phvk_initializers.h
	src/phvk_engine.cpp
	src/phvk_initializers.cpp
)

# Set target include directories
target_include_directories(acid-vulkan PRIVATE
	src
)

# Add dependencies (has dedicated CMakeLists.txt)
add_subdirectory(dependencies dependencies)

# Target link libraries
target_link_libraries(acid-vulkan 
	PUBLIC Vulkan::Vulkan
	PRIVATE dependencies
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET acid-vulkan PROPERTY CXX_STANDARD 20)
endif()
