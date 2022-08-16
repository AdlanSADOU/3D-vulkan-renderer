/*TODO
    Vector4
    Matrix * Vector operator
    Ortho projection
*/
#pragma once

#include <math.h>

#define PI 3.14159265358979323846264338327950288

static float Radians(float degrees)
{
    return degrees * PI / 180;
}

static float Degrees(float radians)
{
    return radians * 180 / PI;
}

union Vector2 {
    struct
    {
        float x, y;
    };
    struct
    {
        float u, v;
    };
    struct
    {
        float width, height;
    };

    float e[2];

    Vector2 Normalized()
    {
        float l = sqrt(powf(e[0], 2.f) + powf(e[1], 2.f));
        return { e[0] / l, e[1] / l };
    }

    static Vector2 Normalize(Vector2 v)
    {
        float l = sqrt(powf(v.x, 2.f) + powf(v.y, 2.f));
        return { v.x / l, v.y / l };
    }
};

struct Vector3
{
    float x, y, z;

    Vector3 Normalized()
    {
        Vector3 res;
        float   l = sqrtf(powf(x, 2.f) + powf(y, 2.f) + powf(z, 2.f));

        res = { x / l, y / l, z / l };

        return res;
    }

    static Vector3 Normalize(Vector3 v)
    {
        Vector3 res;
        float   l = Length(v);

        res = { v.x / l, v.y / l, v.z / l };

        return res;
    }

    static float LengthSquared(Vector3 v)
    {
        return powf(v.x, 2.f) + powf(v.y, 2.f) + powf(v.z, 2.f);
    }

    static float Length(Vector3 v)
    {
        return sqrtf(powf(v.x, 2.f) + powf(v.y, 2.f) + powf(v.z, 2.f));
    }

    static Vector3 Cross(Vector3 v1, Vector3 v2)
    {
        return {
            v1.y * v2.z - v1.z * v2.y,
            v1.z * v2.x - v1.x * v2.z,
            v1.x * v2.y - v1.y * v2.x
        };
    }

    static float Dot(Vector3 v1, Vector3 v2)
    {
        return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
    }

    static Vector3 Zero()
    {
        return { 0.0f, 0.0f, 0.0f };
    }
};

inline Vector3 operator+(const Vector3 &v1, const Vector3 &v2)
{
    Vector3 res;
    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    res.z = v1.z + v2.z;

    return res;
}

inline Vector3 operator-(const Vector3 &v1, const Vector3 &v2)
{
    Vector3 res;
    res.x = v1.x - v2.x;
    res.y = v1.y - v2.y;
    res.z = v1.z - v2.z;

    return res;
}

inline void operator+=(Vector3 &v1, const Vector3 &v2)
{
    v1.x += v2.x;
    v1.y += v2.y;
    v1.z += v2.z;
}

inline Vector3 operator+=(const Vector3 &v1, const Vector3 &v2)
{
    Vector3 res;

    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    res.z = v1.z + v2.z;
    return res;
}

inline void operator-=(Vector3 &v1, const Vector3 &v2)
{
    v1.x -= v2.x;
    v1.y -= v2.y;
    v1.z -= v2.z;
}

inline Vector3 operator*(const float &a, const Vector3 &v)
{
    Vector3 res;
    res.x = a * v.x;
    res.y = a * v.y;
    res.z = a * v.z;

    return res;
}

inline Vector3 operator*(const Vector3 &v, const float &a)
{
    Vector3 res;
    res.x = a * v.x;
    res.y = a * v.y;
    res.z = a * v.z;

    return res;
}

inline Vector3 operator*(Vector3 &v1, const Vector3 &v2)
{
    Vector3 res;
    res.x = v1.x * v2.x;
    res.y = v1.y * v2.y;
    res.z = v1.z * v2.z;

    return res;
}


struct Matrix4
{
    float m[16];
};

inline Matrix4 operator*(Matrix4 m1, Matrix4 m2)
{
    Matrix4 res;

    for (size_t i = 0; i <= 12; i += 4) {
        for (size_t j = 0; j <= 3; j += 1) {
            res.m[i + j] = m1.m[i] * m2.m[j] + m1.m[i + 1] * m2.m[j + 4] + m1.m[i + 2] * m2.m[j + 8] + m1.m[i + 3] * m2.m[j + 12];
            // size_t counter = i + j;
            // float  interm  = res.m[counter];
            // interm         = res.m[counter];
        }
    }

    return res;
}

static Matrix4 Identity()
{
    return {
        1., 0., 0., 0.,
        0., 1., 0., 0.,
        0., 0., 1., 0.,
        0., 0., 0., 1.
    };
}

static Matrix4 Transpose(Matrix4 m)
{
    Matrix4 res {};

    for (size_t j = 0; j < 11; j += 4) {
        for (size_t i = 0; i < 3; i++) {
            // res[j+i]
        }
    }

    return res;
}

static Matrix4 Translate(Vector3 tr)
{
    return {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        tr.x, tr.y, tr.z, 1.f
    };
}

static Matrix4 Scale(Vector3 s)
{
    return {
        s.x, 0.f, 0.f, 0.f,
        0.f, s.y, 0.f, 0.f,
        0.f, 0.f, s.z, 0.f,
        0.0, 0.0, 0.0, 1.f
    };
}


static Matrix4 Rotate(Vector3 rot, float radians)
{
    return Matrix4();
}

static Matrix4 RotateX(float radians)
{

    Matrix4 xrot {};
    xrot = {
        1, 0, 0, 0,
        0, cosf(radians), -sinf(radians), 0,
        0, sinf(radians), cosf(radians), 0,
        0, 0, 0, 1
    };

    return xrot;
}

static Matrix4 RotateY(float radians)
{
    Matrix4 yrot {};
    yrot = {
        cosf(radians), 0.f, sinf(radians), 0.f,
        0.f, 1.f, 0.f, 0.f,
        -sinf(radians), 0.f, cosf(radians), 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    return yrot;
}

static Matrix4 RotateZ(float radians)
{
    Matrix4 zrot {};
    zrot = {
        cosf(radians), -sinf(radians), 0, 0,
        sinf(radians), cosf(radians), 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    return zrot;
}

static Matrix4 Perspective(float zNear, float zFar, float fovy, float aspect)
{
    float f = 1 / tan(((fovy) / 2.0f) * (PI / 180.0f));

    float x = f / aspect;
    float z = (zFar + zNear) / (zNear - zFar);
    float w = (2 * zFar * zNear) / (zNear - zFar);

    // return {
    //     x, 0, 0, 0,
    //     0, f, 0, 0,
    //     0, 0, z, w,
    //     0, 0, -1, 0
    // };
    return {
        x, 0, 0, 0,
        0, f, 0, 0,
        0, 0, z, -1,
        0, 0, w, 0
    };
}


static Matrix4 LookAt(Vector3 eye, Vector3 at, Vector3 up)
{
    Vector3 d = Vector3::Normalize(eye - at);
    Vector3 r = Vector3::Normalize(Vector3::Cross(up, d));
    Vector3 u = Vector3::Cross(d, r);

    // todo: probably needs inversion
    // Matrix4 lookat = {
    //     r.x, r.y, r.z, Vector3::Dot(r, eye),
    //     u.x, u.y, u.z, Vector3::Dot(u, eye),
    //     d.x, d.y, d.z, Vector3::Dot(d, eye),
    //     0.0, 0.0, 0.0, 1.0
    // };

    Matrix4 lookat = {
        r.x, u.x, d.x, 0.0,
        r.y, u.y, d.y, 0.0,
        r.z, u.z, d.z, 0.0,
        -Vector3::Dot(r, eye), -Vector3::Dot(u, eye), -Vector3::Dot(d, eye), 1.0f
    };

    return lookat;
}