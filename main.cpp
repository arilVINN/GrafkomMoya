#include <GL/freeglut.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>

// Struktur Data Vektor 3D
struct Vector3 {
    float x, y, z;
};

// Struktur Data Face
struct Face {
    std::vector<int> vertexIndices;
    std::vector<int> normalIndices;
};

// Kelas untuk menampung 1 Objek 3D beserta Transformasi (Model Matrix)
class GameObject3D {
public:
    std::string name;
    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Face> faces;

    // Transformasi Objek (Model Space -> World Space)
    Vector3 position = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f };
    Vector3 scale    = { 1.0f, 1.0f, 1.0f };
    Vector3 color    = { 0.8f, 0.8f, 0.8f }; // Warna objek

    // Fungsi membaca file .OBJ
    bool loadOBJ(const char* path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Gagal membuka file: " << path << std::endl;
            return false;
        }

        vertices.clear();
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
                    std::string vIdxStr, vtIdxStr, vnIdxStr;

                    std::getline(segmentSS, vIdxStr, '/');
                    std::getline(segmentSS, vtIdxStr, '/');
                    std::getline(segmentSS, vnIdxStr, '/');

                    if (!vIdxStr.empty()) face.vertexIndices.push_back(std::stoi(vIdxStr) - 1);
                    if (!vnIdxStr.empty()) face.normalIndices.push_back(std::stoi(vnIdxStr) - 1);
                }
                faces.push_back(face);
            }
        }
        file.close();
        std::cout << "Berhasil memuat " << path << " (" << vertices.size() << " Vertices)" << std::endl;
        return true;
    }

    // Fungsi Render Objek dengan menerapkan Model Matrix
    void draw() {
        glPushMatrix(); // Save Matriks World/View

        // --- MODEL MATRIX TRANSFORMATIONS ---
        // 1. Translasi (Pindah Posisi)
        glTranslatef(position.x, position.y, position.z);

        // 2. Rotasi
        glRotatef(rotation.x, 1.0f, 0.0f, 0.0f);
        glRotatef(rotation.y, 0.0f, 1.0f, 0.0f);
        glRotatef(rotation.z, 0.0f, 0.0f, 1.0f);

        // 3. Skala
        glScalef(scale.x, scale.y, scale.z);

        // Set Warna
        glColor3f(color.x, color.y, color.z);

        // Drawing Geometri
        for (const auto& face : faces) {
            glBegin(GL_POLYGON);
            for (size_t i = 0; i < face.vertexIndices.size(); ++i) {
                if (i < face.normalIndices.size() && face.normalIndices[i] < normals.size()) {
                    Vector3 vn = normals[face.normalIndices[i]];
                    glNormal3f(vn.x, vn.y, vn.z);
                }
                Vector3 v = vertices[face.vertexIndices[i]];
                glVertex3f(v.x, v.y, v.z);
            }
            glEnd();
        }

        glPopMatrix(); // Restore Matriks World/View
    }
};

// --- DAFTAR SEMUA OBJEK DI SCENE ---
std::vector<GameObject3D> sceneObjects;

// Kontrol Kamera (View Matrix)
float camAngleX = 20.0f;
float camAngleY = -45.0f;
float camDistance = 15.0f;
int lastMouseX, lastMouseY;
bool isDragging = false;

void initGL() {
    glClearColor(0.12f, 0.12f, 0.15f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat lightPos[] = { 10.0f, 20.0f, 10.0f, 1.0f };
    GLfloat ambientLight[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat diffuseLight[] = { 0.8f, 0.8f, 0.8f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
}

// 1. PROJECTION MATRIX (P)
void reshape(int w, int h) {
    if (h == 0) h = 1;
    float aspect = (float)w / (float)h;

    glViewport(0, 0, w, h);
    
    // Set Projection Matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity(); // Reset Projection Matrix
    gluPerspective(45.0, aspect, 0.1, 100.0); // Perspektif Proyeksi

    // Kembali ke Modelview
    glMatrixMode(GL_MODELVIEW);
}

// 2. VIEW MATRIX (V) & 3. MODEL MATRIX (M)
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity(); // Reset Modelview Matrix

    // --- VIEW MATRIX (Kamera) ---
    // Atur posisi kamera melihat ke pusat (0,0,0)
    glTranslatef(0.0f, -2.0f, -camDistance);
    glRotatef(camAngleX, 1.0f, 0.0f, 0.0f);
    glRotatef(camAngleY, 0.0f, 1.0f, 0.0f);

    // --- MODEL MATRIX & RENDER ALL OBJECTS ---
    // Loop dan render setiap objek yang ada di scene
    for (auto& obj : sceneObjects) {
        obj.draw();
    }

    glutSwapBuffers();
}

void mouse(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON) {
        if (state == GLUT_DOWN) {
            isDragging = true;
            lastMouseX = x;
            lastMouseY = y;
        } else if (state == GLUT_UP) {
            isDragging = false;
        }
    }
}

void motion(int x, int y) {
    if (isDragging) {
        camAngleY += (x - lastMouseX) * 0.5f;
        camAngleX += (y - lastMouseY) * 0.5f;
        lastMouseX = x;
        lastMouseY = y;
        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1024, 768);
    glutCreateWindow("TR Grafika Komputer - Multi OBJ dengan MVP");

    initGL();

    // ============================================================
    // MEMASUKKAN BANYAK OBJEK DAN MENGATUR POSISI (MODEL MATRIX)
    // ============================================================

    // OBJEK 1: Pondasi / Tiang Utama (Gunakan file Untitled.obj kamu)
    GameObject3D tiangUtama;
    if (tiangUtama.loadOBJ("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\sofa2.obj")) {
        tiangUtama.name = "Tiang Utama";
        tiangUtama.position = { 0.0f, 0.0f, 0.0f };  // Posisikan di tengah
        tiangUtama.scale    = { 1.0f, 1.0f, 1.0f };
        tiangUtama.color    = { 0.8f, 0.3f, 0.3f };  // Warna Merah
        sceneObjects.push_back(tiangUtama);
    }

    // OBJEK 2: Tiang Kiri (Memakai file yang sama, tapi beda posisi)
    GameObject3D tiangKiri;
    if (tiangKiri.loadOBJ("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Untitled1.obj")) {
        tiangKiri.name = "Tiang Kiri";
        tiangKiri.position = { -4.0f, 0.0f, 0.0f }; // Digeser ke kiri (X = -4)
        tiangKiri.scale    = { 0.8f, 0.8f, 0.8f };  // Agak kecil
        tiangKiri.color    = { 0.3f, 0.8f, 0.3f };  // Warna Hijau
        sceneObjects.push_back(tiangKiri);
    }

    // OBJEK 3: Tiang Kanan (Gunakan file OBJ lain jika ada)
    GameObject3D tiangKanan;
    if (tiangKanan.loadOBJ("C:\\Users\\kevin\\Documents\\Grfk\\TRcoba\\Untitled2.obj")) {
        tiangKanan.name = "Tiang Kanan";
        tiangKanan.position = { 4.0f, 0.0f, 0.0f };  // Digeser ke kanan (X = 4)
        tiangKanan.rotation = { 0.0f, 45.0f, 0.0f }; // Diputar 45 derajat
        tiangKanan.color    = { 0.3f, 0.3f, 0.8f };  // Warna Biru
        sceneObjects.push_back(tiangKanan);
    }

    // Jika kamu punya file lain, misalnya "Atap.obj", tinggal tambahkan:
    /*
    GameObject3D atap;
    if (atap.loadOBJ("Atap.obj")) {
        atap.position = { 0.0f, 3.0f, 0.0f }; // Taruh di atas (Y = 3)
        atap.color = { 0.9f, 0.9f, 0.2f };
        sceneObjects.push_back(atap);
    }
    */

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);

    glutMainLoop();
    return 0;
}