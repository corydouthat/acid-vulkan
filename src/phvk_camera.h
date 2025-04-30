// Copyright (c) 2025, Cory Douthat
// Based on vkguide.net - see LICENSE.txt
//
// Acid Graphics Engine - Vulkan (Ver 1.3-1.4)
// Camera class

#include "phvk_types.h"

#include <SDL_events.h>

class Camera 
{
public:
    Vec3f velocity;
    Vec3f position;
    
    float pitch{ 0.f };
    float yaw{ 0.f };

    Mat4f getViewMatrix();
    Mat4f getRotationMatrix();

    void processSDLEvent(SDL_Event& e);

    void update();
};