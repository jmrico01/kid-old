#pragma once

#define FLOOR_COLLIDER_MAX_VERTICES 128
#define LINE_COLLIDER_MAX_VERTICES 32

struct FloorCollider
{
    int numVertices;
    Vec2 vertices[FLOOR_COLLIDER_MAX_VERTICES];
};

struct LineCollider
{
	int numVertices;
	Vec2 vertices[LINE_COLLIDER_MAX_VERTICES];
};

struct LineColliderIntersect
{
	Vec2 pos;
	const LineCollider* collider;
};

struct WallCollider
{
    Vec2 bottomPos;
    float32 height;
};

float32 GetFloorLength(const FloorCollider& floorCollider);
void GetFloorInfo(const FloorCollider& floorCollider, float32 tX,
	Vec2* outFloorPos, Vec2* outTangent, Vec2* outNormal);
Vec2 FloorCoordsToWorldPos(const FloorCollider& floorCollider, Vec2 coords);

void GetLineColliderIntersections(const LineCollider lineColliders[], int numLineColliders,
	Vec2 pos, Vec2 deltaPos, float32 movementMargin,
	LineColliderIntersect outIntersects[], int* outNumIntersects);