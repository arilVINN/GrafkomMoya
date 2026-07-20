// ============================================================
//  MOYA YA GES YA, OBJEK-NYA SANGAT VARIASI SEKALI
// ============================================================
//
//  FITUR:
//  1. Load model .OBJ + .MTL dari Blender (Info: di blender pake bake biar bisa diload texturenya )
//  2. Kamera bisa woooshhhh
//  3. Kamera kek Blender:
//     - Left Mouse Drag    : Orbit (rotasi mengelilingi pivot)
//     - Right Mouse Drag   : Geser titik pivot
//     - Scroll Wheel       : Zoom in/out
//     - W/A/S/D            : Maju/Mundur/Kiri/Kanan
//     - Spasi/Shift        : Naik/Turun (Kek minecraft, requestnya JOSAN)
//     - Z                  : Toggle wireframe
//     - R                  : Reset kamera
//  4. Material hasil bake dari file .MTL
//  5. Info HUD (vertex count, FPS, kontrol)
//
// ============================================================

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

// ============================================================
// SECTION 1: KONSTANTA & STRUKTUR DATA
// ============================================================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define DEG2RAD (M_PI / 180.0)

// Vektor 2D (buat UV / Texture Coordinatenya)
struct Vec2 {
    float u, v;
    Vec2() : u(0), v(0) {}
    Vec2(float u, float v) : u(u), v(v) {}
};

// Vektor 3D 
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

// Material (dari file .MTL)
struct Material {
    std::string name;
    float Ka[4];    // Ambient color
    float Kd[4];    // Diffuse color
    float Ks[4];    // Specular color
    float Ns;       // Shininess (specular exponent)
    std::string texturePath;    // Path ke file texture (map_Kd)
    float texScaleS, texScaleT; // Texture UV scale (dari opsi -s di MTL)
    GLuint textureID;           // OpenGL texture ID
    bool hasTexture;            // cek material punya texture or nah

    Material() : Ns(32.0f), texScaleS(1.0f), texScaleT(1.0f),
                 textureID(0), hasTexture(false) {
        Ka[0]=0.2f; Ka[1]=0.2f; Ka[2]=0.2f; Ka[3]=1.0f;
        Kd[0]=0.8f; Kd[1]=0.8f; Kd[2]=0.8f; Kd[3]=1.0f;
        Ks[0]=0.5f; Ks[1]=0.5f; Ks[2]=0.5f; Ks[3]=1.0f;
    }
};

//  Vertex pada Face (indeks ke vertex, texcoord, normal) 
struct FaceVert {
    int vi, ti, ni; // vertex index, texcoord index, normal index (-1 = tidak ada)
    FaceVert() : vi(-1), ti(-1), ni(-1) {}
};

//  Batch render: sekelompok face dengan material yang sama 
struct RenderBatch {
    std::string materialName;
    std::vector< std::vector<FaceVert> > faces;
};

// ============================================================
// SECTION 2: VARIABEL GLOBAL
// ============================================================

//  Data Geometri Scene (dari file .OBJ) 
std::vector<Vec3> gVertices;    // Semua vertex positions
std::vector<Vec2> gTexCoords;   // Semua texture coordinates
std::vector<Vec3> gNormals;     // Semua vertex normals
std::map<std::string, Material> gMaterials;  // Semua material (key = nama)
std::vector<RenderBatch> gBatches;           // Face groups per material
std::map<std::string, GLuint> gTextureCache; // Cache texture (path -> GL ID)

// Display List untuk render cepat
GLuint gSceneList = 0;

// Statistik Scene
int gTotalVerts = 0, gTotalFaces = 0, gTotalTextures = 0, gTotalObjects = 0;

// KAMERA
float camYaw   = -45.0f;   // Sudut horizontal (derajat)
float camPitch =  20.0f;   // Sudut vertikal (derajat)
float camDist  =  80.0f;   // Jarak kamera dari pivot
Vec3  camPivot(0.0f, 3.0f, 60.0f); // Titik yang dilihat kamera (pivot/target)

// Batas pivot awal
Vec3 sceneBoundsMin, sceneBoundsMax, sceneCenter;

// Mouse State
int mouseLastX = 0, mouseLastY = 0;
bool mouseLeftDown   = false;
bool mouseRightDown  = false;
bool mouseMiddleDown = false;

// Keyboard State (untuk WASD movement)
bool keyState[256] = { false };
bool shiftDown = false; // Tracking untuk Left Shift

// Day/Night State
bool isDayTime = true;

// Window
int winW = 1280, winH = 720;

// Mode Render
bool wireframeMode = false;

// FPS Counter
int   frameCount = 0;
float currentFPS = 0.0f;
int   lastFPSTime = 0;

// Movement Speed
float moveSpeed = 0.8f;

// ============================================================
// SECTION 3: FUNGSI UTILITAS
// ============================================================

// Trim karakter whitespace dan \r dari string
static std::string trimStr(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Dapatkan direktori dari path file
static std::string getDirectory(const std::string& filepath) {
    size_t pos = filepath.find_last_of("/\\");
    if (pos != std::string::npos) return filepath.substr(0, pos + 1);
    return "";
}

// Clamp nilai
static float clampf(float val, float lo, float hi) {
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

// Render teks bitmap di posisi 2D
static void drawBitmapString(float x, float y, const char* text, void* font = GLUT_BITMAP_HELVETICA_12) {
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; c++) {
        glutBitmapCharacter(font, *c);
    }
}

// ============================================================
// SECTION 4: TEXTURE LOADER (stb_image ke OpenGL)
// ============================================================

GLuint loadTexture(const std::string& path) {
    // Cek cache
    if (gTextureCache.count(path)) {
        return gTextureCache[path];
    }

    std::cout << "  [TEX] Loading: " << path << std::endl;

    // Load image dengan stb_image
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1); 
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "  [TEX] GAGAL load texture: " << path << std::endl;
        return 0;
    }

    // Format
    GLenum format = GL_RGB;
    if (channels == 1) format = GL_LUMINANCE;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 4) format = GL_RGBA;

    // Buat OpenGL texture
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    // Set parameter texture
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload data dan generate mipmaps
    gluBuild2DMipmaps(GL_TEXTURE_2D, format, width, height, format, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Simpan ke cache
    gTextureCache[path] = texID;
    gTotalTextures++;

    std::cout << "  [TEX] OK (" << width << "x" << height << ", " << channels << "ch) -> ID=" << texID << std::endl;
    return texID;
}

// ============================================================
// SECTION 5: MTL PARSER (Material Library Loader)
// ============================================================

bool loadMTL(const std::string& mtlPath) {
    std::ifstream file(mtlPath.c_str());
    if (!file.is_open()) {
        std::cerr << "[MTL] Gagal membuka: " << mtlPath << std::endl;
        return false;
    }

    std::cout << "[MTL] Memuat: " << mtlPath << std::endl;

    Material* currentMat = NULL;
    std::string line;

    while (std::getline(file, line)) {
        line = trimStr(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string keyword;
        ss >> keyword;

        if (keyword == "newmtl") {
            // Material baru
            std::string matName;
            std::getline(ss, matName);
            matName = trimStr(matName);

            gMaterials[matName] = Material();
            gMaterials[matName].name = matName;
            currentMat = &gMaterials[matName];
        }
        else if (!currentMat) {
            continue; // Belum ada material aktif
        }
        else if (keyword == "Ka") {
            ss >> currentMat->Ka[0] >> currentMat->Ka[1] >> currentMat->Ka[2];
            currentMat->Ka[3] = 1.0f;
        }
        else if (keyword == "Kd") {
            ss >> currentMat->Kd[0] >> currentMat->Kd[1] >> currentMat->Kd[2];
            currentMat->Kd[3] = 1.0f;
        }
        else if (keyword == "Ks") {
            ss >> currentMat->Ks[0] >> currentMat->Ks[1] >> currentMat->Ks[2];
            currentMat->Ks[3] = 1.0f;
        }
        else if (keyword == "Ns") {
            ss >> currentMat->Ns;
            // Clamp Ns ke range OpenGL (0-128)
            currentMat->Ns = clampf(currentMat->Ns, 0.0f, 128.0f);
        }
        else if (keyword == "d") {
            float alpha;
            if (ss >> alpha) {
                currentMat->Kd[3] = alpha;
                currentMat->Ka[3] = alpha;
                currentMat->Ks[3] = alpha;
            }
        }
        else if (keyword == "Tr") {
            float trans;
            if (ss >> trans) {
                currentMat->Kd[3] = 1.0f - trans;
                currentMat->Ka[3] = 1.0f - trans;
                currentMat->Ks[3] = 1.0f - trans;
            }
        }
        else if (keyword == "map_Kd") {
            // Parse texture path (mungkin ada opsi -s sebelumnya)
            // Format: map_Kd [-s sx sy sz] path/ke/texture.jpg
            std::string remainder;
            std::getline(ss, remainder);
            remainder = trimStr(remainder);

            size_t pathStart = std::string::npos;
            for (size_t i = 0; i + 2 < remainder.size(); i++) {
                if (isalpha(remainder[i]) && remainder[i+1] == ':' &&
                    (remainder[i+2] == '/' || remainder[i+2] == '\\')) {
                    pathStart = i;
                    break;
                }
            }

            if (pathStart != std::string::npos) {
                // Ada path absolut
                currentMat->texturePath = trimStr(remainder.substr(pathStart));

                // Parse opsi sebelum path
                std::string options = remainder.substr(0, pathStart);
                std::istringstream optSS(options);
                std::string optToken;
                while (optSS >> optToken) {
                    if (optToken == "-s") {
                        float sx = 1.0f, sy = 1.0f, sz = 1.0f;
                        if (optSS >> sx) currentMat->texScaleS = sx;
                        if (optSS >> sy) currentMat->texScaleT = sy;
                        optSS >> sz; // Baca sz tapi tidak dipakai (2D)
                    }
                    // Opsi lain (-o, -t, dll.) di-skip
                }
            } else {
                // Path relatif
                currentMat->texturePath = trimStr(remainder);
            }
        }
    }

    file.close();

    // Load semua texture dari material yang punya map_Kd
    for (auto& pair : gMaterials) {
        Material& mat = pair.second;
        if (!mat.texturePath.empty()) {
            mat.textureID = loadTexture(mat.texturePath);
            mat.hasTexture = (mat.textureID != 0);
        }
    }

    std::cout << "[MTL] Selesai. Total material: " << gMaterials.size() << std::endl;
    return true;
}

// ============================================================
// SECTION 6: OBJ PARSER (Wavefront OBJ Loader)
// ============================================================

bool loadOBJ(const std::string& objPath) {
    std::ifstream file(objPath.c_str());
    if (!file.is_open()) {
        std::cerr << "[OBJ] Gagal membuka: " << objPath << std::endl;
        return false;
    }

    std::cout << "[OBJ] Memuat: " << objPath << std::endl;

    std::string baseDir = getDirectory(objPath);
    RenderBatch* currentBatch = NULL;
    std::string line;
    int lineNum = 0;

    // Inisialisasi batas scene
    sceneBoundsMin = Vec3( 1e9f,  1e9f,  1e9f);
    sceneBoundsMax = Vec3(-1e9f, -1e9f, -1e9f);

    while (std::getline(file, line)) {
        lineNum++;
        line = trimStr(line);
        if (line.empty() || line[0] == '#') continue;

        // Progress setiap 20000 baris
        if (lineNum % 20000 == 0) {
            std::cout << "  [OBJ] Baris " << lineNum << " diproses..." << std::endl;
        }

        // Ambil keyword pertama
        char keyword[16] = {0};
        sscanf(line.c_str(), "%15s", keyword);

        if (strcmp(keyword, "mtllib") == 0) {
            // Load Material Library
            std::string mtlFile = trimStr(line.substr(6));
            std::string mtlFullPath = baseDir + mtlFile;
            loadMTL(mtlFullPath);
        }
        else if (strcmp(keyword, "o") == 0) {
            // Object baru
            gTotalObjects++;
        }
        else if (strcmp(keyword, "v") == 0 && line.size() > 1 && line[1] == ' ') {
            // Vertex Position
            Vec3 v;
            sscanf(line.c_str(), "v %f %f %f", &v.x, &v.y, &v.z);
            gVertices.push_back(v);

            // Update bounding box
            if (v.x < sceneBoundsMin.x) sceneBoundsMin.x = v.x;
            if (v.y < sceneBoundsMin.y) sceneBoundsMin.y = v.y;
            if (v.z < sceneBoundsMin.z) sceneBoundsMin.z = v.z;
            if (v.x > sceneBoundsMax.x) sceneBoundsMax.x = v.x;
            if (v.y > sceneBoundsMax.y) sceneBoundsMax.y = v.y;
            if (v.z > sceneBoundsMax.z) sceneBoundsMax.z = v.z;
        }
        else if (strcmp(keyword, "vt") == 0) {
            // Texture Coordinate
            Vec2 vt;
            sscanf(line.c_str(), "vt %f %f", &vt.u, &vt.v);
            gTexCoords.push_back(vt);
        }
        else if (strcmp(keyword, "vn") == 0) {
            // Vertex Normal
            Vec3 vn;
            sscanf(line.c_str(), "vn %f %f %f", &vn.x, &vn.y, &vn.z);
            gNormals.push_back(vn);
        }
        else if (strcmp(keyword, "usemtl") == 0) {
            // Ganti Material Aktif
            std::string matName = trimStr(line.substr(6));

            // Buat batch baru untuk material ini
            RenderBatch batch;
            batch.materialName = matName;
            gBatches.push_back(batch);
            currentBatch = &gBatches.back();
        }
        else if (strcmp(keyword, "f") == 0) {
            // --- Face (polygon) ---

            if (!currentBatch) {
                // Face tanpa material -> buat batch default
                RenderBatch batch;
                batch.materialName = "";
                gBatches.push_back(batch);
                currentBatch = &gBatches.back();
            }

            std::vector<FaceVert> faceVerts;
            std::istringstream fss(line.substr(1)); // skip 'f'
            std::string segment;

            while (fss >> segment) {
                FaceVert fv;
                // Parse format v/vt/vn
                int vi = 0, ti = 0, ni = 0;

                if (sscanf(segment.c_str(), "%d/%d/%d", &vi, &ti, &ni) == 3) {
                    fv.vi = vi - 1; fv.ti = ti - 1; fv.ni = ni - 1;
                }
                else if (sscanf(segment.c_str(), "%d//%d", &vi, &ni) == 2) {
                    fv.vi = vi - 1; fv.ni = ni - 1;
                }
                else if (sscanf(segment.c_str(), "%d/%d", &vi, &ti) == 2) {
                    fv.vi = vi - 1; fv.ti = ti - 1;
                }
                else if (sscanf(segment.c_str(), "%d", &vi) == 1) {
                    fv.vi = vi - 1;
                }

                faceVerts.push_back(fv);
            }

            if (faceVerts.size() >= 3) {
                currentBatch->faces.push_back(faceVerts);
                gTotalFaces++;
            }
        }
    }

    file.close();
    gTotalVerts = (int)gVertices.size();

    // Hitung pusat scene
    sceneCenter.x = (sceneBoundsMin.x + sceneBoundsMax.x) * 0.5f;
    sceneCenter.y = (sceneBoundsMin.y + sceneBoundsMax.y) * 0.5f;
    sceneCenter.z = (sceneBoundsMin.z + sceneBoundsMax.z) * 0.5f;

    // Hitung diagonal bounding box untuk jarak kamera awal
    float dx = sceneBoundsMax.x - sceneBoundsMin.x;
    float dy = sceneBoundsMax.y - sceneBoundsMin.y;
    float dz = sceneBoundsMax.z - sceneBoundsMin.z;
    float diagonal = sqrtf(dx*dx + dy*dy + dz*dz);

    // Set kamera awal
    camPivot = sceneCenter;
    camDist = diagonal * 0.8f;
    if (camDist < 5.0f) camDist = 5.0f;
    moveSpeed = diagonal * 0.01f;
    if (moveSpeed < 0.1f) moveSpeed = 0.1f;

    std::cout << "[OBJ] Selesai!" << std::endl;
    std::cout << "  Objects  : " << gTotalObjects << std::endl;
    std::cout << "  Vertices : " << gTotalVerts << std::endl;
    std::cout << "  TexCoords: " << gTexCoords.size() << std::endl;
    std::cout << "  Normals  : " << gNormals.size() << std::endl;
    std::cout << "  Faces    : " << gTotalFaces << std::endl;
    std::cout << "  Batches  : " << gBatches.size() << std::endl;
    std::cout << "  Textures : " << gTotalTextures << std::endl;
    std::cout << "  Bounds   : (" << sceneBoundsMin.x << "," << sceneBoundsMin.y << "," << sceneBoundsMin.z
              << ") - (" << sceneBoundsMax.x << "," << sceneBoundsMax.y << "," << sceneBoundsMax.z << ")" << std::endl;
    std::cout << "  Center   : (" << sceneCenter.x << "," << sceneCenter.y << "," << sceneCenter.z << ")" << std::endl;
    std::cout << "  CamDist  : " << camDist << std::endl;

    return true;
}

// ============================================================
// SECTION 7: BUILD DISPLAY LIST (Compile scene)
// ============================================================

void buildSceneDisplayList() {
    std::cout << "[RENDER] Membuat Display List..." << std::endl;

    if (gSceneList) glDeleteLists(gSceneList, 1);
    gSceneList = glGenLists(1);
    glNewList(gSceneList, GL_COMPILE);

    std::string lastMaterial = "___NONE___";

    for (int pass = 0; pass < 2; pass++) {
        // Pass 0: Render Objek Solid/Opaque, Pass 1: Render Objek Transparan (Kaca)
        if (pass == 0) {
            glDepthMask(GL_TRUE);
        } else {
            glDepthMask(GL_FALSE); 
            lastMaterial = "___NONE___"; // Paksa update material untuk pass 2
        }

        for (size_t b = 0; b < gBatches.size(); b++) {
            const RenderBatch& batch = gBatches[b];
            if (batch.faces.empty()) continue;

            bool isTransparent = false;
            if (gMaterials.count(batch.materialName)) {
                if (gMaterials[batch.materialName].Kd[3] < 0.99f) {
                    isTransparent = true;
                }
            }

            // Pisahkan batch berdasarkan pass
            if (pass == 0 && isTransparent) continue;
            if (pass == 1 && !isTransparent) continue;

            if (batch.materialName != lastMaterial) {
                lastMaterial = batch.materialName;

                if (gMaterials.count(batch.materialName)) {
                    const Material& mat = gMaterials[batch.materialName];

                    // Kurangi efek specular jika kaca
                    float customKs[4] = { mat.Ks[0], mat.Ks[1], mat.Ks[2], mat.Ks[3] };
                    if (isTransparent) {
                        customKs[0] *= 0.1f; customKs[1] *= 0.1f; customKs[2] *= 0.1f;
                    }

                    // Set OpenGL material properties
                    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, mat.Ka);
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, mat.Kd);
                    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, customKs);
                    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, mat.Ns);

                    // Set warna untuk non-lighting fallback
                    glColor4fv(mat.Kd);

                    // Bind texture jika ada
                    if (mat.hasTexture) {
                        glEnable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, mat.textureID);
                    } else {
                        glDisable(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, 0);
                    }
                } else {
                    // Material default (tidak ditemukan)
                    float defaultKd[] = {0.7f, 0.7f, 0.7f, 1.0f};
                    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, defaultKd);
                    glColor4fv(defaultKd);
                    glDisable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
            }

            // Dapatkan material untuk texture scaling
            float tsS = 1.0f, tsT = 1.0f;
            if (gMaterials.count(batch.materialName)) {
                tsS = gMaterials[batch.materialName].texScaleS;
                tsT = gMaterials[batch.materialName].texScaleT;
            }

            // --- Render semua face dalam batch ini ---
            for (size_t f = 0; f < batch.faces.size(); f++) {
                const std::vector<FaceVert>& face = batch.faces[f];

                glBegin(GL_POLYGON);
                for (size_t i = 0; i < face.size(); i++) {
                    const FaceVert& fv = face[i];

                    // Normal
                    if (fv.ni >= 0 && fv.ni < (int)gNormals.size()) {
                        glNormal3f(gNormals[fv.ni].x, gNormals[fv.ni].y, gNormals[fv.ni].z);
                    }

                    // Texture Coordinate (dengan UV scaling dari MTL)
                    if (fv.ti >= 0 && fv.ti < (int)gTexCoords.size()) {
                        float u = gTexCoords[fv.ti].u * tsS;
                        float v = gTexCoords[fv.ti].v * tsT;
                        glTexCoord2f(u, v);
                    }

                    // Vertex Position
                    if (fv.vi >= 0 && fv.vi < (int)gVertices.size()) {
                        glVertex3f(gVertices[fv.vi].x, gVertices[fv.vi].y, gVertices[fv.vi].z);
                    }
                }
                glEnd();
            }
        }
    }
    glDepthMask(GL_TRUE); // Kembalikan Depth Mask ke normal

    glDisable(GL_TEXTURE_2D);
    glEndList();

    std::cout << "[RENDER] Display List selesai dibuat!" << std::endl;
}

// ============================================================
// SECTION 8: GRID & AXIS
// ============================================================

// Menggambar grid pada bidang XZ 
void drawGrid() {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    float gridSize = 100.0f;
    float gridStep = 5.0f;
    float gridY = sceneBoundsMin.y - 0.01f; // Sedikit di bawah model

    // Grid center offset ke tengah scene
    float cx = floorf(sceneCenter.x / gridStep) * gridStep;
    float cz = floorf(sceneCenter.z / gridStep) * gridStep;

    glBegin(GL_LINES);
    // Grid tipis (abu-abu gelap)
    glColor4f(0.3f, 0.3f, 0.3f, 0.5f);
    for (float i = -gridSize; i <= gridSize; i += gridStep) {
        glVertex3f(cx + i, gridY, cz - gridSize);
        glVertex3f(cx + i, gridY, cz + gridSize);
        glVertex3f(cx - gridSize, gridY, cz + i);
        glVertex3f(cx + gridSize, gridY, cz + i);
    }
    glEnd();

    glPopAttrib();
}

// Menggambar sumbu XYZ di pusat scene
void drawAxes() {
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LINE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.5f);

    float len = camDist * 0.1f; // Panjang sumbu relatif terhadap zoom
    float ox = sceneCenter.x, oy = sceneBoundsMin.y, oz = sceneCenter.z;

    glBegin(GL_LINES);
    // Sumbu X (Merah)
    glColor3f(1.0f, 0.2f, 0.2f);
    glVertex3f(ox, oy, oz);
    glVertex3f(ox + len, oy, oz);

    // Sumbu Y (Hijau)
    glColor3f(0.2f, 1.0f, 0.2f);
    glVertex3f(ox, oy, oz);
    glVertex3f(ox, oy + len, oz);

    // Sumbu Z (Biru)
    glColor3f(0.2f, 0.2f, 1.0f);
    glVertex3f(ox, oy, oz);
    glVertex3f(ox, oy, oz + len);
    glEnd();

    glLineWidth(1.0f);
    glPopAttrib();
}

// ============================================================
// SECTION 9: HUD (Heads-Up Display)
// ============================================================

void drawHUD() {
    // Simpan semua state OpenGL
    glPushAttrib(GL_ALL_ATTRIB_BITS);

    // Ganti ke proyeksi orthographic 2D
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, winW, 0, winH);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Matikan fitur 3D
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    // === Background semi-transparan untuk panel info ===
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Panel kiri atas (info scene)
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(0, winH);
    glVertex2f(280, winH);
    glVertex2f(280, winH - 160);
    glVertex2f(0, winH - 160);
    glEnd();

    // Panel kanan bawah (kontrol)
    glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(winW - 310, 180);
    glVertex2f(winW, 180);
    glVertex2f(winW, 0);
    glVertex2f(winW - 310, 0);
    glEnd();

    // === Teks Info Scene (kiri atas) ===
    char buf[256];

    // Judul
    glColor3f(0.3f, 0.85f, 1.0f); // Cyan

    glColor3f(1.0f, 1.0f, 1.0f);
    sprintf(buf, "FPS: %.0f", currentFPS);
    drawBitmapString(10, winH - 45, buf);

    glColor3f(0.8f, 0.8f, 0.8f);
    sprintf(buf, "Vertices : %d", gTotalVerts);
    drawBitmapString(10, winH - 65, buf);

    sprintf(buf, "Faces    : %d", gTotalFaces);
    drawBitmapString(10, winH - 80, buf);

    sprintf(buf, "Objects  : %d", gTotalObjects);
    drawBitmapString(10, winH - 95, buf);

    sprintf(buf, "Textures : %d", gTotalTextures);
    drawBitmapString(10, winH - 110, buf);

    sprintf(buf, "Materials: %d", (int)gMaterials.size());
    drawBitmapString(10, winH - 125, buf);

    glColor3f(0.6f, 0.6f, 0.6f);
    sprintf(buf, "Mode: %s", wireframeMode ? "WIREFRAME" : "SOLID");
    drawBitmapString(10, winH - 145, buf);

    // Teks Kontrol (kanan bawah)
    float rx = (float)(winW - 300);

    glColor3f(0.3f, 0.85f, 1.0f);
    drawBitmapString(rx, 165, "KONTROL GESS", GLUT_BITMAP_HELVETICA_18);

    glColor3f(0.9f, 0.9f, 0.7f);
    drawBitmapString(rx, 145, "Left Mouse Drag  : Orbit (Rotasi)");
    drawBitmapString(rx, 130, "Right Mouse Drag : Pan (Geser)");
    drawBitmapString(rx, 115, "Scroll Wheel     : Zoom In/Out");

    glColor3f(0.7f, 0.9f, 0.7f);
    drawBitmapString(rx, 95,  "W / A / S / D    : Gerak Maju/Kiri/Mundur/Kanan");
    drawBitmapString(rx, 80,  "SPACE / LSHIFT   : Gerak Naik/Turun");

    glColor3f(0.9f, 0.7f, 0.7f);
    drawBitmapString(rx, 60,  "Z                : Toggle Wireframe");
    drawBitmapString(rx, 45,  "R                : Reset Kamera");
    drawBitmapString(rx, 30,  "+  /  -          : Kecepatan +/-");
    drawBitmapString(rx, 15,  "ESC              : Keluar");

    // Restore state
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glPopAttrib();
}

// ============================================================
// SECTION 10: INISIALISASI OPENGL
// ============================================================

void initGL() {
    // Background langit siang
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

    // Aktifkan Depth Test
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // Aktifkan Lighting
    glEnable(GL_LIGHTING);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);

    // // === LIGHT 0: Lampu Utama (mengikuti kamera) ===
    // glEnable(GL_LIGHT0);
    // GLfloat light0Pos[]     = { 0.0f, 1.0f, 1.0f, 0.0f }; // Directional (w=0)
    // GLfloat light0Ambient[] = { 0.15f, 0.15f, 0.18f, 1.0f };
    // GLfloat light0Diffuse[] = { 0.85f, 0.83f, 0.80f, 1.0f };
    // GLfloat light0Spec[]    = { 0.5f, 0.5f, 0.5f, 1.0f };
    // glLightfv(GL_LIGHT0, GL_POSITION, light0Pos);
    // glLightfv(GL_LIGHT0, GL_AMBIENT,  light0Ambient);
    // glLightfv(GL_LIGHT0, GL_DIFFUSE,  light0Diffuse);
    // glLightfv(GL_LIGHT0, GL_SPECULAR, light0Spec);


    // Room Light Prop
    GLfloat roomLightAmbient[] = { 0.1f, 0.1f, 0.1f, 1.0f };
    GLfloat roomLightSpec[]    = { 0.5f, 0.5f, 0.5f, 1.0f };

    // === LIGHT 1: RUUANG KEMBAR SATUNYA ===
    glEnable(GL_LIGHT1);
    GLfloat light1Pos[]     = { -1.0f, 0.5f, -0.5f, 0.0f };
    GLfloat light1Diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f }; // White light
    GLfloat light1Spec[]    = { 0.5f, 0.5f, 0.5f, 1.0f };
    float light1ConstAtt = 1.0f;
    float light1LinearAtt = 0.007f;
    float light1QuadAtt = 0.0002f;
    glLightfv(GL_LIGHT1, GL_AMBIENT,  roomLightAmbient);
    glLightfv(GL_LIGHT1, GL_DIFFUSE,  light1Diffuse);
    glLightfv(GL_LIGHT1, GL_SPECULAR, light1Spec);
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, light1ConstAtt);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, light1LinearAtt);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, light1QuadAtt);

    // === LIGHT 2: lampu Ruang Tengah Itu Lah ===
    glEnable(GL_LIGHT2);
    GLfloat light2Diffuse[] = { 0.5f, 0.5f, 0.5f, 1.0f }; // Slightly dimmed
    float light2ConstAtt = 1.0f;
    float light2LinearAtt = 0.01f; // Increased attenuation
    float light2QuadAtt = 0.002f;
    glLightfv(GL_LIGHT2, GL_AMBIENT,  roomLightAmbient);
    glLightfv(GL_LIGHT2, GL_DIFFUSE,  light2Diffuse);
    glLightfv(GL_LIGHT2, GL_SPECULAR, roomLightSpec);
    glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, light2ConstAtt);
    glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, light2LinearAtt);
    glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, light2QuadAtt);

    // === LIGHT 3: RUANG BELAKANG (red) ===
    glEnable(GL_LIGHT3);
    GLfloat light3Diffuse[] = { 1.5f, 1.5f, 1.5f, 1.0f }; // Brighter white
    float light3ConstAtt = 1.0f;
    float light3LinearAtt = 0.001f; // Much lower attenuation
    float light3QuadAtt = 0.0f;
    glLightfv(GL_LIGHT3, GL_AMBIENT,  roomLightAmbient);
    glLightfv(GL_LIGHT3, GL_DIFFUSE,  light3Diffuse);
    glLightfv(GL_LIGHT3, GL_SPECULAR, roomLightSpec);
    glLightf(GL_LIGHT3, GL_CONSTANT_ATTENUATION, light3ConstAtt);
    glLightf(GL_LIGHT3, GL_LINEAR_ATTENUATION, light3LinearAtt);
    glLightf(GL_LIGHT3, GL_QUADRATIC_ATTENUATION, light3QuadAtt);

    // === LIGHT 4: RUANG DEPAN (Green) ===
    glEnable(GL_LIGHT4);
    GLfloat light4Diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float light4ConstAtt = 1.0f;
    float light4LinearAtt = 0.007f;
    float light4QuadAtt = 0.0002f;
    glLightfv(GL_LIGHT4, GL_AMBIENT,  roomLightAmbient);
    glLightfv(GL_LIGHT4, GL_DIFFUSE,  light4Diffuse);
    glLightfv(GL_LIGHT4, GL_SPECULAR, roomLightSpec);
    glLightf(GL_LIGHT4, GL_CONSTANT_ATTENUATION, light4ConstAtt);
    glLightf(GL_LIGHT4, GL_LINEAR_ATTENUATION, light4LinearAtt);
    glLightf(GL_LIGHT4, GL_QUADRATIC_ATTENUATION, light4QuadAtt);

    // === LIGHT 5: WC (Blue) ===
    glEnable(GL_LIGHT5);
    GLfloat light5Diffuse[] = { 0.5f, 0.5f, 0.5f, 1.0f }; // Slightly dimmed
    float light5ConstAtt = 1.0f;
    float light5LinearAtt = 0.01f; // Increased attenuation
    float light5QuadAtt = 0.002f;
    glLightfv(GL_LIGHT5, GL_AMBIENT,  roomLightAmbient);
    glLightfv(GL_LIGHT5, GL_DIFFUSE,  light5Diffuse);
    glLightfv(GL_LIGHT5, GL_SPECULAR, roomLightSpec);
    glLightf(GL_LIGHT5, GL_CONSTANT_ATTENUATION, light5ConstAtt);
    glLightf(GL_LIGHT5, GL_LINEAR_ATTENUATION, light5LinearAtt);
    glLightf(GL_LIGHT5, GL_QUADRATIC_ATTENUATION, light5QuadAtt);

    // === LIGHT 6: SEBELAH DAPUR (Purple/Magenta) ===
    glEnable(GL_LIGHT6);
    GLfloat light6Diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float light6ConstAtt = 1.0f;
    float light6LinearAtt = 0.007f;
    float light6QuadAtt = 0.0002f;
    glLightfv(GL_LIGHT6, GL_AMBIENT,  roomLightAmbient);
    glLightfv(GL_LIGHT6, GL_DIFFUSE,  light6Diffuse);
    glLightfv(GL_LIGHT6, GL_SPECULAR, roomLightSpec);
    glLightf(GL_LIGHT6, GL_CONSTANT_ATTENUATION, light6ConstAtt);
    glLightf(GL_LIGHT6, GL_LINEAR_ATTENUATION, light6LinearAtt);
    glLightf(GL_LIGHT6, GL_QUADRATIC_ATTENUATION, light6QuadAtt);

    // === LIGHT 7: RUANG KEMBAR (Cyan) ===
    glEnable(GL_LIGHT7);
    GLfloat light7Diffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float light7ConstAtt = 1.0f;
    float light7LinearAtt = 0.007f;
    float light7QuadAtt = 0.0002f;
    glLightfv(GL_LIGHT7, GL_AMBIENT,  roomLightAmbient);
    glLightfv(GL_LIGHT7, GL_DIFFUSE,  light7Diffuse);
    glLightfv(GL_LIGHT7, GL_SPECULAR, roomLightSpec);
    glLightf(GL_LIGHT7, GL_CONSTANT_ATTENUATION, light7ConstAtt);
    glLightf(GL_LIGHT7, GL_LINEAR_ATTENUATION, light7LinearAtt);
    glLightf(GL_LIGHT7, GL_QUADRATIC_ATTENUATION, light7QuadAtt);

    // Aktifkan Color Material agar glColor juga mempengaruhi material
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    // Aktifkan Blending untuk efek transparan (kaca/alpha)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Hint kualitas
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

// ============================================================
// SECTION 11: DISPLAY CALLBACK --- PIPELINE MVP ---
// ============================================================
//
//  Pipeline rendering:
//
//  [Model Space] --M--> [World Space] --V--> [View/Camera Space] --P--> [Clip Space]
//
//  - P (Projection Matrix) : diset di reshape() -> gluPerspective()
//  - V (View Matrix)       : diset di display() -> gluLookAt()
//  - M (Model Matrix)      : diset di display list -> glPushMatrix/glPopMatrix per-objek
//
// ============================================================

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ============================
    // VIEW MATRIX (V) - Kamera
    // ============================
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Hitung posisi kamera dari koordinat spherical
    float yawRad   = (float)(camYaw   * DEG2RAD);
    float pitchRad = (float)(camPitch * DEG2RAD);

    // Posisi mata kamera (eye) = pivot + offset spherical
    float eyeX = camPivot.x + camDist * cosf(pitchRad) * sinf(yawRad);
    float eyeY = camPivot.y + camDist * sinf(pitchRad);
    float eyeZ = camPivot.z + camDist * cosf(pitchRad) * cosf(yawRad);

    // gluLookAt menghasilkan View Matrix:
    //   eye    = posisi kamera di world space
    //   center = titik yang dilihat (pivot)
    //   up     = arah "atas" kamera
    gluLookAt(
        eyeX, eyeY, eyeZ,                          // Eye Position
        camPivot.x, camPivot.y, camPivot.z,         // Look-At Target (Pivot)
        0.0, 1.0, 0.0                               // Up Vector
    );

    // Set positions of lights in world coordinates (after gluLookAt)
    GLfloat light1Pos[] = { 15.0f, 11.5f, 140.0f, 1.0f }; //ruang kembar satunya
    GLfloat light2Pos[] = { -10.0f, 11.5f, 90.0f, 1.0f }; //ruang tengah persetan itu - orange
    GLfloat light3Pos[] = { -20.0f, 11.5f, 150.0f, 1.0f }; //semi outdoor - rot
    GLfloat light4Pos[] = { -8.0f, 11.5f, 67.0f, 1.0f }; //ruang kasir - grun
    GLfloat light5Pos[] = { 15.0f, 11.5f, 105.0f, 1.0f }; //wc - blau
    GLfloat light6Pos[] = { 18.0f, 11.5f, 80.0f, 1.0f }; //ruang jejer dapur? - purple
    GLfloat light7Pos[] = { 15.0f, 11.5f, 120.0f, 1.0f }; //ruang kembar - cyan
    
    glLightfv(GL_LIGHT1, GL_POSITION, light1Pos);
    glLightfv(GL_LIGHT2, GL_POSITION, light2Pos);
    glLightfv(GL_LIGHT3, GL_POSITION, light3Pos);
    glLightfv(GL_LIGHT4, GL_POSITION, light4Pos);
    glLightfv(GL_LIGHT5, GL_POSITION, light5Pos);
    glLightfv(GL_LIGHT6, GL_POSITION, light6Pos);
    glLightfv(GL_LIGHT7, GL_POSITION, light7Pos);

    // ============================
    // MODEL MATRIX (M) + RENDER
    // ============================

    // Set wireframe atau solid
    if (wireframeMode) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_LIGHTING);
    } else {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_LIGHTING);
    }

    // Render scene menggunakan Display List
    // Display List berisi Model Matrix transforms + geometry
    if (gSceneList) {
        glPushMatrix();
        // ---- MODEL MATRIX ----
        // Di sini kita bisa menerapkan transformasi Model:
        //   glTranslatef() -> Pindah posisi seluruh scene
        //   glRotatef()    -> Putar seluruh scene
        //   glScalef()     -> Skala seluruh scene
        // Untuk saat ini, model di-render pada posisi aslinya dari Blender
        // (identitas / tanpa transformasi tambahan)

        glCallList(gSceneList);
        glPopMatrix();
    }

    // Render Grid & Axis (bantuan visual)
    drawGrid();
    drawAxes();

    // Render bulbs (balls)
    struct LightInfo {
        GLenum id;
        float x, y, z;
        float r, g, b;
    };

    LightInfo lamps[] = {
        { GL_LIGHT1, 15.0f, 11.5f, 140.0f, 1.0f, 1.0f, 1.0f }, // Kembar2
        { GL_LIGHT2, -10.0f, 11.5f, 90.0f, 1.0f, 1.0f, 1.0f }, // Tengah
        { GL_LIGHT3, -20.0f, 11.5f, 150.0f, 1.0f, 1.0f, 1.0f }, // Semitruck
        { GL_LIGHT5, 15.0f, 11.5f, 105.0f, 1.0f, 1.0f, 1.0f }, // WC
        { GL_LIGHT6, 18.0f, 11.5f, 80.0f, 1.0f, 1.20f, 1.0f }, // SebalahDapur
        { GL_LIGHT7, 15.0f, 11.5f, 120.0f, 1.0f, 1.0f, 1.0f } // Kembar1
    };

    for (int i = 0; i < 7; ++i) {
        if (glIsEnabled(lamps[i].id)) {
            glPushMatrix();
            glDisable(GL_LIGHTING);
            glDisable(GL_TEXTURE_2D);

            glTranslatef(lamps[i].x, lamps[i].y, lamps[i].z);

            glColor4f(lamps[i].r, lamps[i].g, lamps[i].b, 1.0f);
            glutSolidSphere(1.0f, 16, 16);

            glEnable(GL_LIGHTING);
            glPopMatrix();
        }
    }

    // ============================
    // HUD (Informasi di layar 2D)
    // ============================
    drawHUD();

    // ============================
    // FPS Counter
    // ============================
    frameCount++;
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    if (currentTime - lastFPSTime >= 1000) {
        currentFPS = frameCount * 1000.0f / (float)(currentTime - lastFPSTime);
        lastFPSTime = currentTime;
        frameCount = 0;
    }

    glutSwapBuffers();
}


// ============================================================
// SECTION 12: RESHAPE CALLBACK --- PROJECTION MATRIX (P) ---
// ============================================================

void reshape(int w, int h) {
    if (h == 0) h = 1;
    winW = w;
    winH = h;

    float aspect = (float)w / (float)h;

    // Set Viewport
    glViewport(0, 0, w, h);

    // ============================
    // PROJECTION MATRIX (P)
    // ============================
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // gluPerspective menghasilkan Projection Matrix:
    //   fovy   = Field of View vertical (sudut pandang)
    //   aspect = Rasio lebar/tinggi window
    //   zNear  = Jarak bidang potong dekat
    //   zFar   = Jarak bidang potong jauh
    gluPerspective(
        45.0,       // FOV: 45 derajat
        aspect,     // Aspect Ratio
        2.0,        // Near Clipping Plane (Dinaikkan untuk mencegah Z-fighting / bolong)
        5000.0      // Far Clipping Plane (jauh karena scene besar)
    );

    // Kembali ke mode ModelView untuk transformasi V dan M
    glMatrixMode(GL_MODELVIEW);
}

// ============================================================
// SECTION 13: INPUT CALLBACKS
// ============================================================

// --- Keyboard biasa (key down) ---
void keyboard(unsigned char key, int x, int y) {
    keyState[key] = true;

    switch (key) {
        case '1':
            if (glIsEnabled(GL_LIGHT1)) glDisable(GL_LIGHT1); else glEnable(GL_LIGHT1);
            glutPostRedisplay();
            break;
        case '2':
            if (glIsEnabled(GL_LIGHT2)) glDisable(GL_LIGHT2); else glEnable(GL_LIGHT2);
            glutPostRedisplay();
            break;
        case '3':
            if (glIsEnabled(GL_LIGHT3)) glDisable(GL_LIGHT3); else glEnable(GL_LIGHT3);
            glutPostRedisplay();
            break;
        case '4':
            if (glIsEnabled(GL_LIGHT4)) glDisable(GL_LIGHT4); else glEnable(GL_LIGHT4);
            glutPostRedisplay();
            break;
        case '5':
            if (glIsEnabled(GL_LIGHT5)) glDisable(GL_LIGHT5); else glEnable(GL_LIGHT5);
            glutPostRedisplay();
            break;
        case '6':
            if (glIsEnabled(GL_LIGHT6)) glDisable(GL_LIGHT6); else glEnable(GL_LIGHT6);
            glutPostRedisplay();
            break;
        case '7':
            if (glIsEnabled(GL_LIGHT7)) glDisable(GL_LIGHT7); else glEnable(GL_LIGHT7);
            glutPostRedisplay();
            break;

        case 27: // ESC -> keluar
            exit(0);
            break;

        case 'z': case 'Z':
            wireframeMode = !wireframeMode;
            glutPostRedisplay();
            break;

        case 'b': case 'B':
            isDayTime = !isDayTime;
            if (isDayTime) {
                glClearColor(0.53f, 0.81f, 0.92f, 1.0f); // Siang
            } else {
                glClearColor(0.18f, 0.18f, 0.22f, 1.0f); // Malam
            }
            glutPostRedisplay();
            break;

        case 'r': case 'R':
            // Reset kamera ke posisi awal
            camPivot = sceneCenter;
            camYaw   = -45.0f;
            camPitch =  20.0f;
            {
                float dx = sceneBoundsMax.x - sceneBoundsMin.x;
                float dy = sceneBoundsMax.y - sceneBoundsMin.y;
                float dz = sceneBoundsMax.z - sceneBoundsMin.z;
                camDist = sqrtf(dx*dx + dy*dy + dz*dz) * 0.8f;
            }
            glutPostRedisplay();
            break;

        case '+': case '=':
            moveSpeed *= 1.5f;
            std::cout << "[SPEED] " << moveSpeed << std::endl;
            break;

        case '-': case '_':
            moveSpeed /= 1.5f;
            if (moveSpeed < 0.01f) moveSpeed = 0.01f;
            std::cout << "[SPEED] " << moveSpeed << std::endl;
            break;
    }
}

// --- Keyboard biasa (key up) ---
void keyboardUp(unsigned char key, int x, int y) {
    keyState[key] = false;
}

// --- Special Keyboard (key down) ---
void specialKey(int key, int x, int y) {
    if (key == GLUT_KEY_SHIFT_L) shiftDown = true;
}

// --- Special Keyboard (key up) ---
void specialKeyUp(int key, int x, int y) {
    if (key == GLUT_KEY_SHIFT_L) shiftDown = false;
}

// --- Mouse Button ---
void mouseButton(int button, int state, int x, int y) {
    // Simpan posisi mouse
    mouseLastX = x;
    mouseLastY = y;

    if (button == GLUT_LEFT_BUTTON) {
        mouseLeftDown = (state == GLUT_DOWN);
    }
    else if (button == GLUT_RIGHT_BUTTON) {
        mouseRightDown = (state == GLUT_DOWN);
    }
    else if (button == GLUT_MIDDLE_BUTTON) {
        mouseMiddleDown = (state == GLUT_DOWN);
    }

    // Scroll wheel (FreeGLUT: button 3 = scroll up, 4 = scroll down)
    if (button == 3 && state == GLUT_DOWN) {
        // Zoom In (10% lebih dekat)
        camDist *= 0.9f;
        camDist = clampf(camDist, 0.5f, 5000.0f);
        glutPostRedisplay();
    }
    else if (button == 4 && state == GLUT_DOWN) {
        // Zoom Out (10% lebih jauh)
        camDist *= 1.1f;
        camDist = clampf(camDist, 0.5f, 5000.0f);
        glutPostRedisplay();
    }
}

// --- Mouse Motion (drag) ---
void mouseMotion(int x, int y) {
    float dx = (float)(x - mouseLastX);
    float dy = (float)(y - mouseLastY);
    mouseLastX = x;
    mouseLastY = y;

    int mods = glutGetModifiers();
    bool shiftHeld = (mods & GLUT_ACTIVE_SHIFT) != 0;

    // ---- ORBIT (Rotasi mengelilingi pivot) ----
    // Left mouse drag TANPA shift = orbit
    // Middle mouse drag TANPA shift = orbit (Blender-style)
    if ((mouseLeftDown && !shiftHeld) || (mouseMiddleDown && !shiftHeld)) {
        camYaw   += dx * 0.4f;
        camPitch += dy * 0.4f;
        // Clamp pitch agar tidak flip
        camPitch = clampf(camPitch, -89.0f, 89.0f);
        glutPostRedisplay();
    }

    // ---- PAN (Geser titik pivot) ----
    // Right mouse drag = pan
    // Shift + Left/Middle mouse drag = pan (Blender-style)
    if (mouseRightDown || (mouseLeftDown && shiftHeld) || (mouseMiddleDown && shiftHeld)) {
        float panScale = camDist * 0.002f; // Pan speed proporsional dengan jarak

        float yawRad = (float)(camYaw * DEG2RAD);

        // Geser pivot dalam arah "kanan" kamera (screen X)
        float rightX =  cosf(yawRad);
        float rightZ = -sinf(yawRad);
        camPivot.x -= rightX * dx * panScale;
        camPivot.z -= rightZ * dx * panScale;

        // Geser pivot dalam arah "atas" (screen Y -> world Y)
        camPivot.y += dy * panScale;

        glutPostRedisplay();
    }
}

// --- Mouse Wheel (FreeGLUT extension) ---
void mouseWheel(int wheel, int direction, int x, int y) {
    if (direction > 0) {
        camDist *= 0.9f;  // Zoom in
    } else {
        camDist *= 1.1f;  // Zoom out
    }
    camDist = clampf(camDist, 0.5f, 5000.0f);
    glutPostRedisplay();
}

// ============================================================
// SECTION 14: UPDATE / TIMER (Game Loop untuk WASD)
// ============================================================

void update(int value) {
    bool needRedraw = false;

    // Hitung arah kamera untuk WASD movement
    float yawRad = (float)(camYaw * DEG2RAD);

    // Forward = arah dari kamera ke pivot (proyeksi XZ)
    float fwdX = -sinf(yawRad);
    float fwdZ = -cosf(yawRad);

    // Right = tegak lurus forward (proyeksi XZ)
    float rightX =  cosf(yawRad);
    float rightZ = -sinf(yawRad);

    // WASD Movement (gerakkan pivot)
    if (keyState['w'] || keyState['W']) {
        camPivot.x += fwdX * moveSpeed;
        camPivot.z += fwdZ * moveSpeed;
        needRedraw = true;
    }
    if (keyState['s'] || keyState['S']) {
        camPivot.x -= fwdX * moveSpeed;
        camPivot.z -= fwdZ * moveSpeed;
        needRedraw = true;
    }
    if (keyState['a'] || keyState['A']) {
        camPivot.x -= rightX * moveSpeed;
        camPivot.z -= rightZ * moveSpeed;
        needRedraw = true;
    }
    if (keyState['d'] || keyState['D']) {
        camPivot.x += rightX * moveSpeed;
        camPivot.z += rightZ * moveSpeed;
        needRedraw = true;
    }
    if (keyState[' ']) {
        camPivot.y += moveSpeed;
        needRedraw = true;
    }
    if (shiftDown) {
        camPivot.y -= moveSpeed;
        needRedraw = true;
    }

    if (needRedraw) {
        glutPostRedisplay();
    }

    // Timer berikutnya
    glutTimerFunc(16, update, 0);
}

// ============================================================
// SECTION 15: MAIN FUNCTION
// ============================================================

int main(int argc, char** argv) {
    // Inisialisasi GLUT
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(winW, winH);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("Grafkom | 0_[-MOYA-]_0 |");

    // Inisialisasi OpenGL
    initGL();

    // ============================================================
    // LOAD SCENE DARI FILE OBJ
    // ============================================================
    // File .OBJ dan .MTL hasil export dari Blender
    // OBJ berisi geometri (vertex, normal, texcoord, face)
    // MTL berisi material dan referensi texture

    std::cout << "========================================" << std::endl;
    std::cout << " LOADING SCENE" << std::endl;
    std::cout << "========================================" << std::endl;

    // Path Obj
    // File MTL akan otomatis di-load melalui directive "mtllib" di dalam OBJ
    if (!loadOBJ("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\test.obj")) {
        std::cerr << "FATAL: Gagal memuat file OBJ!" << std::endl;
        std::cerr << "Pastikan file 'object/test.obj' dan 'object/test.mtl' ada." << std::endl;
        return 1;
    }

    // Buat Display List untuk rendering cepat
    buildSceneDisplayList();

    std::cout << "========================================" << std::endl;
    std::cout << " SCENE READY! Gunakan mouse & WASD." << std::endl;
    std::cout << "========================================" << std::endl;

    // ============================================================
    // REGISTER CALLBACK FUNCTIONS
    // ============================================================
    glutDisplayFunc(display);        // Render callback
    glutReshapeFunc(reshape);        // Window resize callback (Projection Matrix)
    glutKeyboardFunc(keyboard);      // Keyboard key-down
    glutKeyboardUpFunc(keyboardUp);  // Keyboard key-up
    glutSpecialFunc(specialKey);     // Special key-down (Shift)
    glutSpecialUpFunc(specialKeyUp); // Special key-up
    glutMouseFunc(mouseButton);      // Mouse button
    glutMotionFunc(mouseMotion);     // Mouse drag
    glutMouseWheelFunc(mouseWheel);  // Scroll wheel (FreeGLUT)

    // Timer untuk WASD movement (game loop)
    glutTimerFunc(16, update, 0);

    // ============================================================
    // MULAI MAIN LOOP
    // ============================================================
    glutMainLoop();

    return 0;
}