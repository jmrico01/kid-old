#pragma once

#define FLOOR_COLLIDER_MAX_VERTICES 128

struct FloorCollider
{
    int numVertices;
    Vec2 vertices[FLOOR_COLLIDER_MAX_VERTICES];
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
