#pragma once

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
        float Width, Height;
    };

    float E[2];
};

struct Vector3
{
    float x, y, z;
};

inline Vector3 operator+(Vector3 v1, Vector3 &v2)
{
    Vector3 res;
    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    res.z = v1.z + v2.z;

    return res;
}

inline Vector3 operator*(float a, Vector3 v)
{
    Vector3 res;
    res.x = a * v.x;
    res.y = a * v.y;
    res.z = a * v.z;

    return res;
}

struct Matrix4
{
    float m[16];

    static Matrix4 Identity()
    {
        return {
            1., 0., 0., 0.,
            0., 1., 0., 0.,
            0., 0., 1., 0.,
            0., 0., 0., 1.
        };
    }

    static Matrix4 Translate(Vector3 tr)
    {
        Matrix4 res;

        // res.m[0]  = 1;
        // res.m[5]  = 1;
        // res.m[10] = 1;
        // res.m[15] = 1;
        // res.m[3]  = tr.x;
        // res.m[7]  = tr.y;
        // res.m[11] = tr.z;

        return {
            1.f, 0.f, 0.f, tr.x,
            0.f, 1.f, 0.f, tr.y,
            0.f, 0.f, 1.f, tr.z,
            0.f, 0.f, 0.f, 1.f

            // tr.x, tr.y, tr.z, 1.f
        };
    } 

    static Matrix4 Perspective(float zNear, float zFar, float fovy, float aspect)
    {
        float f = 1/ tan(((fovy) / 2) * (M_PI / 180));

        float x  = f / aspect;
        float z  = (zFar + zNear) / (zNear - zFar);
        float w  = (2 * zFar * zNear) / (zNear - zFar);

        return {
            x, 0, 0, 0,
            0, f, 0, 0,
            0, 0, z, w,
            0, 0, -1, 0
        };
    }
};
