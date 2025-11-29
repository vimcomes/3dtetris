#pragma once

#include <algorithm>
#include <array>
#include <cmath>

struct Vec3
{
    float x;
    float y;
    float z;
};

struct Vec3i
{
    int x;
    int y;
    int z;
};

struct Mat4
{
    std::array<float, 16> m{};
};

constexpr float to_radians(float degrees)
{
    return degrees * 0.01745329251994329577f;
}

inline Mat4 identity()
{
    Mat4 out{};
    out.m = {1.f, 0.f, 0.f, 0.f,
             0.f, 1.f, 0.f, 0.f,
             0.f, 0.f, 1.f, 0.f,
             0.f, 0.f, 0.f, 1.f};
    return out;
}

inline Mat4 multiply(const Mat4& a, const Mat4& b)
{
    Mat4 r{};
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            r.m[col * 4 + row] =
                a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                a.m[3 * 4 + row] * b.m[col * 4 + 3];
        }
    }
    return r;
}

inline Mat4 translation(const Vec3& t)
{
    Mat4 r = identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

inline Mat4 scale(float s)
{
    Mat4 r{};
    r.m = {s, 0.f, 0.f, 0.f,
           0.f, s, 0.f, 0.f,
           0.f, 0.f, s, 0.f,
           0.f, 0.f, 0.f, 1.f};
    return r;
}

inline Mat4 rotation_x(float radians)
{
    Mat4 r = identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[5] = c;
    r.m[6] = s;
    r.m[9] = -s;
    r.m[10] = c;
    return r;
}

inline Mat4 rotation_y(float radians)
{
    Mat4 r = identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[0] = c;
    r.m[2] = -s;
    r.m[8] = s;
    r.m[10] = c;
    return r;
}

inline Mat4 rotation_z(float radians)
{
    Mat4 r = identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    r.m[0] = c;
    r.m[1] = s;
    r.m[4] = -s;
    r.m[5] = c;
    return r;
}

inline float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b)
{
    return Vec3{a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
}

inline Vec3 normalize(const Vec3& v)
{
    float len = std::sqrt(dot(v, v));
    if (len < 1e-6f)
    {
        return v;
    }
    float inv = 1.0f / len;
    return Vec3{v.x * inv, v.y * inv, v.z * inv};
}

inline Mat4 look_at(const Vec3& eye, const Vec3& target, const Vec3& up)
{
    Vec3 f = normalize(Vec3{target.x - eye.x, target.y - eye.y, target.z - eye.z});
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 r = identity();
    r.m[0] = s.x;
    r.m[4] = s.y;
    r.m[8] = s.z;

    r.m[1] = u.x;
    r.m[5] = u.y;
    r.m[9] = u.z;

    r.m[2] = -f.x;
    r.m[6] = -f.y;
    r.m[10] = -f.z;

    r.m[12] = -dot(s, eye);
    r.m[13] = -dot(u, eye);
    r.m[14] = dot(f, eye);
    return r;
}

inline Mat4 perspective(float fov_deg, float aspect, float near, float far)
{
    float f = 1.0f / std::tan(to_radians(fov_deg) * 0.5f);
    Mat4 r{};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = (far + near) / (near - far);
    r.m[11] = -1.0f;
    r.m[14] = (2.0f * far * near) / (near - far);
    return r;
}

inline Mat4 orthographic(float left, float right, float bottom, float top, float near, float far)
{
    Mat4 r{};
    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = -2.0f / (far - near);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = -(far + near) / (far - near);
    r.m[15] = 1.0f;
    return r;
}
