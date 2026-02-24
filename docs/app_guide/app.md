# Panduan Pembuatan Aplikasi GUI nusaOS

Dokumentasi ini menjelaskan cara membuat aplikasi berbasis GUI menggunakan pustaka `libui`.

## 1. Header Utama yang Wajib Di-include
Untuk membangun aplikasi GUI standar, Anda memerlukan:
- `#include <libui/Application.h>`: Untuk mengatur siklus hidup aplikasi.
- `#include <libui/Window.h>`: Untuk membuat jendela utama.
- `#include <libui/widget/Widget.h>`: Kelas dasar untuk semua elemen UI.

## 2. Widget yang Tersedia
Sertakan header sesuai kebutuhan elemen yang ingin ditampilkan:
- `#include <libui/widget/Label.h>` (Teks statis)
- `#include <libui/widget/Button.h>` (Tombol interaktif)
- `#include <libui/widget/BoxLayout.h>` (Untuk mengatur tata letak/layout)

## 3. Contoh Template Aplikasi
Berikut adalah struktur dasar aplikasi GUI:

```cpp
#include <libui/Application.h>
#include <libui/Window.h>
#include <libui/widget/Label.h>
#include <libui/widget/layout/BoxLayout.h>

int main(int argc, char** argv) {
    // 1. Inisialisasi Aplikasi
    auto app = UI::Application::construct(argc, argv);
    
    // 2. Buat Jendela Utama
    auto window = UI::Window::construct();
    window->set_title("Nama Aplikasi");
    window->resize({300, 200});
    
    // 3. Atur Layout dan Isi
    auto main_layout = UI::BoxLayout::construct(UI::Orientation::Vertical);
    auto label = UI::Label::construct("Halo Dunia!");
    main_layout->add_widget(label);
    
    window->set_contents(main_layout);
    
    // 4. Tampilkan dan Jalankan
    window->show();
    return app->run();
}
```

## 4. Konfigurasi Build (CMake)
Jangan lupa menambahkan dependensi `libui` di `CMakeLists.txt` aplikasi Anda:
```cmake
TARGET_LINK_LIBRARIES(nama_app libui libgraphics libpond)
```
