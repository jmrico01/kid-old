#include "load_obj.h"

#include "km_debug.h"
#include "km_math.h"
#include "km_lib.h"

#define OBJ_LINE_MAX 512
#define FACE_MAX_VERTICES 4

struct Face {
    int inds[FACE_MAX_VERTICES];
    int size;
};

internal Vec3 ComputeTriangleNormal(Vec3 v1, Vec3 v2, Vec3 v3)
{
    Vec3 a = v2 - v1;
    Vec3 b = v3 - v1;
    return Normalize(Cross(a, b));
}

// str is a null-terminated string buffer with length+1 characters
internal Vec3 ParseVec3(char* str, int length)
{
    Vec3 result = { 0.0f, 0.0f, 0.0f };
    char* c = str;
    int el = 0;
    for (int i = 0; i < length + 1; i++) {
        if (str[i] == ' ' || i == length) {
            char* endptr = c;
            str[i] = '\0';
            result.e[el++] = (float32)strtod(c, &endptr);
            // TODO check error
            c = &str[i + 1];
        }
    }

    return result;
}

// line is a null-terminated string buffer with length+1 characters
// triangulates all faces
internal void ProcessLine(char* line, int length,
    DynamicArray<Vec3>& outVertices,
    DynamicArray<Face>& outFaces)
{
    if (length < 3) {
        return;
    }

    if (line[0] == 'v') {
        if (line[1] == ' ') {
            outVertices.Append(ParseVec3(&line[2], length - 2));
        }
        else if (line[1] == 'n') {
            // parse vec3 normal at &line[3], length - 3
        }
        else {
            // idk
        }
    }
    else if (line[0] == 'f') {
        Face face;
        face.size = 0;

        char* c = &line[2];
        for (int i = 2; i < length + 1; i++) {
            if (line[i] == ' ' || line[i] == '/' || i == length) {
                if (face.size >= FACE_MAX_VERTICES) {
                    DEBUG_PANIC("Face with %d vertices (max %d)\n",
                        face.size + 1, FACE_MAX_VERTICES);
                }

                int end = i;
                while (line[i] != ' ' && i < length + 1) {
                    i++;
                }
                line[end] = '\0';
                char* endptr = c;
                face.inds[face.size++] = (int)strtol(c, &endptr, 10) - 1;
                // TODO check error
                c = &line[i + 1];
            }
        }

        if (face.size < 3) {
            DEBUG_PANIC("Face with < 3 vertices\n");
        }
        else if (face.size == 4) {
            Face face2;
            face2.size = 3;
            face2.inds[0] = face.inds[2];
            face2.inds[1] = face.inds[3];
            face2.inds[2] = face.inds[0];
            outFaces.Append(face2);

            face.size = 3;
        }
        else if (face.size > 4) {
            DEBUG_PANIC("Unhandled %d-vertex face triangulation\n");
        }

        outFaces.Append(face);
    }
    else if (line[0] == 'o') {
        // TODO handle this (object name)
    }
    else if (line[0] == 'g') {
        // TODO handle this (group name, subgroups within object)
    }
    else if (line[0] == 's'
    || StringCompare(line, "usemtl", 6)
    || StringCompare(line, "mtllib", 6)) {
        // Ew
    }
    else if (line[0] == '#') {
        // Comment, ignore
    }
    else {
        // idk
        DEBUG_PANIC("Unhandled line in OBJ file: %s\n", line);
    }
}

MeshGL LoadOBJOpenGL(const ThreadContext* thread,
    const char* filePath,
    DEBUGPlatformReadFileFunc* DEBUGPlatformReadFile,
    DEBUGPlatformFreeFileMemoryFunc* DEBUGPlatformFreeFileMemory)
{
    MeshGL meshGL;
    meshGL.vertexCount = 0;

    DEBUGReadFileResult objFile = DEBUGPlatformReadFile(thread, filePath);
    if (!objFile.data) {
        DEBUG_PRINT("Failed to open OBJ file at: %s\n", filePath);
        return meshGL;
    }

    DynamicArray<Vec3> vertices;
    vertices.Init();
    DynamicArray<Face> faces;
    faces.Init();

    char line[OBJ_LINE_MAX];
    char* c = (char*)objFile.data;
    int i = 0;
    int lineLength = 0;
    while (true) {
        // TODO add lineLength < OBJ_LINE_MAX checks
        if (i < objFile.size && *c != '\r' && *c != '\n') {
            line[lineLength++] = *c;
            c++;
            i++;
        } else {
            while (i < objFile.size && (*c == '\r' || *c == '\n')) {
                c++;
                i++;
            }

            line[lineLength] = '\0';
            ProcessLine(line, lineLength, vertices, faces);

            if (i < objFile.size) {
                lineLength = 0;
            }
            else {
                break;
            }
        }
    }

#if 0
    DEBUG_PRINT("=== Parsed 3D Model ===\n");
    DEBUG_PRINT("--- VERTICES ---\n");
    for (int v = 0; v < (int)vertices.size; v++) {
        DEBUG_PRINT("( %f, %f, %f )\n",
            vertices[v].x, vertices[v].y, vertices[v].z);
    }
    DEBUG_PRINT("--- FACES ---\n");
    for (int f = 0; f < (int)faces.size; f++) {
        DEBUG_ASSERT(faces[f].size == 3);
        DEBUG_PRINT("( ");
        for (int fi = 0; fi < (int)faces[f].size; fi++) {
            DEBUG_PRINT("%d", faces[f].inds[fi]);
            if (fi != (int)faces[f].size - 1) {
                DEBUG_PRINT(", ");
            }
        }
        DEBUG_PRINT(" )\n");
    }
#endif

    DynamicArray<Vec3> verticesGL;
    verticesGL.Init();
    DynamicArray<Vec3> normalsGL;
    normalsGL.Init();

    for (int f = 0; f < (int)faces.size; f++) {
        DEBUG_ASSERT(faces[f].size == 3);

        Vec3 v1 = vertices[faces[f].inds[0]];
        Vec3 v2 = vertices[faces[f].inds[1]];
        Vec3 v3 = vertices[faces[f].inds[2]];
        Vec3 normal = ComputeTriangleNormal(v1, v2, v3);

        verticesGL.Append(v1);
        verticesGL.Append(v2);
        verticesGL.Append(v3);
        normalsGL.Append(normal);
        normalsGL.Append(normal);
        normalsGL.Append(normal);
    }

    vertices.Free();
    faces.Free();

#if 0
    DEBUG_PRINT("MeshGL: %d vertices, %d normals\n",
        verticesGL.size, normalsGL.size);
    for (int v = 0; v < (int)verticesGL.size; v++) {
        DEBUG_PRINT("( %f, %f, %f )\n    (%f, %f, %f)\n",
            verticesGL[v].x, verticesGL[v].y, verticesGL[v].z,
            normalsGL[v].x, normalsGL[v].y, normalsGL[v].z);
    }
#endif

    glGenVertexArrays(1, &meshGL.vertexArray);
    glBindVertexArray(meshGL.vertexArray);

    glGenBuffers(1, &meshGL.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, meshGL.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, verticesGL.size * sizeof(Vec3),
        verticesGL.data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); // match shader layout location
    glVertexAttribPointer(
        0, // match shader layout location
        3, // size (Vec3)
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        0, // stride
        (void*)0 // array buffer offset
    );

    glGenBuffers(1, &meshGL.normalBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, meshGL.normalBuffer);
    glBufferData(GL_ARRAY_BUFFER, normalsGL.size * sizeof(Vec3),
        normalsGL.data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1); // match shader layout location
    glVertexAttribPointer(
        1, // match shader layout location
        3, // size (Vec3)
        GL_FLOAT, // type
        GL_FALSE, // normalized?
        0, // stride
        (void*)0 // array buffer offset
    );

    glBindVertexArray(0);

    meshGL.programID = LoadShaders(thread,
        "shaders/model.vert",
        "shaders/model.frag",
        DEBUGPlatformReadFile,
        DEBUGPlatformFreeFileMemory);

    meshGL.vertexCount = (int)verticesGL.size;

    // TODO free vertices & normals

    return meshGL;
}