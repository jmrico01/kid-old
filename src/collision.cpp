#include "collision.h"

#if 0
struct FloorColliderIntersect
{
	Vec2 pos;
	const FloorCollider* collider;
};

bool GetFloorColliderHeight(const FloorCollider* floorCollider, Vec2 refPos, float32* outHeight)
{
	DEBUG_ASSERT(floorCollider->numVertices > 1);

	int numVertices = floorCollider->numVertices;
	Vec2 vertPrev = floorCollider->vertices[0];
	for (int i = 1; i < numVertices; i++) {
		Vec2 vert = floorCollider->vertices[i];
		if (vertPrev.x <= refPos.x && vert.x >= refPos.x) {
			float32 t = (refPos.x - vertPrev.x) / (vert.x - vertPrev.x);
			*outHeight = vertPrev.y + t * (vert.y - vertPrev.y);
			return true;
		}

		vertPrev = vert;
	}

	if (refPos.x < floorCollider->vertices[0].x) {
		*outHeight = floorCollider->vertices[0].y;
	}
	if (refPos.x > floorCollider->vertices[floorCollider->numVertices - 1].x) {
		*outHeight = floorCollider->vertices[floorCollider->numVertices - 1].y;
	}
	return false;
}

internal inline float32 Cross2D(Vec2 v1, Vec2 v2)
{
	return v1.x * v2.y - v1.y * v2.x;
}

// Source: https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
bool32 Intersection(Vec2 line1Start, Vec2 line1Dir, Vec2 line2Start, Vec2 line2Dir,
	Vec2* outIntersect)
{
	Vec2 startDiff = line2Start - line1Start;
	float32 crossDirs12 = Cross2D(line1Dir, line2Dir);
	float32 crossDiffDir1 = Cross2D(startDiff, line1Dir);

	if (crossDirs12 == 0.0f && crossDiffDir1 == 0.0f) {
		// collinear
		float32 magDir1 = MagSq(line1Dir);
		float32 dotDirs = Dot(line1Dir, line2Dir);
		float32 t = Dot(startDiff, line1Dir) / magDir1;
		float32 tt = t + dotDirs / magDir1;
		if ((t < 0.0f && tt < 0.0f) || (t > 1.0f && tt > 1.0f)) {
			return false;
		}

		// Could return any point in interval [t, tt]
		*outIntersect = line1Start + t * line1Dir;
		return true;
	}
	else if (crossDirs12 == 0.0f) {
		// parallel
		return false;
	}
	else {
		float32 t1 = Cross2D(startDiff, line2Dir) / crossDirs12;
		float32 t2 = crossDiffDir1 / crossDirs12;

		if (0.0f <= t1 && t1 <= 1.0f && 0.0f <= t2 && t2 <= 1.0f) {
			// intersection
			*outIntersect = line1Start + t1 * line1Dir;
			return true;
		}
		else {
			// no intersection
			return false;
		}
	}
}

void GetFloorColliderIntersections(const FloorCollider floorColliders[], int numFloorColliders,
	Vec2 pos, Vec2 deltaPos, float32 movementMargin,
	FloorColliderIntersect outIntersects[], int* outNumIntersects)
{
	*outNumIntersects = 0;
	float32 deltaPosMag = Mag(deltaPos);
	if (deltaPosMag == 0.0f) {
		return;
	}
	Vec2 dir = deltaPos / deltaPosMag;
	Vec2 playerDir = deltaPos + dir * movementMargin;

	for (int c = 0; c < numFloorColliders; c++) {
		DEBUG_ASSERT(floorColliders[c].numVertices >= 2);
		Vec2 vertPrev = floorColliders[c].vertices[0];
		for (int v = 1; v < floorColliders[c].numVertices; v++) {
			Vec2 vert = floorColliders[c].vertices[v];
			Vec2 intersectPoint;
			if (Intersection(pos, playerDir, vertPrev, vert - vertPrev, &intersectPoint)) {
				int intersectInd = *outNumIntersects;
				outIntersects[intersectInd].pos = intersectPoint;
				outIntersects[intersectInd].collider = &floorColliders[c];
				(*outNumIntersects)++;
				break;
			}
			vertPrev = vert;
		}
	}
}
#endif

Vec2 GetQuadraticBezierPoint(Vec2 v1, Vec2 v2, Vec2 v3, float32 t)
{
	float32 oneMinusT = 1.0f - t;
	return oneMinusT * oneMinusT * v1 + 2.0f * oneMinusT * t * v2 + t * t * v3;
}

float32 GetFloorLength(const FloorCollider& floorCollider)
{
	float32 length = 0.0f;
	for (int i = 1; i < floorCollider.numVertices; i++) {
		length += Mag(floorCollider.vertices[i] - floorCollider.vertices[i - 1]);
	}

	return length;
}

void GetFloorInfo(const FloorCollider& floorCollider, float32 tX,
	Vec2* outFloorPos, Vec2* outTangent, Vec2* outNormal)
{
	DEBUG_ASSERT(floorCollider.numVertices >= 2);

	const int EDGE_NEIGHBORS = 3;

	*outFloorPos = Vec2::zero;
	*outTangent = Vec2::unitX;
	*outNormal = Vec2::unitY;

	float32 t = 0.0f;
	Vec2 dir = floorCollider.vertices[1] - floorCollider.vertices[0];
	for (int i = 1; i < floorCollider.numVertices; i++) {
		Vec2 edge = floorCollider.vertices[i] - floorCollider.vertices[i - 1];
		float32 edgeLength = Mag(edge);
		if (t + edgeLength >= tX) {
			float32 tEdge = (tX - t) / edgeLength;
			Vec2 sumTangents = Vec2::zero;
			Vec2 sumNormals = Vec2::zero;
			for (int n = -EDGE_NEIGHBORS; n <= EDGE_NEIGHBORS; n++) {
				float32 edgeWeight = EDGE_NEIGHBORS - AbsFloat32(n - (tEdge - 0.5f));
				edgeWeight = MaxFloat32(edgeWeight, 0.0f);
				int ind1 = ClampInt(i + n - 1, 0, floorCollider.numVertices - 1);
				int ind2 = ClampInt(i + n,     1, floorCollider.numVertices);
				Vec2 edgeTangent = Normalize(floorCollider.vertices[ind2]
					- floorCollider.vertices[ind1]);
				sumTangents += edgeTangent * edgeWeight;
				sumNormals += Vec2 { -edgeTangent.y, edgeTangent.x } * edgeWeight;
			}
			int indPrev = ClampInt(i - 1, 1, floorCollider.numVertices);
			int indNext = ClampInt(i + 1, 1, floorCollider.numVertices);
			Vec2 tangentPrev = Normalize(floorCollider.vertices[indPrev]
				- floorCollider.vertices[indPrev - 1]);
			Vec2 tangentNext = Normalize(floorCollider.vertices[indNext]
				- floorCollider.vertices[indNext - 1]);
			Vec2 bezierMid = (floorCollider.vertices[i - 1] + tangentPrev * edgeLength / 2.0f
				+ floorCollider.vertices[i] - tangentNext * edgeLength / 2.0f) / 2.0f;
			//*outFloorPos = floorCollider.vertices[i - 1] + edge * tEdge;
			*outFloorPos = GetQuadraticBezierPoint(
				floorCollider.vertices[i - 1],
				bezierMid,
				floorCollider.vertices[i],
				tEdge
			);
			*outTangent = Normalize(sumTangents);
			*outNormal = Normalize(sumNormals);
			return;
		}

		t += edgeLength;
	}
}

Vec2 FloorCoordsToWorldPos(const FloorCollider& floorCollider, Vec2 coords)
{
	Vec2 floorPos, floorTangent, floorNormal;
	GetFloorInfo(floorCollider, coords.x, &floorPos, &floorTangent, &floorNormal);
	return floorPos + floorNormal * coords.y;
}