#pragma once

#include <km_common/km_lib.h>
#include <km_common/km_math.h>

#define FLOOR_PRECOMPUTED_POINTS_MAX 262144
#define FLOOR_COLLIDER_MAX_VERTICES 8192
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
    
    Vec2 GetCoordsFromWorldPos(Vec2 worldPos) const;
	void GetInfoFromCoordXSlow(float32 coordX, Vec2* outFloorPos, Vec2* outNormal) const;
	void PrecomputeSampleVerticesFromLine();
};

struct LineCollider
{
	FixedArray<Vec2, LINE_COLLIDER_MAX_VERTICES> line;
};

struct LineColliderIntersect
{
	Vec2 pos;
	Vec2 normal;
	const LineCollider* collider;
};

template <uint64 S>
void GetLineColliderIntersections(const Array<LineCollider>& lineColliders, Vec2 pos, Vec2 deltaPos,
                                  float32 movementMargin, FixedArray<LineColliderIntersect, S>* outIntersects);

bool GetLineColliderCoordYFromFloorCoordX(const LineCollider& lineCollider,
                                          const FloorCollider& floorCollider, float32 coordX,
                                          float32* outHeight);