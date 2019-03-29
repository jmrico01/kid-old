#pragma once

#include "km_math.h"

#define FLOOR_PRECOMPUTED_POINTS_MAX 32768
#define FLOOR_PRECOMPUTED_STEP_LENGTH 0.05f
#define FLOOR_COLLIDER_MAX_VERTICES 128
#define LINE_COLLIDER_MAX_VERTICES 32

struct FloorSampleVertex
{
	Vec2 pos;
	Vec2 normal;
};

struct FloorCollider
{
	FixedArray<Vec2, FLOOR_COLLIDER_MAX_VERTICES> line;

	// Precomputed fields
	float32 length;
	FixedArray<FloorSampleVertex, FLOOR_PRECOMPUTED_POINTS_MAX> sampleVertices;

	void GetInfoFromCoordX(float32 coordX, Vec2* outFloorPos, Vec2* outNormal) const;
	Vec2 GetWorldPosFromCoords(Vec2 coords) const;

	void GetInfoFromCoordXSlow(float32 coordX, Vec2* outFloorPos, Vec2* outNormal) const;
	void PrecomputeSampleVerticesFromLine();
};

struct LineCollider
{
	FixedArray<Vec2, LINE_COLLIDER_MAX_VERTICES> line; // TODO use this
	int numVertices;
	Vec2 vertices[LINE_COLLIDER_MAX_VERTICES];
};

struct LineColliderIntersect
{
	Vec2 pos;
	const LineCollider* collider;
};

float32 GetFloorLength(const FloorCollider& floorCollider);
void GetFloorInfo(const FloorCollider& floorCollider, float32 tX,
	Vec2* outFloorPos, Vec2* outTangent, Vec2* outNormal);
Vec2 FloorCoordsToWorldPos(const FloorCollider& floorCollider, Vec2 coords);

void GetLineColliderIntersections(const LineCollider lineColliders[], int numLineColliders,
	Vec2 pos, Vec2 deltaPos, float32 movementMargin,
	LineColliderIntersect outIntersects[], int* outNumIntersects);

bool32 GetLineColliderCoordYFromFloorCoordX(const LineCollider& lineCollider,
	const FloorCollider& floorCollider, float32 coordX,
	float32* outHeight);