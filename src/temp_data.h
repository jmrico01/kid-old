#pragma once

#include "collision.h"
#include "km_lib.h"
#include "km_math.h"

#define FIXED_ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

const Vec2 FLOOR_VERTEX_STORE[10][FLOOR_COLLIDER_MAX_VERTICES] = {
{ // 0
    Vec2 { -46.04f, 14.92f },
    Vec2 { -43.18f, 14.92f },
    Vec2 { -40.29f, 14.92f },
    Vec2 { -37.08f, 14.90f },
    Vec2 { -33.53f, 14.88f },
    Vec2 { -32.38f, 15.01f },
    Vec2 { -31.34f, 15.20f },
    Vec2 { -30.55f, 15.56f },
    Vec2 { -29.59f, 15.97f },
    Vec2 { -28.68f, 16.61f },
    Vec2 { -27.71f, 17.43f },
    Vec2 { -27.04f, 18.18f },
    Vec2 { -26.54f, 19.06f },
    Vec2 { -26.11f, 19.93f },
    Vec2 { -25.68f, 20.96f },
    Vec2 { -25.45f, 22.02f },
    Vec2 { -25.34f, 23.25f },
    Vec2 { -25.31f, 25.27f },
    Vec2 { -25.34f, 27.11f },
    Vec2 { -25.36f, 29.63f },
    Vec2 { -25.44f, 32.07f },
    Vec2 { -25.36f, 34.17f },
    Vec2 { -25.33f, 36.75f },
    Vec2 { -25.05f, 38.93f },
    Vec2 { -24.45f, 40.74f },
    Vec2 { -23.85f, 41.85f },
    Vec2 { -23.00f, 42.79f },
    Vec2 { -22.17f, 43.58f },
    Vec2 { -21.05f, 44.33f },
    Vec2 { -19.98f, 44.79f },
    Vec2 { -18.92f, 45.09f },
    Vec2 { -18.01f, 45.25f },
    Vec2 { -17.10f, 45.36f },
    Vec2 { -16.34f, 45.39f },
    Vec2 { -14.33f, 45.36f },
    Vec2 { -11.56f, 45.45f },
    Vec2 { -8.43f, 45.40f },
    Vec2 { -4.93f, 45.36f },
    Vec2 { -0.96f, 45.36f },
    Vec2 { 2.18f, 45.36f },
    Vec2 { 5.38f, 45.38f },
    Vec2 { 8.31f, 45.41f },
    Vec2 { 10.33f, 45.37f },
    Vec2 { 13.80f, 45.28f },
    Vec2 { 17.09f, 45.28f },
    Vec2 { 18.52f, 44.97f },
    Vec2 { 19.60f, 44.57f },
    Vec2 { 20.58f, 44.13f },
    Vec2 { 21.41f, 43.60f },
    Vec2 { 22.04f, 43.07f },
    Vec2 { 22.60f, 42.42f },
    Vec2 { 23.15f, 41.76f },
    Vec2 { 23.57f, 41.06f },
    Vec2 { 23.97f, 40.23f },
    Vec2 { 24.26f, 39.40f },
    Vec2 { 24.42f, 38.60f },
    Vec2 { 24.57f, 37.61f },
    Vec2 { 24.64f, 36.96f },
    Vec2 { 24.63f, 35.82f },
    Vec2 { 24.66f, 34.09f },
    Vec2 { 24.76f, 32.13f },
    Vec2 { 24.65f, 29.76f },
    Vec2 { 24.64f, 27.01f },
    Vec2 { 24.57f, 25.14f },
    Vec2 { 24.64f, 22.89f },
    Vec2 { 24.79f, 21.78f },
    Vec2 { 24.93f, 21.04f },
    Vec2 { 25.10f, 20.45f },
    Vec2 { 25.36f, 19.85f },
    Vec2 { 25.66f, 19.26f },
    Vec2 { 25.97f, 18.70f },
    Vec2 { 26.30f, 18.20f },
    Vec2 { 26.71f, 17.71f },
    Vec2 { 27.17f, 17.26f },
    Vec2 { 27.75f, 16.74f },
    Vec2 { 28.34f, 16.29f },
    Vec2 { 28.97f, 15.86f },
    Vec2 { 29.62f, 15.54f },
    Vec2 { 30.26f, 15.27f },
    Vec2 { 31.06f, 15.07f },
    Vec2 { 32.19f, 14.87f },
    Vec2 { 33.28f, 14.80f },
    Vec2 { 34.79f, 14.81f },
    Vec2 { 36.22f, 14.83f },
    Vec2 { 38.38f, 14.84f },
    Vec2 { 41.54f, 14.88f },
    Vec2 { 43.48f, 14.88f },
    Vec2 { 45.74f, 14.79f },
    Vec2 { 47.27f, 14.61f },
    Vec2 { 48.13f, 14.43f },
    Vec2 { 48.90f, 14.14f },
    Vec2 { 49.58f, 13.85f },
    Vec2 { 50.56f, 13.45f },
    Vec2 { 51.27f, 12.91f },
    Vec2 { 51.85f, 12.35f },
    Vec2 { 52.41f, 11.78f },
    Vec2 { 52.84f, 11.04f },
    Vec2 { 53.14f, 10.35f },
    Vec2 { 53.53f, 9.63f },
    Vec2 { 53.85f, 8.88f },
    Vec2 { 54.10f, 7.65f },
    Vec2 { 54.22f, 6.57f },
    Vec2 { 54.30f, 5.42f },
    Vec2 { 54.28f, 3.18f },
    Vec2 { 54.32f, 0.89f },
    Vec2 { 54.43f, -1.98f },
    Vec2 { 54.38f, -4.91f },
    Vec2 { 54.38f, -6.79f },
    Vec2 { 54.34f, -9.02f },
    Vec2 { 54.38f, -10.78f },
    Vec2 { 54.30f, -14.38f },
    Vec2 { 54.31f, -16.44f },
    Vec2 { 54.26f, -17.57f },
    Vec2 { 54.26f, -18.35f }
},
{ // 1
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 2
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 3
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 4
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 5
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 6
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 7
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 8
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
},
{ // 9
    Vec2 { -10.0f, 0.0f },
    Vec2 { -5.0f, 0.0f },
    Vec2 { 0.0f, 0.0f },
    Vec2 { 5.0f, 0.0f },
    Vec2 { 10.0f, 0.0f }
}
};