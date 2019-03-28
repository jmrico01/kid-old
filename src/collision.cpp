#include "collision.h"

#include "km_debug.h"

internal Vec2 GetQuadraticBezierPoint(Vec2 v1, Vec2 v2, Vec2 v3, float32 t)
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

internal float32 Cross2D(Vec2 v1, Vec2 v2)
{
	return v1.x * v2.y - v1.y * v2.x;
}

// Generalization of this solution to line segment intersection:
// https://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
bool32 RayIntersectionCoefficients(
    Vec2 line1Start, Vec2 line1Dir,
    Vec2 line2Start, Vec2 line2Dir,
    float32* t1, float32* t2)
{
    Vec2 startDiff = line2Start - line1Start;
    float32 crossDirs12 = Cross2D(line1Dir, line2Dir);
    float32 crossDiffDir1 = Cross2D(startDiff, line1Dir);

    if (crossDirs12 == 0.0f && crossDiffDir1 == 0.0f) {
        // collinear
        float32 magDir1 = MagSq(line1Dir);
        float32 dotDirs = Dot(line1Dir, line2Dir);
        *t1 = Dot(startDiff, line1Dir) / magDir1;
        *t2 = *t1 + dotDirs / magDir1;
        return true;
    }
    else if (crossDirs12 == 0.0f) {
        // parallel
        return false;
    }
    else {
        *t1 = Cross2D(startDiff, line2Dir) / crossDirs12;
        *t2 = crossDiffDir1 / crossDirs12;
        return true;
    }
}

bool32 LineSegmentIntersection(
    Vec2 line1Start, Vec2 line1Dir,
    Vec2 line2Start, Vec2 line2Dir,
    Vec2* outIntersect)
{
    float32 t1, t2;
    bool32 rayIntersect = RayIntersectionCoefficients(line1Start, line1Dir, line2Start, line2Dir,
        &t1, &t2);
    if (!rayIntersect) {
        return false;
    }
    else if (0.0f <= t1 && t1 <= 1.0f && 0.0f <= t2 && t2 <= 1.0f) {
        *outIntersect = line1Start + line1Dir * t1;
        return true;
    }
    else {
        return false;
    }
}

void GetLineColliderIntersections(const LineCollider lineColliders[], int numLineColliders,
	Vec2 pos, Vec2 deltaPos, float32 movementMargin,
	LineColliderIntersect outIntersects[], int* outNumIntersects)
{
	*outNumIntersects = 0;
	float32 deltaPosMag = Mag(deltaPos);
	if (deltaPosMag == 0.0f) {
		return;
	}
	Vec2 dir = deltaPos / deltaPosMag;
	Vec2 playerDir = deltaPos + dir * movementMargin;

	for (int c = 0; c < numLineColliders; c++) {
		DEBUG_ASSERT(lineColliders[c].numVertices >= 2);
		Vec2 vertPrev = lineColliders[c].vertices[0];
		for (int v = 1; v < lineColliders[c].numVertices; v++) {
			Vec2 vert = lineColliders[c].vertices[v];
			Vec2 intersectPoint;
			if (LineSegmentIntersection(pos, playerDir, vertPrev, vert - vertPrev,
			&intersectPoint)) {
				int intersectInd = *outNumIntersects;
				outIntersects[intersectInd].pos = intersectPoint;
				outIntersects[intersectInd].collider = &lineColliders[c];
				(*outNumIntersects)++;
				break;
			}
			vertPrev = vert;
		}
	}
}

bool32 GetLineColliderCoordYFromFloorCoordX(const LineCollider& lineCollider,
    const FloorCollider& floorCollider, float32 coordX,
    float32* outHeight)
{
    Vec2 floorPos, floorTangent, floorNormal;
    GetFloorInfo(floorCollider, coordX, &floorPos, &floorTangent, &floorNormal);

    DEBUG_ASSERT(lineCollider.numVertices >= 2);
    Vec2 vertPrev = lineCollider.vertices[0];
    for (int v = 1; v < lineCollider.numVertices; v++) {
        Vec2 vert = lineCollider.vertices[v];
        float32 t1, t2;
        bool32 rayIntersect = RayIntersectionCoefficients(
            floorPos, floorNormal, vertPrev, vert - vertPrev,
            &t1, &t2);
        if (rayIntersect && t1 >= 0.0f && 0.0f <= t2 && t2 <= 1.0f) {
            *outHeight = t1;
            return true;
        }
        vertPrev = vert;
    }

    return false;
}