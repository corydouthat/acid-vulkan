cmake_minimum_required(VERSION 3.15)

add_library(dependencies INTERFACE)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add fastgltf
add_subdirectory(fastgltf fastgltf)
target_link_libraries(dependencies INTERFACE fastgltf)

# Add fmt
add_subdirectory(fmt fmt)
target_link_libraries(dependencies INTERFACE fmt)

# Add imgui
add_library(imgui STATIC
    imgui/imgui.cpp
    imgui/imgui_demo.cpp    #TODO: only temporary?
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC
    imgui
    imgui/backends
)
target_link_libraries(dependencies INTERFACE imgui)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL2::SDL2)

# Add libraries
add_subdirectory(libraries libraries)
target_link_libraries(dependencies INTERFACE libraries)

# Add SDL
add_subdirectory(SDL SDL)
target_link_libraries(dependencies INTERFACE SDL2::SDL2)

# Add stb (header-only library, no CMakeLists.txt required)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE stb)
target_link_libraries(dependencies INTERFACE stb)

# Add vk-bootstrap
add_subdirectory(vk-bootstrap vk-bootstrap)
target_link_libraries(dependencies INTERFACE vk-bootstrap)

# Add volk
# Note: volk causes issues with imgui when calling vkCreateSampler
# there is supposed to be a compile flag for imgui (IMGUI_IMPL_VULKAN_USE_VOLK),
# but I couldn't get it to work and vkguide.dev chapter 2 doesn't require volk yet 
# so I gave up until it is required.
#add_subdirectory(volk volk)
#target_link_libraries(dependencies INTERFACE volk)

# Add VulkanMemoryAllocator (VMA)
add_subdirectory(VulkanMemoryAllocator VulkanMemoryAllocator)
target_link_libraries(dependencies INTERFACE VulkanMemoryAllocator)

# Example to include a local main function to test building the library
#add_executable(dependencies-testbench )
#target_link_libraries(dependencies-testbench 
#	PRIVATE fastgltf
#	PRIVATE fmt
#	PRIVATE imgui
#	PRIVATE libraries
#	PRIVATE SDL2::SDL2
#   PRIVATE stb
#   PRIVATE vk-bootstrap
#   PRIVATE volk
#   PRIVATE VulkanMemoryAllocator
#)
