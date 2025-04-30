// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Camera class

#include "phvk_camera.h"
#include "vec.hpp"
#include "quat.hpp"
#include "mat.hpp"

Mat4f Camera::getViewMatrix()
{
    // To create a correct model view, we need to move the world in opposite
    // direction to the camera, so we will create the camera model matrix and invert
    Mat4f camera_translation = Mat4f::transl(position);
    Mat4f camera_rotation = getRotationMatrix();
    return (camera_translation * camera_rotation).inv();
}

Mat4f Camera::getRotationMatrix()
{
    // Fairly typical FPS style camera. we join the pitch and yaw rotations into
    // the final rotation matrix

    Quatf pitch_rotation = Quatf(pitch, Vec3f(1.f, 0.f, 0.f));
    Quatf yaw_rotation = Quatf(yaw, Vec3f(0.f, -1.f, 0.f));

    // TODO: Why are we making quaternions only to convert them to matrix?
    return Mat4f(Mat3f::rot(yaw_rotation)) * Mat4f(Mat3f::rot(pitch_rotation));
}

void Camera::processSDLEvent(SDL_Event& e)
{
    if (e.type == SDL_KEYDOWN) 
    {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = -1; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 1; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = -1; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 1; }
    }

    if (e.type == SDL_KEYUP) 
    {
        if (e.key.keysym.sym == SDLK_w) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_s) { velocity.z = 0; }
        if (e.key.keysym.sym == SDLK_a) { velocity.x = 0; }
        if (e.key.keysym.sym == SDLK_d) { velocity.x = 0; }
    }

    if (e.type == SDL_MOUSEMOTION) 
    {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
}

void Camera::update()
{
    Mat4f camera_rotation = getRotationMatrix();
    position += Vec3f(camera_rotation * Vec3f(velocity * 0.5f));
}