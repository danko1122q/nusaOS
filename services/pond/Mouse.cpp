/*
   This file is part of nusaOS.

   Copyright (c) Byteduck 2016-2021. All rights reserved.
*/

#include <unistd.h>
#include "Mouse.h"
#include "Display.h"
#include <libgraphics/Image.h>
#include <libgraphics/PNG.h>

using namespace Gfx;

Mouse::Mouse(Window* parent): Window(parent, {0, 0, 1, 1}, false) {
   display()->set_mouse_window(this);

   mouse_fd = open("/dev/input/mouse", O_RDONLY | O_CLOEXEC);
   if(mouse_fd < 0) {
      perror("Failed to open mouse");
      return;
   }

   load_cursor(cursor_normal, "cursor.png");
   load_cursor(cursor_resize_v, "resize_v.png");
   load_cursor(cursor_resize_h, "resize_h.png");
   load_cursor(cursor_resize_dr, "resize_dr.png");
   load_cursor(cursor_resize_dl, "resize_dl.png");
   set_cursor(Pond::NORMAL);
}

int Mouse::fd() {
   return mouse_fd;
}

bool Mouse::update() {
   if(mouse_fd < 0)
      return false;

   // FIX 1: Naikkan buffer dari 32 ke 64 event agar tidak ada yang terbuang
   // saat mouse bergerak cepat
   MouseEvent events[64];
   ssize_t nread = read(mouse_fd, &events, sizeof(MouseEvent) * 64);
   if(nread <= 0) return false;
   if((size_t)nread < sizeof(MouseEvent)) return false;
   int num_events = (int) nread / sizeof(MouseEvent);

   Point total_delta {0, 0};
   int total_z = 0;
   uint8_t last_buttons = _mouse_buttons;

   for(int i = 0; i < num_events; i++) {
      Gfx::Point new_pos = rect().position();
      if(events[i].absolute) {
         auto disp_dimensions = Display::inst().dimensions();
         FloatPoint float_pos = {events[i].x / (float) 0xFFFF, events[i].y / (float) 0xFFFF};
         new_pos.x = float_pos.x * disp_dimensions.width;
         new_pos.y = float_pos.y * disp_dimensions.height;
         // FIX 2: Untuk absolute mouse, delta harus dihitung dari posisi sebelum set_position
         Gfx::Point old_pos = rect().position();
         if(parent())
            new_pos = new_pos.constrain(parent()->rect());
         Gfx::Point delta_pos = new_pos - old_pos;
         set_position(new_pos);
         total_delta += delta_pos;
      } else {
         new_pos.x += events[i].x;
         new_pos.y -= events[i].y;
         if(parent())
            new_pos = new_pos.constrain(parent()->rect());
         // FIX 3: Hitung delta dari posisi aktual setelah constrain,
         // bukan dari events[i].x/y mentah — agar delta akurat di tepi layar
         Gfx::Point delta_pos = new_pos - rect().position();
         set_position(new_pos);
         total_delta += delta_pos;
      }
      total_z += events[i].z;

      // FIX 4: Hanya flush event saat button state berubah, bukan setiap event.
      // Sebelumnya flush terjadi di setiap button change dan mereset total_delta,
      // memotong akumulasi gerakan dan menyebabkan gerakan terputus-putus.
      if (events[i].buttons != last_buttons) {
         Display::inst().create_mouse_events(total_delta.x, total_delta.y, total_z, events[i].buttons);
         _mouse_buttons = events[i].buttons;
         last_buttons = events[i].buttons;
         total_delta = {0, 0};
         total_z = 0;
      }
   }

   // Flush sisa movement setelah semua event diproses
   if (total_delta.x != 0 || total_delta.y != 0 || total_z != 0)
      Display::inst().create_mouse_events(total_delta.x, total_delta.y, total_z, _mouse_buttons);

   return true;
}

void Mouse::set_cursor(Pond::CursorType cursor) {
   current_type = cursor;
   Duck::Ptr<Gfx::Image> cursor_image;
   switch(cursor) {
      case Pond::NORMAL:
         cursor_image = cursor_normal;
         break;
      case Pond::RESIZE_H:
         cursor_image = cursor_resize_h;
         break;
      case Pond::RESIZE_V:
         cursor_image = cursor_resize_v;
         break;
      case Pond::RESIZE_DR:
         cursor_image = cursor_resize_dr;
         break;
      case Pond::RESIZE_DL:
         cursor_image = cursor_resize_dl;
         break;
      default:
         cursor_image = cursor_normal;
   }
   if(!cursor_image)
      return;

   set_dimensions(cursor_image->size());
   cursor_image->draw(_framebuffer, {0, 0});
}

Duck::Result Mouse::load_cursor(Duck::Ptr<Gfx::Image>& storage, const std::string& filename) {
   auto cursor_res = Image::load("/usr/share/cursors/" + filename);
   if(cursor_res.is_error()) {
      Duck::Log::errf("Couldn't load cursor {}: {}", filename, cursor_res.result());
      return cursor_res.result();
   }
   storage = cursor_res.value();
   return Duck::Result::SUCCESS;
}