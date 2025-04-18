// Copyright (c) 2025, Cory Douthat
//
// Acid Game Engine - Vulkan
// Primary includes and type definitions

#pragma once

#include <cassert>
//#include <memory>
//#include <optional>
//#include <string>
#include <vector>   // TODO: replace with phArrayList?
//#include <span>
//#include <array>
//#include <functional>
//#include <deque>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

#include "vec.hpp"
#include "mat.hpp"


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
