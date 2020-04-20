#include "camera.h"
#include <math/mat3.h>
#include <filament/Camera.h>

//------------------------------------------------------------------------------

CameraManipulator::CameraManipulator(filament::math::float3 pos, filament::math::float3 target, filament::math::float3 up)
    : m_position(pos), m_target(target), m_up(up) {}

//------------------------------------------------------------------------------

// rotate around camera local z-axis (viewing direction)
void CameraManipulator::roll(float radians) {
    using namespace filament;
    math::float3 viewdir = normalize(m_target - m_position);

    math::mat3f rotation = math::mat3f::rotation(radians, viewdir);

    m_up = normalize(rotation * m_up);
}

//------------------------------------------------------------------------------

// rotate camera around local y-axis (up direction)
void CameraManipulator::yaw(float radians) {
    filament::math::float3 view = (m_target - m_position);
    float dist = length(view);

    filament::math::float3 viewdir = normalize(view);

    auto rotation = filament::math::mat3f::rotation(radians, m_up);

    viewdir = normalize(rotation * viewdir);

    m_position = m_target - (viewdir * dist);
}

//------------------------------------------------------------------------------

// rotate around local x-axis (viewing direction x up)
void CameraManipulator::pitch(float radians) {
    filament::math::float3 view = (m_target - m_position);
    float dist = norm(view);

    filament::math::float3 viewdir = normalize(view);

    // Get the rotation axis.
    filament::math::float3 rot_axis = cross(viewdir, m_up);

    auto xform = filament::math::mat3f::rotation(radians, rot_axis);

    // Apply to both viewing direction and up-vector.
    viewdir = normalize(xform * viewdir);
    m_up = normalize(xform * m_up);

    m_position = m_target - (viewdir * dist);
}

//------------------------------------------------------------------------------

void CameraManipulator::moveRelative(filament::math::float3 dir) {
    filament::math::float3 viewdir = normalize(m_target - m_position);
    filament::math::float3 bivector = cross(viewdir, m_up);
    m_position += dir[0] * bivector;
    m_position += dir[1] * m_up;
    m_position += dir[2] * viewdir;

    m_target += dir[0] * bivector;
    m_target += dir[1] * m_up;
    m_target += dir[2] * viewdir;
}

//------------------------------------------------------------------------------

// Move camera along z-axis
void CameraManipulator::dolly(float distance) { moveRelative(filament::math::float3(0, 0, distance)); }

//------------------------------------------------------------------------------

void CameraManipulator::updateCamera(filament::Camera* cam) {
    cam->lookAt({m_position[0], m_position[1], m_position[2]},
                {m_target[0], m_target[1], m_target[2]},
                {m_up[0], m_up[1], m_up[2]});
}

//------------------------------------------------------------------------------
