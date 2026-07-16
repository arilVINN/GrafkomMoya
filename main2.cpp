#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cmath> // Untuk std::cos dan std::sin

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Vector3 {
    float x, y, z;
};

struct Vector2 {
    float u, v;
};

struct FaceIndex {
    int vIdx;
    int vtIdx;
    int vnIdx;
};

struct Face {
    std::vector<FaceIndex> indices;
};

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
            std::cerr << "Gagal memuat tekstur: " << imagePath << std::endl;
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
            std::cerr << "Gagal membuka file OBJ: " << path << std::endl;
            return false;
        }

        vertices.clear();
        texCoords.clear();
        normals.clear();
        faces.clear();

        std::string line;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string type;
            ss >> type;

            if (type == "v") {
                Vector3 v;
                ss >> v.x >> v.y >> v.z;
                vertices.push_back(v);
            }
            else if (type == "vt") {
                Vector2 vt;
                ss >> vt.u >> vt.v;
                texCoords.push_back(vt);
            }
            else if (type == "vn") {
                Vector3 vn;
                ss >> vn.x >> vn.y >> vn.z;
                normals.push_back(vn);
            }
            else if (type == "f") {
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

                if (idx.vnIdx >= 0 && idx.vnIdx < normals.size()) {
                    Vector3 vn = normals[idx.vnIdx];
                    glNormal3f(vn.x, vn.y, vn.z);
                }

                if (idx.vtIdx >= 0 && idx.vtIdx < texCoords.size()) {
                    Vector2 vt = texCoords[idx.vtIdx];
                    glTexCoord2f(vt.u, vt.v);
                }

                if (idx.vIdx >= 0 && idx.vIdx < vertices.size()) {
                    Vector3 v = vertices[idx.vIdx];
                    glVertex3f(v.x, v.y, v.z);
                }
            }
            glEnd();
        }

        if (textureID != 0) {
            glDisable(GL_TEXTURE_2D);
        }

        glPopMatrix();
    }
};

std::vector<TexturedGameObject> sceneObjects;

// ============================================================
// SISTEM KAMERA BLENDER (SMOOTH ORBIT & RELATIVE PAN)
// ============================================================
float camAngleX = 25.0f;
float camAngleY = -45.0f;
float camDist = 18.0f;

float targetX = 0.0f;
float targetY = 0.0f;
float targetZ = 7.0f; // Diset di tengah area scene (antara Z=0 hingga Z=14)

const float minCamDist = 1.0f;
const float maxCamDist = 100.0f;

int lastMouseX, lastMouseY;
bool isRotateDragging = false;
bool isPanDragging = false;

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

    // --- KAMERA VIEW MATRIX ---
    glTranslatef(0.0f, 0.0f, -camDist);
    glRotatef(camAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(camAngleY, 0.0f, 1.0f, 0.0f);
    glTranslatef(-targetX, -targetY, -targetZ);

    // --- RENDER SEMUA OBJEK ---
    for (auto& obj : sceneObjects) {
        obj.draw();
    }

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
        lastMouseX = x;
        lastMouseY = y;
    }
    else if (button == GLUT_RIGHT_BUTTON) {
        isPanDragging = (state == GLUT_DOWN);
        lastMouseX = x;
        lastMouseY = y;
    }
}

void motion(int x, int y) {
    int deltaX = x - lastMouseX;
    int deltaY = y - lastMouseY;

    // Klik Kiri + Drag: Memutar Kamera Mengelilingi Pivot Point
    if (isRotateDragging) {
        camAngleY += deltaX * 0.5f;
        camAngleX += deltaY * 0.5f;

        if (camAngleX > 89.0f) camAngleX = 89.0f;
        if (camAngleX < -89.0f) camAngleX = -89.0f;

        glutPostRedisplay();
    }
    // Klik Kanan + Drag: Pan Kamera Relatif terhadap Rotasi Kamera
    else if (isPanDragging) {
        float panSpeed = 0.003f * camDist;

        float radY = camAngleY * 3.14159265f / 180.0f;

        // Vektor arah kanan relatif terhadap rotasi horizontal kamera
        float rightX = cos(radY);
        float rightZ = sin(radY);

        targetX -= (deltaX * rightX) * panSpeed;
        targetZ -= (deltaX * rightZ) * panSpeed;
        targetY += deltaY * panSpeed;

        glutPostRedisplay();
    }

    lastMouseX = x;
    lastMouseY = y;
}

void mouseWheel(int wheel, int direction, int x, int y) {
    if (direction > 0) {
        camDist -= 1.5f;
        if (camDist < minCamDist) camDist = minCamDist;
    } 
    else {
        camDist += 1.5f;
        if (camDist > maxCamDist) camDist = maxCamDist;
    }
    
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("TR Grafika - Precise Blender Camera Control");

    initGL();

    auto loadObject = [&](const std::string& objPath,
                          const std::string& texPath,
                          const Vector3& pos = { 0.0f, 0.0f, 0.0f },
                          const Vector3& rot = { 0.0f, 0.0f, 0.0f },
                          const Vector3& scale = { 1.0f, 1.0f, 1.0f }) {
        TexturedGameObject obj;
        if (obj.loadOBJ(objPath.c_str())) {
            if (!texPath.empty()) {
                obj.loadTexture(texPath.c_str());
            }
            obj.position = pos;
            obj.rotation = rot;
            obj.scale = scale;
            sceneObjects.push_back(obj);
        }
    };

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\object\\FloorIndoorRoom.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Texture\\FloorTiles.png",
               { 10.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 0.0f },
            { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\object\\sofa3.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Texture\\fabric.jpg");

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\object\\MejaKayuKotak.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Texture\\wood.jpg",
               { 0.0f, 0.0f, 7.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\object\\sofa3.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Texture\\fabric.jpg",
               { 0.0f, 0.0f, 14.0f },
               { 0.0f, 180.0f, 0.0f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Texture\\wood.jpg",
               { 16.0f, 0.0f, 7.0f },
               { 0.0f, 180.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Texture\\wood.jpg",
               { 16.0f, 0.0f, -5.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Drawer.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\leather.jpg",
               { 12.0f, 0.0f, -14.0f },
               { 0.0f, -90.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMouseWheelFunc(mouseWheel);

    glutMainLoop();
    return 0;
}