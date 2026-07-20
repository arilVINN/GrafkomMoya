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
    float alpha      = 1.0f;

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

        if (alpha < 1.0f) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE); // Mencegah kaca menutupi objek di belakangnya
        }

        if (textureID != 0) {
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glColor4f(1.0f, 1.0f, 1.0f, alpha);
        } else {
            glDisable(GL_TEXTURE_2D);
            glColor4f(0.8f, 0.8f, 0.8f, alpha);
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

        if (alpha < 1.0f) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE); // Kembalikan efek depth buffer
        }

        glPopMatrix();
    }
};

std::vector<TexturedGameObject> sceneObjects;

// ============================================================
// SISTEM KAMERA BLENDER (ORBIT, PAN, & WASD NAVIGATION)
// ============================================================
float camAngleX = 25.0f;
float camAngleY = -45.0f;
float camDist = 18.0f;

float targetX = 0.0f;
float targetY = 0.0f;
float targetZ = 7.0f; 

const float minCamDist = 1.0f;
const float maxCamDist = 1000.0f;

int lastMouseX, lastMouseY;
bool isRotateDragging = false;
bool isPanDragging = false;

void initGL() {
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); // Memastikan glColor bisa mengatur warna material saat ada cahaya
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

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
    gluPerspective(45.0, (float)w / (float)h, 0.1, 1000.0);
    
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

    // Klik Kiri + Drag: Orbit Kamera
    if (isRotateDragging) {
        camAngleY += deltaX * 0.5f;
        camAngleX += deltaY * 0.5f;

        if (camAngleX > 89.0f) camAngleX = 89.0f;
        if (camAngleX < -89.0f) camAngleX = -89.0f;

        glutPostRedisplay();
    }
    // Klik Kanan + Drag: Pan Kamera (Relatif terhadap sudut rotasi)
    else if (isPanDragging) {
        float panSpeed = 0.003f * camDist;

        float radY = camAngleY * 3.14159265f / 180.0f;

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

// ============================================================
// PERGERAKAN KAMERA WASD (KEYBOARD)
// ============================================================
void keyboard(unsigned char key, int x, int y) {
    float moveSpeed = 0.8f; // Kecepatan gerak kamera
    float radY = camAngleY * 3.14159265f / 180.0f;

    // Vektor arah Maju (Forward) & Kanan (Right) relatif terhadap sudut rotasi kamera
    float forwardX = sin(radY);
    float forwardZ = -cos(radY);

    float rightX = cos(radY);
    float rightZ = sin(radY);

    switch (tolower(key)) {
    case 'w': // Maju
        targetX += forwardX * moveSpeed;
        targetZ += forwardZ * moveSpeed;
        break;
    case 's': // Mundur
        targetX -= forwardX * moveSpeed;
        targetZ -= forwardZ * moveSpeed;
        break;
    case 'a': // Geser Kiri (Strafe Left)
        targetX -= rightX * moveSpeed;
        targetZ -= rightZ * moveSpeed;
        break;
    case 'd': // Geser Kanan (Strafe Right)
        targetX += rightX * moveSpeed;
        targetZ += rightZ * moveSpeed;
        break;
    case 'e': // Naik Vertikal (Atas)
        targetY += moveSpeed;
        break;
    case 'q': // Turun Vertikal (Bawah)
        targetY -= moveSpeed;
        break;
    }

    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("TR Grafika Komputer | 0-_[Moya Caffe]_-0 |");

    initGL();

    auto loadObject = [&](const std::string& objPath,
                          const std::string& texPath,
                          const Vector3& pos = { 0.0f, 0.0f, 0.0f },
                          const Vector3& rot = { 0.0f, 0.0f, 0.0f },
                          const Vector3& scale = { 1.0f, 1.0f, 1.0f },
                          float alpha = 1.0f) {
        TexturedGameObject obj;
        if (obj.loadOBJ(objPath.c_str())) {
            if (!texPath.empty()) {
                obj.loadTexture(texPath.c_str());
            }
            obj.position = pos;
            obj.rotation = rot;
            obj.scale = scale;
            obj.alpha = alpha;
            sceneObjects.push_back(obj);
        }
    };

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\FloorIndoorRoom.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\FloorTiles.png",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.9f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\TembokMeratap1.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wall1.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\TembokMeratap2.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wall1.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\DoorCurtains.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\curtain.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WindowsPlane.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\OldWindows.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\TembokMeratap1.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wall1.jpg",
               { 10.0f, 0.0f, -33.2f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Painting1.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\TexturePainting1.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\sofa3.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\fabric.jpg");

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\MejaKayuKotak.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 0.0f, 0.0f, 7.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\sofa3.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\fabric.jpg",
               { 0.0f, 0.0f, 14.0f },
               { 0.0f, 180.0f, 0.0f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 16.0f, 0.0f, 7.0f },
               { 0.0f, 180.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 16.0f, 0.0f, -5.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Drawer.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\leather.jpg",
               { 12.0f, 0.0f, -14.0f },
               { 0.0f, -90.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });


    //Room2
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WallRoom2.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wall1.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\DoorCurtains.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\curtain.jpg",
               { 10.0f, 0.0f, -10.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WindowsPlane.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\OldWindows.jpg",
               { 10.0f, 0.0f, -12.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 18.0f, 0.0f, -40.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 2.0f, 0.0f, -40.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 12.0f, 0.0f, -100.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\SetMejaMakanKayuIndoor.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 12.0f, 0.0f, -85.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });
        
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\sofa3.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\fabric.jpg",
               { 23.0f, 0.0f, -23.0f },
               { 0.0f, -90.0f, 0.0f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Drawer.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\leather.jpg",
               { -5.0f, 0.0f, -28.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.2f, 1.2f, 1.2f });
    
    //WC
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WC.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wall1.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Step.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\leatherRed.png",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Toilet.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\ToiletTile.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\ToiletTile.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //Hallway
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WallHall.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wall1.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WallHallDetail.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\TembokMeratap3.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wall1.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\TembokMeratap4.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\TembokMeratap5.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //doorframe
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\DoorFrame.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //Meja Kasir
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\MejaKasirBawah.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\MejaKasirAtas.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\ObjKasirBawah.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\leather.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\ObjKasirAtas.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\CofeeMicrowave.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 11.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\CofeeBlenderBawah.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\CofeeBlenderAtas.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\CofeeBlenderBawah.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 4.5f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\CofeeBlenderAtas.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 4.5f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
               
    //Furnitur Ruang Kasir
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\KursiKayuKasir.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\MejaBesiKasir.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\SofaKasir.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\fabric.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\KursiKotakBaseKasir.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\KursiKotakPillowKasir.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\fabric.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\MejaKayuKotakKasir.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\fabric.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });



    //Pintu Luar
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\PintuLuar.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    //kaca
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Kaca.obj",
               "", // Dihilangkan (dikosongkan) agar tidak pakai tekstur solid
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f },
               0.7f); // Ubah alpha ke 0.3 agar lebih bening (transparan)

    //Pillar
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\PillarLuar.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    
    //TembokLuar
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\TembokLuar1.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    //Garasi
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Garasi.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //Floor Launge
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\FloorLaunge.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\leather.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //wall
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WallLaunge.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //FloorLuar
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\FloorLuarHitam.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\FloorLuarSemen.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Carpet.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\leatherRed.png",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //atap
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\Atap.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //OpenArea
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WallOpenArea.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WallOpenArea2.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\WallOpenArea3.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });

    //Furnitur
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\KursiLuar.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\MejaBundarLuar.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\white.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });



    //floor
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\FloorOpenArea.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\fabric.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\FloorTileOpenArea.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\wood.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });
    loadObject("C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\object\\FloorKrikil.obj",
               "C:\\Users\\kevin\\Documents\\Grfk\\TRGrafkom\\Texture\\AtapAncur.jpg",
               { 10.0f, 0.0f, 2.0f },
               { 0.0f, 0.0f, 0.0f },
               { 1.8f, 1.8f, 1.8f });


    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutMouseWheelFunc(mouseWheel);
    glutKeyboardFunc(keyboard);

    glutMainLoop();
    return 0;
}