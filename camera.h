#pragma once

#include <filament/Engine.h>
#include <math/vec3.h>

class CameraManipulator {
   public:
    CameraManipulator() = default;
    CameraManipulator(filament::math::float3 pos, filament::math::float3 target, filament::math::float3 up);
    ~CameraManipulator() = default;

    // rotate around camera local z-axis (viewing direction)
    void roll(float radians);

    // rotate camera around local y-axis (up direction)
    void yaw(float radians);

    // rotate around local x-axis (viewing direction x up)
    void pitch(float radians);

    // The following moves the camera relative to it's current local axes:
    //      dir.x: moves in camera's local x-axis (axis of pitch)
    //      dir.y: moves in camera's local y-axis (up-vector, axis of yaw)
    //      dir.z: moves in camera's local z-axis (axis of roll)
    // This will make it easier to respond to mouse events such as "zoom" and
    // "pan".
    void moveRelative(filament::math::float3 dir);

    // Move camera along z-axis
    void dolly(float distance);

    filament::math::float3 pos() const { return m_position; }
    filament::math::float3 up() const { return m_up; }
    filament::math::float3 target() const { return m_target; }

    void updateCamera(filament::Camera* cam);

   private:
    filament::math::float3 m_position;
    filament::math::float3 m_target;
    filament::math::float3 m_up;
};
