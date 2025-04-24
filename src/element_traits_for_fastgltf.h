// Copyright (c) 2025, Cory Douthat
// Provides element traits specialization for fastgltf

#include <fastgltf/tools.hpp>
#include "vec.hpp"
#include "mat.hpp"


namespace fastgltf 
{
    // Vec2<T> specialization
    template <typename T>
    struct ElementTraits<Vec2<T>> : ElementTraitsBase<Vec2<T>, AccessorType::Vec2, T> {};

    // Vec3<T> specialization
    template <typename T>
    struct ElementTraits<Vec3<T>> : ElementTraitsBase<Vec3<T>, AccessorType::Vec3, T> {};

    // Vec4<T> specialization
    template <typename T>
    struct ElementTraits<Vec4<T>> : ElementTraitsBase<Vec4<T>, AccessorType::Vec4, T> {};

    // Mat3<T> specialization
    template <typename T>
    struct ElementTraits<Mat3<T>> : ElementTraitsBase<Mat3<T>, AccessorType::Mat3, T> {};

    // Mat4<T> specialization
    template <typename T>
    struct ElementTraits<Mat4<T>> : ElementTraitsBase<Mat4<T>, AccessorType::Mat4, T> {};
}