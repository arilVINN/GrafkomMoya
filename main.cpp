#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ============================================================
// STRUKTUR DATA DASAR
// ============================================================
struct Vector3 {
    float x, y, z;
};

struct Vector2 {
    float u, v;
};

struct FaceIndex {
    int vIdx, vtIdx, vnIdx;
};

struct Face {
    std::vector<FaceIndex> indices;
};

// Config pendukung untuk pendaftaran objek yang lebih rapi
struct ObjectConfig {
    std::string objPath;
    std::string texPath;
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 rot = { 0.0f, 0.0f, 0.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
};

// ============================================================
// CLASS OBJECT (TexturedGameObject)
// ============================================================
class TexturedGameObject {
public:
    std::string name;
    std::vector<Vector3> vertices;
    std::vector<Vector2> texCoords;
    std::vector<Vector3> normals;
    std::vector<Face> faces;
    GLuint textureID = 0;

    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    Vector3 scale    = { 1.0f, 1.0f, 1.0f };

    bool loadTexture(const char* imagePath) {
        int width, height, nrChannels;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(imagePath, &width, &height, &nrChannels, 0);

        if (!data) {
            std::cerr << "[Warning] Gagal memuat tekstur: " << imagePath << std::endl;
            return false;
        }

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);

        stbi_image_free(data);
        return true;
    }

    bool loadOBJ(const char* path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[Error] Gagal membuka file OBJ: " << path << std::endl;
            return false;
        }

        vertices.clear(); texCoords.clear(); normals.clear(); faces.clear();
        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v") {
                Vector3 v; ss >> v.x >> v.y >> v.z;
                vertices.push_back(v);
            } else if (type == "vt") {
                Vector2 vt; ss >> vt.u >> vt.v;
                texCoords.push_back(vt);
            } else if (type == "vn") {
                Vector3 vn; ss >> vn.x >> vn.y >> vn.z;
                normals.push_back(vn);
            } else if (type == "f") {
                Face face;
                std::string segment;
                while (ss >> segment) {
                    std::stringstream segmentSS(segment);
                    std::string vStr, vtStr, vnStr;
                    std::getline(segmentSS, vStr, '/');
                    std::getline(segmentSS, vtStr, '/');
                    std::getline(segmentSS, vnStr, '/');

                    FaceIndex idx;
                    idx.vIdx  = !vStr.empty()  ? std::stoi(vStr) - 1  : -1;
                    idx.vtIdx = !vtStr.empty() ? std::stoi(vtStr) - 1 : -1;
                    idx.vnIdx = !vnStr.empty() ? std::stoi(vnStr) - 1 : -1;
                    face.indices.push_back(idx);
                }
                faces.push_back(face);
            }
        }
        file.close();
        return true;
    }

    void draw() {
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rotation.z, 0.0f, 0.0f, 1.0f);
        glScalef(scale.x, scale.y, scale.z);

        if (textureID != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glColor3f(1.0f, 1.0f, 1.0f);
        } else {
            glDisable(GL_TEXTURE_2D);
            glColor3f(0.8f, 0.8f, 0.8f);
        }

        for (const auto& face : faces) {
            glBegin(GL_POLYGON);
            for (const auto& idx : face.indices) {
                if (idx.vnIdx >= 0 && idx.vnIdx < (int)normals.size()) {
                    glNormal3f(normals[idx.vnIdx].x, normals[idx.vnIdx].y, normals[idx.vnIdx].z);
                }
                if (idx.vtIdx >= 0 && idx.vtIdx < (int)texCoords.size()) {
                    glTexCoord2f(texCoords[idx.vtIdx].u, texCoords[idx.vtIdx].v);
                }
                if (idx.vIdx >= 0 && idx.vIdx < (int)vertices.size()) {
                    glVertex3f(vertices[idx.vIdx].x, vertices[idx.vIdx].y, vertices[idx.vIdx].z);
                }
            }
            glEnd();
        }

        if (textureID != 0) glDisable(GL_TEXTURE_2D);
        glPopMatrix();
    }
};

// ============================================================
// ARSITEKTUR RUANGAN (ROOM & SCENE MANAGER)
// ============================================================
class Room {
public:
    std::string roomName;
    Vector3 roomOffset; // Offset lokasi awal untuk seluruh ruangan ini
    std::vector<TexturedGameObject> objects;

    Room(const std::string& name, Vector3 offset = { 0.0f, 0.0f, 0.0f }) 
        : roomName(name), roomOffset(offset) {}

    void addObject(const ObjectConfig& config) {
        TexturedGameObject obj;
        if (obj.loadOBJ(config.objPath.c_str())) {
            if (!config.texPath.empty()) {
                obj.loadTexture(config.texPath.c_str());
            }
            // Posisi dihitung relatif terhadap offset ruangan
            obj.position = { 
                config.pos.x + roomOffset.x, 
                config.pos.y + roomOffset.y, 
                config.pos.z + roomOffset.z 
            };
            obj.rotation = config.rot;
            obj.scale = config.scale;
            objects.push_back(obj);
        }
    }

    void draw() {
        for (auto& obj : objects) {
            obj.draw();
        }
    }
};

class SceneManager {
public:
    std::vector<Room> rooms;

    void addRoom(const Room& room) {
        rooms.push_back(room);
    }

    void drawAll() {
        for (auto& room : rooms) {
            room.draw();
        }
    }
};

SceneManager g_Scene;

// ============================================================
// KONTROL KAMERA & MOUSE
// ============================================================
float camAngleX = 25.0f;
float camAngleY = -45.0f;
float camDist = 18.0f;

float targetX = 0.0f, targetY = 0.0f, targetZ = 7.0f;
const float minCamDist = 1.0f, maxCamDist = 100.0f;

int lastMouseX, lastMouseY;
bool isRotateDragging = false, isPanDragging = false;

void initGL() {
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat lightPos[] = { 10.0f, 20.0f, 10.0f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Transformasi Kamera
    glTranslatef(0.0f, 0.0f, -camDist);
    glRotatef(camAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(camAngleY, 0.0f, 1.0f, 0.0f);
    glTranslatef(-targetX, -targetY, -targetZ);

    // Render Seluruh Ruangan via Manager
    g_Scene.drawAll();

    glutSwapBuffers();
}

void reshape(int w, int h) {
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);
    
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (float)w / (float)h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        isRotateDragging = (state == GLUT_DOWN);
    } else if (button == GLUT_RIGHT_BUTTON) {
        isPanDragging = (state == GLUT_DOWN);
    }
    lastMouseX = x;
    lastMouseY = y;
}

void motion(int x, int y) {
    int deltaX = x - lastMouseX;
    int deltaY = y - lastMouseY;

    if (isRotateDragging) {
        camAngleY += deltaX * 0.5f;
        camAngleX += deltaY * 0.5f;
        if (camAngleX > 89.0f) camAngleX = 89.0f;
        if (camAngleX < -89.0f) camAngleX = -89.0f;
        glutPostRedisplay();
    } else if (isPanDragging) {
        float panSpeed = 0.003f * camDist;
        float radY = camAngleY * 3.14159265f / 180.0f;

        float rightX = cos(radY);
        float rightZ = sin(radY);

        targetX -= (deltaX * rightX) * panSpeed;
        targetZ -= (deltaX * rightZ) * panSpeed;
        targetY += deltaY * panSpeed;
        glutPostRedisplay();
    }

    lastMouseX = x; lastMouseY = y;
}

void mouseWheel(int wheel, int direction, int x, int y) {
    if (direction > 0) {
        camDist -= 1.5f;
        if (camDist < minCamDist) camDist = minCamDist;
    } else {
        camDist += 1.5f;
        if (camDist > maxCamDist) camDist = maxCamDist;
    }
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    float moveSpeed = 0.8f;
    float radY = camAngleY * 3.14159265f / 180.0f;

    float forwardX = sin(radY);
    float forwardZ = -cos(radY);
    float rightX = cos(radY);
    float rightZ = sin(radY);

    switch (tolower(key)) {
    case 'w': targetX += forwardX * moveSpeed; targetZ += forwardZ * moveSpeed; break;
    case 's': targetX -= forwardX * moveSpeed; targetZ -= forwardZ * moveSpeed; break;
    case 'a': targetX -= rightX * moveSpeed; targetZ -= rightZ * moveSpeed; break;
    case 'd': targetX += rightX * moveSpeed; targetZ += rightZ * moveSpeed; break;
    case 'e': targetY += moveSpeed; break;
    case 'q': targetY -= moveSpeed; break;
    }
    glutPostRedisplay();
}

// ============================================================
// PEMBUATAN STRUKTUR SCENE & RUANGAN
// ============================================================
void buildScene() {
    // 1. STRUKTUR UTAMA / BANGUNAN DINDING & LANTAI
    Room mainBuilding("Bangunan Utama");
    mainBuilding.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\FloorIndoorRoom.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/FloorTiles.png", {10,0,2}, {0,0,0}, {1.8f, 1.9f, 1.8f} });
    mainBuilding.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\TembokMeratap1.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/wall1.jpg", {10,0,2}, {0,0,0}, {1.8f, 1.8f, 1.8f} });
    mainBuilding.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\TembokMeratap1.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/wall1.jpg", {10,0,-33.2f}, {0,0,0}, {1.8f, 1.8f, 1.8f} });
    g_Scene.addRoom(mainBuilding);

    // 2. RUANG SANTAI / LOUNGE (Dua Sofa + Meja)
    Room loungeRoom("Lounge Area");
    loungeRoom.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\sofa3.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/fabric.jpg", {0, 0, 0} });
    loungeRoom.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\MejaKayuKotak.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/wood.jpg", {0, 0, 7}, {0, 0, 0}, {1.2f, 1.2f, 1.2f} });
    loungeRoom.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\sofa3.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/fabric.jpg", {0, 0, 14}, {0, 180, 0} });
    g_Scene.addRoom(loungeRoom);

    // 3. AREA MAKAN (DINING AREA)
    Room diningRoom("Dining Area", { 16.0f, 0.0f, 0.0f }); // Offset X diset 16.0f
    diningRoom.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\SetMejaMakanKayuIndoor.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/wood.jpg", {0, 0, 7}, {0, 180, 0}, {1.2f, 1.2f, 1.2f} });
    diningRoom.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\SetMejaMakanKayuIndoor.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/wood.jpg", {0, 0, -5}, {0, 0, 0}, {1.2f, 1.2f, 1.2f} });
    g_Scene.addRoom(diningRoom);

    // 4. AREA PENYIMPANAN / DRAWER
    Room storageRoom("Storage Area");
    storageRoom.addObject({ "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\objects\\Drawer.obj", "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture/leather.jpg", {12, 0, -14}, {0, -90, 0}, {1.2f, 1.2f, 1.2f} });
    g_Scene.addRoom(storageRoom);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("TR Grafika Komputer | Moya Caffe |");

    initGL();
    
    // Panggil pembuat scene
    buildScene();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMouseWheelFunc(mouseWheel);
    glutKeyboardFunc(keyboard);

    glutMainLoop();
    return 0;
}