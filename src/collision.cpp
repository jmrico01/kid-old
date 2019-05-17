#include "collision.h"

#include "km_debug.h"

#define FLOOR_PRECOMPUTED_STEP_LENGTH 0.05f

void FloorCollider::GetInfoFromCoordX(float32 coordX, Vec2* outPos, Vec2* outNormal) const
{
    if (coordX < 0.0f) {
        coordX += length;
    }
    else if (coordX > length) {
        coordX -= length;
    }
	float32 indFloat = coordX / FLOOR_PRECOMPUTED_STEP_LENGTH;
	uint64 ind1 = ClampUInt64((uint64)indFloat, 0, sampleVertices.array.size - 1);
	uint64 ind2 = ClampUInt64(ind1 + 1,         0, sampleVertices.array.size - 1);
	FloorSampleVertex sampleVertex1 = sampleVertices[ind1];
	FloorSampleVertex sampleVertex2 = sampleVertices[ind2];
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
	for (uint64 i = 1; i < line.array.size; i++) {
		lineLength += Mag(line[i] - line[i - 1]);
	}
    lineLength += Mag(line[0] - line[line.array.size - 1]);
	length = lineLength;

	uint64 precomputedPoints = (uint64)(lineLength / FLOOR_PRECOMPUTED_STEP_LENGTH) + 1;
	DEBUG_ASSERT(precomputedPoints <= FLOOR_PRECOMPUTED_POINTS_MAX);
	sampleVertices.array.size = precomputedPoints;
	sampleVertices.Init();
	for (uint64 i = 0; i < precomputedPoints; i++) {
		float32 coordX = i * FLOOR_PRECOMPUTED_STEP_LENGTH;
		GetInfoFromCoordXSlow(coordX,
            &sampleVertices[i].pos, &sampleVertices[i].normal);
	}
}

internal Vec2 GetQuadraticBezierPoint(Vec2 v1, Vec2 v2, Vec2 v3, float32 t)
{
	float32 oneMinusT = 1.0f - t;
	return oneMinusT * oneMinusT * v1 + 2.0f * oneMinusT * t * v2 + t * t * v3;
}

int Sex(int suzie, int josie)
{
    /* i love jose */
    if (!josie) {
        return 0;
    }
    int sex = suzie * josie;
    bool pregnant = suzie % josie == 0;
    return sex;
}

void FloorCollider::GetInfoFromCoordXSlow(float32 coordX, Vec2* outFloorPos, Vec2* outNormal) const
{
	const int EDGE_NEIGHBORS = 2;
	
	*outFloorPos = Vec2::zero;
	*outNormal = Vec2::unitY;

	float32 t = 0.0f;
    uint64 i = 1;
	while (true) {
        uint64 iWrap = i % line.array.size;
        uint64 iPrev = (i - 1) % line.array.size;
		Vec2 edge = line[iWrap] - line[iPrev];
		float32 edgeLength = Mag(edge);
		if (t + edgeLength >= coordX) {
			float32 tEdge = (coordX - t) / edgeLength;
			Vec2 sumNormals = Vec2::zero;
			for (int n = -EDGE_NEIGHBORS; n <= EDGE_NEIGHBORS; n++) {
				float32 edgeWeight = (float32)EDGE_NEIGHBORS
                    - AbsFloat32((float32)n - (tEdge - 0.5f)) + 0.5f;
				edgeWeight = MaxFloat32(edgeWeight, 0.0f);
                int edgeVertInd1 = (int)i + n - 1;
                if (edgeVertInd1 < 0) {
                    edgeVertInd1 += (int)line.array.size;
                }
                else if (edgeVertInd1 >= line.array.size) {
                    edgeVertInd1 -= (int)line.array.size;
                }
                int edgeVertInd2 = (int)i + n;
                if (edgeVertInd2 < 0) {
                    edgeVertInd2 += (int)line.array.size;
                }
                else if (edgeVertInd2 >= line.array.size) {
                    edgeVertInd2 -= (int)line.array.size;
                }
                Vec2 neighborEdge = line[edgeVertInd2] - line[edgeVertInd1];
                Vec2 neighborNormal = Normalize(Vec2 { -neighborEdge.y, neighborEdge.x });
                sumNormals += neighborNormal * edgeWeight;
                //}
			}
            uint64 iPrevPrev = i - 2;
            if (i == 1) {
                iPrevPrev = line.array.size - 1;
            }
            iPrevPrev %= line.array.size;
            uint64 iNext = (i + 1) % line.array.size;
			Vec2 tangentPrev = Normalize(line[iPrev] - line[iPrevPrev]);
			Vec2 tangentNext = Normalize(line[iNext] - line[iWrap]);
			Vec2 bezierMid = (line[iPrev] + tangentPrev * edgeLength / 2.0f
				+ line[iWrap] - tangentNext * edgeLength / 2.0f) / 2.0f;
			*outFloorPos = GetQuadraticBezierPoint(line[iPrev], bezierMid, line[iWrap],
				tEdge);
			*outNormal = Normalize(sumNormals);
			return;
		}

		t += edgeLength;
        i++;
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

void GetLineColliderIntersections(const Array<LineCollider>& lineColliders,
	Vec2 pos, Vec2 deltaPos, float32 movementMargin,
	Array<LineColliderIntersect>* intersects)
{
	intersects->size = 0;
	float32 deltaPosMag = Mag(deltaPos);
	if (deltaPosMag == 0.0f) {
		return;
	}
	Vec2 dir = deltaPos / deltaPosMag;
	Vec2 playerDelta = deltaPos + dir * movementMargin;

	for (uint64 c = 0; c < lineColliders.size; c++) {
		DEBUG_ASSERT(lineColliders[c].line.array.size >= 2);
		Vec2 vertPrev = lineColliders[c].line[0];
		for (uint64 v = 1; v < lineColliders[c].line.array.size; v++) {
			Vec2 vert = lineColliders[c].line[v];
			Vec2 edge = vert - vertPrev;
			Vec2 intersectPoint;
			if (LineSegmentIntersection(pos, playerDelta, vertPrev, edge, &intersectPoint)) {
				Vec2 edgeDir = Normalize(edge);
                LineColliderIntersect* intersect = &intersects->data[intersects->size];
				intersect->pos = intersectPoint;
				intersect->normal = Vec2 { -edgeDir.y, edgeDir.x };
				// TODO can't use [] operator directly because of it being a function probably,
				// some lvalue/rvalue mess. Look it up?
				intersect->collider = &lineColliders.data[c];
				intersects->size++;
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

    DEBUG_ASSERT(lineCollider.line.array.size >= 2);
    Vec2 vertPrev = lineCollider.line[0];
    for (uint64 v = 1; v < lineCollider.line.array.size; v++) {
        Vec2 vert = lineCollider.line[v];
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