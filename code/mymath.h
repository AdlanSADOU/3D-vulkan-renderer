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
        float width, height;
    };

    float e[2];
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

    static Vector3 Zero()
    {
        return { 0.0f, 0.0f, 0.0f };
    }
};

inline Vector3 operator+(Vector3 &v1, const Vector3 &v2)
{
    Vector3 res;
    res.x = v1.x + v2.x;
    res.y = v1.y + v2.y;
    res.z = v1.z + v2.z;

    return res;
}

inline Vector3 operator-(Vector3 &v1, const Vector3 &v2)
{
    Vector3 res;
    res.x = v1.x - v2.x;
    res.y = v1.y - v2.y;
    res.z = v1.z - v2.z;

    return res;
}

inline void operator+=(Vector3 &v1, Vector3 &v2)
{
    v1.x += v2.x;
    v1.y += v2.y;
    v1.z += v2.z;
}

inline void operator-=(Vector3 &v1, Vector3 &v2)
{
    v1.x -= v2.x;
    v1.y -= v2.y;
    v1.z -= v2.z;
}

inline Vector3 operator*(float a, Vector3 &v)
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
        return {
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            tr.x, tr.y, tr.z, 1.f
        };
    }

    static Matrix4 Perspective(float zNear, float zFar, float fovy, float aspect)
    {
        float f = 1 / tan(((fovy) / 2.0f) * (M_PI / 180.0f));

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

    static Matrix4 LookAt(Vector3 position, Vector3 target, Vector3 up)
    {
        Vector3 p = position;
        Vector3 d = Vector3::Normalize(position - target);
        Vector3 r = Vector3::Normalize(Vector3::Cross(up, d));
        Vector3 u = Vector3::Normalize(Vector3::Cross(r, d));

        // todo: probably needs inversion
        Matrix4 rot = {
            r.x, r.y, r.z, 0.0,
            u.x, u.y, u.z, 0.0,
            d.x, d.y, d.z, 0.0,
            0.0, 0.0, 0.0, 0.0
        };

        Matrix4 tr = {
            1.0, 0.0, 0.0, -p.x,
            0.0, 1.0, 0.0, -p.y,
            0.0, 0.0, 1.0, -p.z,
            0.0, 0.0, 0.0, 1.0f
        };

        // todo Matrix * Matrix multiplication
        // return rot * tr;
        return {};
    }
};
