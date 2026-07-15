# TR Grafika Komputer

Proyek ini adalah aplikasi rendering 3D sederhana menggunakan OpenGL dan FreeGLUT. Aplikasi menampilkan objek 3D dari file .obj, menerapkan pencahayaan, transformasi model, serta kontrol kamera dengan mouse.

## Fitur
- Memuat model 3D dari file .obj
- Menampilkan beberapa objek dalam satu scene
- Penerapan pencahayaan dasar (light source)
- Transformasi objek: translasi, rotasi, dan skala
- Kontrol kamera menggunakan drag mouse

## Teknologi yang Digunakan
- C++
- OpenGL
- FreeGLUT
- MinGW / g++

## Struktur Folder
- main.cpp: source utama program
- main2.cpp: file pendukung / eksperimen
- object/: folder berisi file model .obj dan .mtl
- Texture/: folder berisi tekstur jika digunakan
- GL/: header FreeGLUT/OpenGL
- bin/ dan obj/: hasil build

## Persyaratan
Pastikan perangkat Anda telah menyiapkan:
- MinGW dengan compiler g++
- FreeGLUT untuk Windows
- Library OpenGL yang terhubung dengan benar

## Cara Menjalankan
1. Buka folder proyek di VS Code.
2. Pastikan path FreeGLUT dan MinGW sudah sesuai di file tugas build.
3. Jalankan build melalui task yang tersedia atau compile manual dengan g++.
4. Jalankan file .exe yang dihasilkan.

## Catatan
Jika Anda ingin mengubah lokasi model .obj, pastikan path file benar pada source program.

## Referensi
Proyek ini dibuat sebagai latihan grafika komputer untuk menampilkan objek 3D berbasis model eksternal.
