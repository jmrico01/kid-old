#include "collision.h"

#include "km_debug.h"

void FloorCollider::GetInfoFromCoordX(float32 coordX, Vec2* outPos, Vec2* outNormal) const
{
	float32 indFloat = coordX / FLOOR_PRECOMPUTED_STEP_LENGTH;
	int ind1 = ClampInt((int)indFloat, 0, sampleVertices.size);
	int ind2 = ClampInt(ind1 + 1,      0, sampleVertices.size);
	FloorSampleVertex sampleVertex1 = sampleVertices.array[ind1];
	FloorSampleVertex sampleVertex2 = sampleVertices.array[ind2];
	float32 lerpT = indFloat - (float32)ind1;
	*outPos = Lerp(sampleVertex1.pos, sampleVertex2.pos, lerpT);
	*outNormal = Lerp(sampleVertex1.normal, sampleVertex2.normal, lerpT);
}

Vec2 FloorCollider::GetWorldPosFromCoords(Vec2 coords) const
{
	Vec2 floorPos, floorNormal;
	GetInfoFromCoordX(coords.x, &floorPos, &floorNormal);
	return floorPos + floorNormal * coords.y;
}

void FloorCollider::PrecomputeSampleVerticesFromLine()
{
	float32 lineLength = 0.0f;
	for (uint32 i = 1; i < line.size; i++) {
		lineLength += Mag(line.array[i] - line.array[i - 1]);
	}
	length = lineLength;

	int precomputedPoints = (int)(lineLength / FLOOR_PRECOMPUTED_STEP_LENGTH);
	DEBUG_ASSERT(precomputedPoints <= FLOOR_PRECOMPUTED_POINTS_MAX);
	sampleVertices.size = precomputedPoints;
	for (int i = 0; i < precomputedPoints; i++) {
		float32 coordX = i * FLOOR_PRECOMPUTED_STEP_LENGTH;
		FloorSampleVertex sampleVertex;
		GetInfoFromCoordXSlow(coordX, &sampleVertex.pos, &sampleVertex.normal);
		sampleVertices.array[i] = sampleVertex;
	}
}

internal Vec2 GetQuadraticBezierPoint(Vec2 v1, Vec2 v2, Vec2 v3, float32 t)
{
	float32 oneMinusT = 1.0f - t;
	return oneMinusT * oneMinusT * v1 + 2.0f * oneMinusT * t * v2 + t * t * v3;
}

void FloorCollider::GetInfoFromCoordXSlow(float32 coordX, Vec2* outFloorPos, Vec2* outNormal) const
{
	const int EDGE_NEIGHBORS = 3;
	
	*outFloorPos = Vec2::zero;
	*outNormal = Vec2::unitY;

	float32 t = 0.0f;
	for (uint32 i = 1; i < line.size; i++) {
		Vec2 edge = line.array[i] - line.array[i - 1];
		float32 edgeLength = Mag(edge);
		if (t + edgeLength >= coordX) {
			float32 tEdge = (coordX - t) / edgeLength;
			Vec2 sumTangents = Vec2::zero;
			Vec2 sumNormals = Vec2::zero;
			for (int n = -EDGE_NEIGHBORS; n <= EDGE_NEIGHBORS; n++) {
				float32 edgeWeight = EDGE_NEIGHBORS - AbsFloat32(n - (tEdge - 0.5f));
				edgeWeight = MaxFloat32(edgeWeight, 0.0f);
				int ind1 = ClampInt(i + n - 1, 0, line.size - 1);
				int ind2 = ClampInt(i + n,     1, line.size);
				Vec2 edgeTangent = Normalize(line.array[ind2] - line.array[ind1]);
				sumTangents += edgeTangent * edgeWeight;
				sumNormals += Vec2 { -edgeTangent.y, edgeTangent.x } * edgeWeight;
			}
			int indPrev = ClampInt(i - 1, 1, line.size);
			int indNext = ClampInt(i + 1, 1, line.size);
			Vec2 tangentPrev = Normalize(line.array[indPrev] - line.array[indPrev - 1]);
			Vec2 tangentNext = Normalize(line.array[indNext] - line.array[indNext - 1]);
			Vec2 bezierMid = (line.array[i - 1] + tangentPrev * edgeLength / 2.0f
				+ line.array[i] - tangentNext * edgeLength / 2.0f) / 2.0f;
			*outFloorPos = GetQuadraticBezierPoint(line.array[i - 1], bezierMid, line.array[i],
				tEdge);
			*outNormal = Normalize(sumNormals);
			return;
		}

		t += edgeLength;
	}
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
		DEBUG_ASSERT(lineColliders[c].line.size >= 2);
		Vec2 vertPrev = lineColliders[c].line.array[0];
		for (uint32 v = 1; v < lineColliders[c].line.size; v++) {
			Vec2 vert = lineColliders[c].line.array[v];
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
    Vec2 floorPos, floorNormal;
    floorCollider.GetInfoFromCoordX(coordX, &floorPos, &floorNormal);

    DEBUG_ASSERT(lineCollider.line.size >= 2);
    Vec2 vertPrev = lineCollider.line.array[0];
    for (uint32 v = 1; v < lineCollider.line.size; v++) {
        Vec2 vert = lineCollider.line.array[v];
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