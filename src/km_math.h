#pragma once

#include <math.h>

#include "km_defines.h"

#define PI_F 3.14159265f

inline int AbsInt(int n) {
	return n >= 0 ? n : -n;
}
inline int MinInt(int a, int b) {
	return a < b ? a : b;
}
inline int MaxInt(int a, int b) {
	return a > b ? a : b;
}
inline int ClampInt(int a, int min, int max) {
	return MinInt(MaxInt(a, min), max);
}
inline uint32 MinUInt32(uint32 a, uint32 b) {
	return a < b ? a : b;
}
inline uint32 MaxUInt32(uint32 a, uint32 b) {
	return a > b ? a : b;
}
inline uint32 ClampUInt32(uint32 a, uint32 min, uint32 max) {
	return MinUInt32(MaxUInt32(a, min), max);
}
inline float32 MinFloat32(float32 a, float32 b) {
	return a < b ? a : b;
}
inline float32 MaxFloat32(float32 a, float32 b) {
	return a > b ? a : b;
}
inline float32 ClampFloat32(float32 a, float32 min, float32 max) {
	return MinFloat32(MaxFloat32(a, min), max);
}
// TODO quick and dirty round implementation
inline int RoundFloat32Fast(float32 a) {
	if (a < 0.0) {
		return (int)(a - 0.5);
	}
	else {
		return (int)(a + 0.5);
	}
}

// ========== MATH TYPES ==========

union Vec2
{
	const static Vec2 zero;
	const static Vec2 one;
	const static Vec2 unitX;
	const static Vec2 unitY;

	struct
	{
		float32 x, y;
	};
	float32 e[2];
};

union Vec2Int
{
	const static Vec2Int zero;
	const static Vec2Int unitX;
	const static Vec2Int unitY;

	struct
	{
		int x, y;
	};
	int e[2];
};

union Vec3
{
	const static Vec3 zero;
	const static Vec3 one;
	const static Vec3 unitX;
	const static Vec3 unitY;
	const static Vec3 unitZ;

	struct
	{
		float32 x, y, z;
	};
	struct
	{
		float32 r, g, b;
	};
	float32 e[3];
};

union Vec4
{
	const static Vec4 zero;
	const static Vec4 one;
	const static Vec4 black;
	const static Vec4 red;
	const static Vec4 green;
	const static Vec4 blue;

	struct
	{
		float32 x, y, z, w;
	};
	struct
	{
		float32 r, g, b, a;
	};
	float32 e[4];
};

// Column-major 4x4 matrix (columns stored contiguously)
// ORDER IS OPPOSITE OF NORMAL MATH
/*
| e[0][0]  e[1][0]  e[2][0]  e[3][0] |
| e[0][1]  e[1][1]  e[2][1]  e[3][1] |
| e[0][2]  e[1][2]  e[2][2]  e[3][2] |
| e[0][3]  e[1][3]  e[2][3]  e[3][3] |
*/
struct Mat4
{
	const static Mat4 zero;
	const static Mat4 one;

	float32 e[4][4];
};

// Should always be unit quaternions
struct Quat
{
	const static Quat one;

	// You should NOT be changing
	// these values manually
	float32 x, y, z, w;
};

inline float32 Lerp(float32 a, float32 b, float32 t)
{
	return a + (b - a) * t;
}

// ========== OPERATORS & FUNCTIONS ==========

// -------------------- Vec2 --------------------
const Vec2 Vec2::zero = {
	0.0f, 0.0f
};
const Vec2 Vec2::one = {
	1.0f, 1.0f
};
const Vec2 Vec2::unitX = {
	1.0f, 0.0f
};
const Vec2 Vec2::unitY = {
	0.0f, 1.0f
};

inline Vec2Int ToVec2Int(Vec2 v)
{
	return Vec2Int { (int)v.x, (int)v.y };
}

inline Vec2 operator-(Vec2 v)
{
	Vec2 result;
	result.x = -v.x;
	result.y = -v.y;
	return result;
}

inline Vec2 operator+(Vec2 v1, Vec2 v2)
{
	Vec2 result;
	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	return result;
}
inline Vec2& operator+=(Vec2& v1, Vec2 v2)
{
	v1 = v1 + v2;
	return v1;
}

inline Vec2 operator-(Vec2 v1, Vec2 v2)
{
	Vec2 result;
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	return result;
}
inline Vec2& operator-=(Vec2& v1, Vec2 v2)
{
	v1 = v1 - v2;
	return v1;
}

inline Vec2 operator*(float32 s, Vec2 v)
{
	Vec2 result;
	result.x = s * v.x;
	result.y = s * v.y;
	return result;
}
inline Vec2 operator*(Vec2 v, float32 s)
{
	return s * v;
}
inline Vec2& operator*=(Vec2& v, float32 s)
{
	v = s * v;
	return v;
}

inline Vec2 operator/(Vec2 v, float32 s)
{
	Vec2 result;
	result.x = v.x / s;
	result.y = v.y / s;
	return result;
}
inline Vec2& operator/=(Vec2& v, float32 s)
{
	v = v / s;
	return v;
}

inline bool operator==(const Vec2& v1, const Vec2& v2)
{
	return v1.x == v2.x && v1.y == v2.y;
}

inline Vec2 Lerp(Vec2 v1, Vec2 v2, float t)
{
	Vec2 result = {
		Lerp(v1.x, v2.x, t),
		Lerp(v1.y, v2.y, t)
	};
	return result;
}

inline float32 Dot(Vec2 v1, Vec2 v2)
{
	return v1.x * v2.x + v1.y * v2.y;
}

inline float32 MagSq(Vec2 v)
{
	return v.x*v.x + v.y*v.y;
}
inline float32 Mag(Vec2 v)
{
	return sqrtf(v.x*v.x + v.y*v.y);
}
inline Vec2 Normalize(Vec2 v)
{
	return v / Mag(v);
}

// ------------------ Vec2Int -------------------
const Vec2Int Vec2Int::zero = {
	0, 0
};
const Vec2Int Vec2Int::unitX = {
	1, 0
};
const Vec2Int Vec2Int::unitY = {
	0, 1
};

inline Vec2 ToVec2(Vec2Int v)
{
	return Vec2 { (float32)v.x, (float32)v.y };
}

inline Vec2Int operator-(Vec2Int v)
{
	Vec2Int result;
	result.x = -v.x;
	result.y = -v.y;
	return result;
}

inline Vec2Int operator+(Vec2Int v1, Vec2Int v2)
{
	Vec2Int result;
	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	return result;
}
inline Vec2Int& operator+=(Vec2Int& v1, Vec2Int v2)
{
	v1 = v1 + v2;
	return v1;
}

inline Vec2Int operator-(Vec2Int v1, Vec2Int v2)
{
	Vec2Int result;
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	return result;
}
inline Vec2Int& operator-=(Vec2Int& v1, Vec2Int v2)
{
	v1 = v1 - v2;
	return v1;
}

inline Vec2Int operator*(int s, Vec2Int v)
{
	Vec2Int result;
	result.x = s * v.x;
	result.y = s * v.y;
	return result;
}
inline Vec2Int operator*(Vec2Int v, int s)
{
	return s * v;
}
inline Vec2Int& operator*=(Vec2Int& v, int s)
{
	v = s * v;
	return v;
}

inline Vec2Int operator/(Vec2Int v, int s)
{
	Vec2Int result;
	result.x = v.x / s;
	result.y = v.y / s;
	return result;
}
inline Vec2Int& operator/=(Vec2Int& v, int s)
{
	v = v / s;
	return v;
}

inline Vec2Int operator*(float32 s, Vec2Int v)
{
	Vec2Int result;
	result.x = (int)(s * (float32)v.x);
	result.y = (int)(s * (float32)v.y);
	return result;
}
inline Vec2Int operator*(Vec2Int v, float32 s)
{
	return s * v;
}
inline Vec2Int& operator*=(Vec2Int& v, float32 s)
{
	v = s * v;
	return v;
}

inline bool operator==(const Vec2Int& v1, const Vec2Int& v2)
{
	return v1.x == v2.x && v1.y == v2.y;
}

inline bool operator!=(const Vec2Int& v1, const Vec2Int& v2)
{
	return v1.x != v2.x || v1.y != v2.y;
}

inline int MagSq(Vec2Int v)
{
	return v.x*v.x + v.y*v.y;
}
inline int Mag(Vec2Int v)
{
	return (int)sqrtf((float32)v.x*v.x + v.y*v.y);
}

// -------------------- Vec3 --------------------
const Vec3 Vec3::zero = {
	0.0f, 0.0f, 0.0f
};
const Vec3 Vec3::one = {
	1.0f, 1.0f, 1.0f
};
const Vec3 Vec3::unitX = {
	1.0f, 0.0f, 0.0f
};
const Vec3 Vec3::unitY = {
	0.0f, 1.0f, 0.0f
};
const Vec3 Vec3::unitZ = {
	0.0f, 0.0f, 1.0f
};

inline Vec2 ToVec2(Vec3 v)
{
	Vec2 result;
	result.x = v.x;
	result.y = v.y;
	return result;
}
inline Vec4 ToVec4(Vec3 v, float32 w)
{
	Vec4 result;
	result.x = v.x;
	result.y = v.y;
	result.z = v.z;
	result.w = w;
	return result;
}

inline Vec3 operator-(Vec3 v)
{
	Vec3 result;
	result.x = -v.x;
	result.y = -v.y;
	result.z = -v.z;
	return result;
}

inline Vec3 operator+(Vec3 v1, Vec3 v2)
{
	Vec3 result;
	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	result.z = v1.z + v2.z;
	return result;
}
inline Vec3& operator+=(Vec3& v1, Vec3 v2)
{
	v1 = v1 + v2;
	return v1;
}

inline Vec3 operator-(Vec3 v1, Vec3 v2)
{
	Vec3 result;
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	result.z = v1.z - v2.z;
	return result;
}
inline Vec3& operator-=(Vec3& v1, Vec3 v2)
{
	v1 = v1 - v2;
	return v1;
}

inline Vec3 operator*(float32 s, Vec3 v)
{
	Vec3 result;
	result.x = s * v.x;
	result.y = s * v.y;
	result.z = s * v.z;
	return result;
}
inline Vec3 operator*(Vec3 v, float32 s)
{
	return s * v;
}
inline Vec3& operator*=(Vec3& v, float32 s)
{
	v = s * v;
	return v;
}

inline Vec3 operator/(Vec3 v, float32 s)
{
	Vec3 result;
	result.x = v.x / s;
	result.y = v.y / s;
	result.z = v.z / s;
	return result;
}
inline Vec3& operator/=(Vec3& v, float32 s)
{
	v = v / s;
	return v;
}

inline bool operator==(const Vec3& v1, const Vec3& v2)
{
	return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}

inline Vec3 Lerp(Vec3 v1, Vec3 v2, float t)
{
	Vec3 result = {
		Lerp(v1.x, v2.x, t),
		Lerp(v1.y, v2.y, t),
		Lerp(v1.z, v2.z, t)
	};
	return result;
}

inline float32 Dot(Vec3 v1, Vec3 v2)
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}
inline Vec3 Cross(Vec3 v1, Vec3 v2)
{
	return Vec3 {
		v1.y * v2.z - v1.z * v2.y,
		v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x
	};
}

inline float32 MagSq(Vec3 v)
{
	return v.x*v.x + v.y*v.y + v.z*v.z;
}
inline float32 Mag(Vec3 v)
{
	return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}
inline Vec3 Normalize(Vec3 v)
{
	return v / Mag(v);
}

// -------------------- Vec4 --------------------
const Vec4 Vec4::zero = {
	0.0f, 0.0f, 0.0f, 0.0f
};
const Vec4 Vec4::one = {
	1.0f, 1.0f, 1.0f, 1.0f
};
const Vec4 Vec4::black = {
	0.0f, 0.0f, 0.0f, 1.0f
};
const Vec4 Vec4::red = {
	1.0f, 0.0f, 0.0f, 1.0f
};
const Vec4 Vec4::green = {
	0.0f, 1.0f, 0.0f, 1.0f
};
const Vec4 Vec4::blue = {
	0.0f, 0.0f, 1.0f, 1.0f
};

inline Vec4 operator-(Vec4 v)
{
	Vec4 result;
	result.x = -v.x;
	result.y = -v.y;
	result.z = -v.z;
	result.w = -v.w;
	return result;
}

inline Vec4 operator+(Vec4 v1, Vec4 v2)
{
	Vec4 result;
	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	result.z = v1.z + v2.z;
	result.w = v1.w + v2.w;
	return result;
}
inline Vec4& operator+=(Vec4& v1, Vec4 v2)
{
	v1 = v1 + v2;
	return v1;
}

inline Vec4 operator-(Vec4 v1, Vec4 v2)
{
	Vec4 result;
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	result.z = v1.z - v2.z;
	result.w = v1.w - v2.w;
	return result;
}
inline Vec4& operator-=(Vec4& v1, Vec4 v2)
{
	v1 = v1 - v2;
	return v1;
}

inline Vec4 operator*(float32 s, Vec4 v)
{
	Vec4 result;
	result.x = s * v.x;
	result.y = s * v.y;
	result.z = s * v.z;
	result.w = s * v.w;
	return result;
}
inline Vec4 operator*(Vec4 v, float32 s)
{
	return s * v;
}
inline Vec4& operator*=(Vec4& v, float32 s)
{
	v = s * v;
	return v;
}

inline Vec4 operator/(Vec4 v, float32 s)
{
	Vec4 result;
	result.x = v.x / s;
	result.y = v.y / s;
	result.z = v.z / s;
	result.w = v.w / s;
	return result;
}
inline Vec4& operator/=(Vec4& v, float32 s)
{
	v = v / s;
	return v;
}

inline bool operator==(const Vec4& v1, const Vec4& v2)
{
	return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z && v1.w == v2.w;
}

inline Vec4 Lerp(Vec4 v1, Vec4 v2, float t)
{
	Vec4 result = {
		Lerp(v1.x, v2.x, t),
		Lerp(v1.y, v2.y, t),
		Lerp(v1.z, v2.z, t),
		Lerp(v1.w, v2.w, t)
	};
	return result;
}

// -------------------- Mat4 --------------------
// TODO these functions might be better off not inlined
// though they are used very infrequently
const Mat4 Mat4::one =
{
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
};
const Mat4 Mat4::zero =
{
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
};

inline Mat4 operator+(Mat4 m1, Mat4 m2)
{
	Mat4 result;

	// TODO this will hopefully get unrolled by the compiler
	for (int col = 0; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			result.e[col][row] = m1.e[col][row] + m2.e[col][row];
		}
	}

	return result;
}
inline Mat4& operator+=(Mat4& m1, Mat4 m2)
{
	m1 = m1 + m2;
	return m1;
}

inline Mat4 operator-(Mat4 m1, Mat4 m2)
{
	Mat4 result;

	// TODO this will hopefully get unrolled by the compiler
	for (int col = 0; col < 4; col++)
		for (int row = 0; row < 4; row++)
			result.e[col][row] = m1.e[col][row] - m2.e[col][row];

	return result;
}
inline Mat4& operator-=(Mat4& m1, Mat4 m2)
{
	m1 = m1 - m2;
	return m1;
}

inline Mat4 operator*(Mat4 m1, Mat4 m2)
{
	Mat4 result = Mat4::zero;

	// I really thought hard about this
	// Make it as cache-efficient as possible
	// Probably doesn't matter at all...
	for (int colM2 = 0; colM2 < 4; colM2++) {
		for (int colM1 = 0; colM1 < 4; colM1++) {
			for (int rowM1 = 0; rowM1 < 4; rowM1++) {
				result.e[colM2][rowM1] += 
					m2.e[colM2][colM1] * m1.e[colM1][rowM1];
			}
		}
	}

	return result;
}

inline Vec4 operator*(Mat4 m, Vec4 v)
{
	Vec4 result = Vec4::zero;

	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			result.e[row] += m.e[col][row] * v.e[col];
		}
	}

	return result;
}

Mat4 Translate(Vec3 v)
{
	Mat4 result = Mat4::one;

	result.e[3][0] = v.x;
	result.e[3][1] = v.y;
	result.e[3][2] = v.z;

	return result;
}

Mat4 Scale(float32 s)
{
	Mat4 result = {};

	result.e[0][0] = s;
	result.e[1][1] = s;
	result.e[2][2] = s;
	result.e[3][3] = 1.0f;

	return result;
}

Mat4 Scale(Vec3 v)
{
	Mat4 result = {};

	result.e[0][0] = v.x;
	result.e[1][1] = v.y;
	result.e[2][2] = v.z;
	result.e[3][3] = 1.0f;

	return result;
}

Mat4 Rotate(Vec3 r)
{
	Mat4 rx = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, cosf(r.x), -sinf(r.x), 0.0f,
		0.0f, sinf(r.x), cosf(r.x), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	Mat4 ry = {
		cosf(r.y), 0.0f, sinf(r.y), 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		-sinf(r.y), 0.0f, cosf(r.y), 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};
	Mat4 rz = {
		cosf(r.z), -sinf(r.z), 0.0f, 0.0f,
		sinf(r.z), cosf(r.z), 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	// Order: <- yaw <- pitch <- roll <-
	return rz * ry * rx;
}

Mat4 Projection(float32 fov, float32 aspect,
	float32 nearZ, float32 farZ)
{
	float32 degToRad = PI_F / 180.0f;
	float32 yScale = 1.0f / tanf(degToRad * fov / 2.0f);
	float32 xScale = yScale / aspect;
	float32 nearMinusFar = nearZ - farZ;
	Mat4 proj = {
		xScale, 0.0f, 0.0f, 0.0f,
		0.0f, yScale, 0.0f, 0.0f,
		0.0f, 0.0f, (farZ + nearZ) / nearMinusFar, -1.0f,
		0.0f, 0.0f, 2.0f*farZ*nearZ / nearMinusFar, 0.0f
	};

	return proj;
}

// -------------------- Quat --------------------
const Quat Quat::one = {
	0.0f, 0.0f, 0.0f, 1.0f
};

// Compounds two rotations q1 and q2 (like matrices: q2 first, then q1)
inline Quat operator*(Quat q1, Quat q2)
{
	Quat result;
	result.x = q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y;
	result.y = q1.w*q2.y + q1.y*q2.w + q1.z*q2.x - q1.x*q2.z;
	result.z = q1.w*q2.z + q1.z*q2.w + q1.x*q2.y - q1.y*q2.x;
	result.w = q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z;
	return result;
}

inline float32 MagSq(Quat q)
{
	return q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
}
inline float32 Mag(Quat q)
{
	return sqrtf(MagSq(q));
}
inline Quat Normalize(Quat q)
{
	Quat result = q;
	float32 mag = Mag(q);
	result.x /= mag;
	result.y /= mag;
	result.z /= mag;
	result.w /= mag;
	return result;
}

// Returns a new quaternion qInv such that q * qInv = Quat::one
inline Quat Inverse(Quat q)
{
	Quat inv;
	inv.x = -q.x;
	inv.y = -q.y;
	inv.z = -q.z;
	inv.w = q.w;
	return inv;
}

// Rotates vector v by quaternion q
inline Vec3 operator*(Quat q, Vec3 v)
{
	// Treat v as a quaternion with w = 0
	Quat vQuat = { v.x, v.y, v.z, 0.0f };
	/*qv.x = q.w*v.x + q.y*v.z - q.z*v.y;
	qv.y = q.w*v.y + q.z*v.x - q.x*v.z;
	qv.z = q.w*v.z + q.x*v.y - q.y*v.x;
	qv.w = -q.x*v.x - q.y*v.y - q.z*v.z;*/
	Quat qv = q * vQuat;

	Quat qInv = Inverse(q);
	Quat qvqInv = qv * qInv;

	Vec3 result = { qvqInv.x, qvqInv.y, qvqInv.z };
	return result;
}

// Axis should be a unit vector
Quat QuatFromAngleUnitAxis(float32 angle, Vec3 axis)
{
	Quat quat;
	quat.x = axis.x * sinf(angle / 2.0f);
	quat.y = axis.y * sinf(angle / 2.0f);
	quat.z = axis.z * sinf(angle / 2.0f);
	quat.w = cosf(angle / 2.0f);
	return quat;
}

// TODO not working
Quat QuatFromEulerAngles(Vec3 euler)
{
	/*float32 cosYaw = cosf(euler.z * 0.5f);
	float32 sinYaw = sinf(euler.z * 0.5f);
	float32 cosRoll = cosf(euler.x * 0.5f);
	float32 sinRoll = sinf(euler.x * 0.5f);
	float32 cosPitch = cosf(euler.y * 0.5f);
	float32 sinPitch = sinf(euler.y * 0.5f);

	Quat q;
	q.x = cosYaw*sinRoll*cosPitch - sinYaw*cosRoll*sinPitch;
	q.y = cosYaw*cosRoll*sinPitch + sinYaw*sinRoll*cosPitch;
	q.z = sinYaw*cosRoll*cosPitch - cosYaw*sinRoll*sinPitch;
	q.w = cosYaw*cosRoll*cosPitch + sinYaw*sinRoll*sinPitch;
	return q;*/
	Quat quat = QuatFromAngleUnitAxis(euler.x, Vec3 { 1.0f, 0.0f, 0.0f });
	quat = QuatFromAngleUnitAxis(euler.y, Vec3 { 0.0f, 1.0f, 0.0f }) * quat;
	quat = QuatFromAngleUnitAxis(euler.z, Vec3 { 0.0f, 0.0f, 1.0f }) * quat;
	return quat;
}

Quat QuatRotBetweenVectors(Vec3 v1, Vec3 v2)
{
	Vec3 axis = Cross(v1, v2);
	float angle = asinf(Mag(axis) / (Mag(v1) * Mag(v2)));
	if (axis == Vec3::zero) {
		return Quat::one;
	}

	return QuatFromAngleUnitAxis(angle, Normalize(axis));
}

// q, as always, must be a unit quaternion
Mat4 UnitQuatToMat4(Quat q)
{
	Mat4 result;
	result.e[0][0] = 1.0f - 2.0f * (q.y*q.y + q.z*q.z);
	result.e[0][1] = 2.0f * (q.x*q.y + q.w*q.z);
	result.e[0][2] = 2.0f * (q.x*q.z - q.w*q.y);
	result.e[0][3] = 0.0f;

	result.e[1][0] = 2.0f * (q.x*q.y - q.w*q.z);
	result.e[1][1] = 1.0f - 2.0f * (q.x*q.x + q.z*q.z);
	result.e[1][2] = 2.0f * (q.y*q.z + q.w*q.x);
	result.e[1][3] = 0.0f;

	result.e[2][0] = 2.0f * (q.x*q.z + q.w*q.y);
	result.e[2][1] = 2.0f * (q.y*q.z - q.w*q.x);
	result.e[2][2] = 1.0f - 2.0f * (q.x*q.x + q.y*q.y);
	result.e[2][3] = 0.0f;

	result.e[3][0] = 0.0f;
	result.e[3][1] = 0.0f;
	result.e[3][2] = 0.0f;
	result.e[3][3] = 1.0f;
	return result;
}
