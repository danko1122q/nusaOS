/*
	This file is part of nusaOS.

	nusaOS is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	nusaOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with nusaOS.  If not, see <https://www.gnu.org/licenses/>.

	Copyright (c) Byteduck 2016-2021. All rights reserved.
*/

#include "PNG.h"
#include <memory.h>
#include "Deflate.h"
#include "Framebuffer.h"
#include <stdio.h>

#define abs(a) ((a) < 0 ? -(a) : (a))

// PNG menggunakan Big Endian (Network Byte Order)
uint32_t fget32(FILE* file) {
	uint32_t a = fgetc(file);  // byte pertama (most significant)
	uint32_t b = fgetc(file);
	uint32_t c = fgetc(file);
	uint32_t d = fgetc(file);  // byte terakhir (least significant)
	return (a << 24) | (b << 16) | (c << 8) | d;
}

const uint8_t PNG_HEADER[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

#define CHUNK_IHDR 0x49484452
#define CHUNK_IDAT 0x49444154
#define CHUNK_IEND 0x49454e44

#define STATE_SCANLINE_BEGIN 1
#define STATE_READPIXEL 2
#define STATE_DONE 3

#define PNG_FILTERTYPE_NONE 0
#define PNG_FILTERTYPE_SUB 1
#define PNG_FILTERTYPE_UP 2
#define PNG_FILTERTYPE_AVG 3
#define PNG_FILTERTYPE_PAETH 4

#define PNG_COLORTYPE_GRAYSCALE 0
#define PNG_COLORTYPE_TRUECOLOR 2
#define PNG_COLORTYPE_INDEXED 3
#define PNG_COLORTYPE_AGRAYSCALE 4
#define PNG_COLORTYPE_ATRUECOLOR 6

using namespace Gfx;

typedef struct PNG {
	struct {
		uint32_t width;
		uint32_t height;
		uint8_t	bit_depth;
		uint8_t color_type;
		uint8_t compression_method;
		uint8_t filter_method;
		uint8_t interlace_method;
	} ihdr;
	Gfx::Framebuffer* image;
	FILE* file;
	uint32_t chunk_size;
	uint32_t chunk_type;

	uint8_t state;
	uint8_t pixel_buffer[4];
	uint8_t pixel_buffer_pos;
	int pixel_x;
	int pixel_y;
	uint8_t scanline_filtertype;
} PNG;

// Paeth predictor function
int paeth(int a, int b, int c) {
	int p = (int) a + (int) b - (int) c;
	if (abs(p - a) <= abs(p - b) && abs(p - a) <= abs(p - c)) return a;
	else if (abs(p - b) <= abs(p - c)) return b;
	return c;
}

void put_pixel(PNG* png, uint32_t color) {
	IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y) = color;
	png->pixel_buffer_pos = 0;
	png->pixel_x++;
	if(png->pixel_x == (int)png->ihdr.width) {
		if(png->pixel_y == (int)png->ihdr.height - 1) {
			png->state = STATE_DONE;
		} else {
			png->state = STATE_SCANLINE_BEGIN;
			png->pixel_x = 0;
			png->pixel_y++;
		}
	}
}

// Filter PNG dengan uint8_t casting untuk modulo 256 otomatis
void grayscale_pixel(PNG* png) {
	uint8_t c = png->pixel_buffer[0];

	if(png->scanline_filtertype == PNG_FILTERTYPE_SUB && png->pixel_x > 0) {
		Color left_color = IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y);
		c = (uint8_t)(c + COLOR_R(left_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_UP && png->pixel_y > 0) {
		Color up_color = IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1);
		c = (uint8_t)(c + COLOR_R(up_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_AVG) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		c = (uint8_t)(c + ((COLOR_R(up_color) + COLOR_R(left_color)) / 2));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_PAETH) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		Color corner_color = (png->pixel_x > 0 && png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y - 1) : Color();
		c = (uint8_t)(c + paeth(COLOR_R(left_color), COLOR_R(up_color), COLOR_R(corner_color)));
	}

	put_pixel(png, RGB(c,c,c));
}

void truecolor_pixel(PNG* png) {
	uint8_t r = png->pixel_buffer[0];
	uint8_t g = png->pixel_buffer[1];
	uint8_t b = png->pixel_buffer[2];

	if(png->scanline_filtertype == PNG_FILTERTYPE_SUB && png->pixel_x > 0) {
		Color left_color = IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y);
		r = (uint8_t)(r + COLOR_R(left_color));
		g = (uint8_t)(g + COLOR_G(left_color));
		b = (uint8_t)(b + COLOR_B(left_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_UP && png->pixel_y > 0) {
		Color up_color = IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1);
		r = (uint8_t)(r + COLOR_R(up_color));
		g = (uint8_t)(g + COLOR_G(up_color));
		b = (uint8_t)(b + COLOR_B(up_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_AVG) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		r = (uint8_t)(r + ((COLOR_R(up_color) + COLOR_R(left_color)) / 2));
		g = (uint8_t)(g + ((COLOR_G(up_color) + COLOR_G(left_color)) / 2));
		b = (uint8_t)(b + ((COLOR_B(up_color) + COLOR_B(left_color)) / 2));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_PAETH) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		Color corner_color = (png->pixel_x > 0 && png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y - 1) : Color();
		r = (uint8_t)(r + paeth(COLOR_R(left_color), COLOR_R(up_color), COLOR_R(corner_color)));
		g = (uint8_t)(g + paeth(COLOR_G(left_color), COLOR_G(up_color), COLOR_G(corner_color)));
		b = (uint8_t)(b + paeth(COLOR_B(left_color), COLOR_B(up_color), COLOR_B(corner_color)));
	}

	put_pixel(png, RGB(r,g,b));
}

void alpha_truecolor_pixel(PNG* png) {
	uint8_t r = png->pixel_buffer[0];
	uint8_t g = png->pixel_buffer[1];
	uint8_t b = png->pixel_buffer[2];
	uint8_t a = png->pixel_buffer[3];

	if(png->scanline_filtertype == PNG_FILTERTYPE_SUB && png->pixel_x > 0) {
		Color left_color = IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y);
		r = (uint8_t)(r + COLOR_R(left_color));
		g = (uint8_t)(g + COLOR_G(left_color));
		b = (uint8_t)(b + COLOR_B(left_color));
		a = (uint8_t)(a + COLOR_A(left_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_UP && png->pixel_y > 0) {
		Color up_color = IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1);
		r = (uint8_t)(r + COLOR_R(up_color));
		g = (uint8_t)(g + COLOR_G(up_color));
		b = (uint8_t)(b + COLOR_B(up_color));
		a = (uint8_t)(a + COLOR_A(up_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_AVG) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		r = (uint8_t)(r + ((COLOR_R(up_color) + COLOR_R(left_color)) / 2));
		g = (uint8_t)(g + ((COLOR_G(up_color) + COLOR_G(left_color)) / 2));
		b = (uint8_t)(b + ((COLOR_B(up_color) + COLOR_B(left_color)) / 2));
		a = (uint8_t)(a + ((COLOR_A(up_color) + COLOR_A(left_color)) / 2));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_PAETH) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		Color corner_color = (png->pixel_x > 0 && png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y - 1) : Color();
		r = (uint8_t)(r + paeth(COLOR_R(left_color), COLOR_R(up_color), COLOR_R(corner_color)));
		g = (uint8_t)(g + paeth(COLOR_G(left_color), COLOR_G(up_color), COLOR_G(corner_color)));
		b = (uint8_t)(b + paeth(COLOR_B(left_color), COLOR_B(up_color), COLOR_B(corner_color)));
		a = (uint8_t)(a + paeth(COLOR_A(left_color), COLOR_A(up_color), COLOR_A(corner_color)));
	}

	put_pixel(png, RGBA(r,g,b,a));
}

void alpha_grayscale_pixel(PNG* png) {
	uint8_t c = png->pixel_buffer[0];
	uint8_t a = png->pixel_buffer[1];

	if(png->scanline_filtertype == PNG_FILTERTYPE_SUB && png->pixel_x > 0) {
		Color left_color = IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y);
		c = (uint8_t)(c + COLOR_R(left_color));
		a = (uint8_t)(a + COLOR_A(left_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_UP && png->pixel_y > 0) {
		Color up_color = IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1);
		c = (uint8_t)(c + COLOR_R(up_color));
		a = (uint8_t)(a + COLOR_A(up_color));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_AVG) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		c = (uint8_t)(c + ((COLOR_R(up_color) + COLOR_R(left_color)) / 2));
		a = (uint8_t)(a + ((COLOR_A(up_color) + COLOR_A(left_color)) / 2));
	} else if(png->scanline_filtertype == PNG_FILTERTYPE_PAETH) {
		Color left_color = (png->pixel_x > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y) : Color();
		Color up_color = (png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x, png->pixel_y - 1) : Color();
		Color corner_color = (png->pixel_x > 0 && png->pixel_y > 0) ? IMGPTRPIXEL(png->image, png->pixel_x - 1, png->pixel_y - 1) : Color();
		c = (uint8_t)(c + paeth(COLOR_R(left_color), COLOR_R(up_color), COLOR_R(corner_color)));
		a = (uint8_t)(a + paeth(COLOR_A(left_color), COLOR_A(up_color), COLOR_A(corner_color)));
	}

	put_pixel(png, RGBA(c,c,c,a));
}

void png_write(uint8_t byte, void* png_void) {
	PNG* png = (PNG*) png_void;

	if(png->state == STATE_SCANLINE_BEGIN) {
		png->scanline_filtertype = byte;
		png->state = STATE_READPIXEL;
	} else if(png->state == STATE_READPIXEL) {
		png->pixel_buffer[png->pixel_buffer_pos++] = byte;
		if(png->pixel_buffer_pos == 1 && png->ihdr.color_type == PNG_COLORTYPE_GRAYSCALE) {
			grayscale_pixel(png);
		} else if(png->pixel_buffer_pos == 3 && png->ihdr.color_type == PNG_COLORTYPE_TRUECOLOR) {
			truecolor_pixel(png);
		} else if(png->pixel_buffer_pos == 2 && png->ihdr.color_type == PNG_COLORTYPE_AGRAYSCALE) {
			alpha_grayscale_pixel(png);
		} else if(png->pixel_buffer_pos == 4 && png->ihdr.color_type == PNG_COLORTYPE_ATRUECOLOR) {
			alpha_truecolor_pixel(png);
		}
	}
	// STATE_DONE: abaikan sisa byte dari decompressor
}

// FIX UTAMA: png_read menangani multiple IDAT chunks dengan benar.
// Ketika chunk_size habis, ia skip CRC (4 byte) lalu baca header chunk berikutnya.
// Jika chunk berikutnya bukan IDAT, kembalikan 0 sebagai sinyal EOF ke decompressor.
// PENTING: Setelah decompress() selesai, file pointer ada di tengah IDAT terakhir
// (chunk_size berapa pun yang tersisa). Loop utama TIDAK boleh skip CRC lagi
// untuk chunk IDAT karena png_read sudah mengelola posisi file pointer sendiri.
uint8_t png_read(void* png_void) {
	PNG* png = (PNG*) png_void;
	if(png->chunk_size == 0) {
		// Habis baca chunk ini, skip CRC (4 byte)
		fget32(png->file);

		// Baca header chunk berikutnya
		png->chunk_size = fget32(png->file);
		png->chunk_type = fget32(png->file);

		if(feof(png->file) || png->chunk_type != CHUNK_IDAT) {
			// Bukan IDAT lagi, atau sudah EOF — sinyal selesai ke decompressor
			// Reset chunk_size ke 0 agar tidak ada yang dibaca lebih lanjut
			png->chunk_size = 0;
			return 0;
		}
	}
	png->chunk_size--;
	return fgetc(png->file);
}

Framebuffer* Gfx::load_png_from_file(FILE* file) {
	// Baca dan verifikasi PNG header
	fseek(file, 0, SEEK_SET);
	uint8_t header[8];
	fread(header, 8, 1, file);
	if(memcmp(header, PNG_HEADER, 8) != 0) {
		fprintf(stderr, "PNG: Invalid file header!\n");
		return NULL;
	}

	PNG png;
	png.file = file;
	png.pixel_buffer_pos = 0;
	png.pixel_x = 0;
	png.pixel_y = 0;
	png.state = STATE_SCANLINE_BEGIN;
	png.image = nullptr;

	size_t chunk = 0;
	bool idat_done = false; // sudah selesai proses semua IDAT?

	while(1) {
		// Jika IDAT sudah selesai diproses (oleh decompress + png_read),
		// file pointer sudah ada setelah chunk IDAT terakhir yang dikonsumsi png_read.
		// Kita perlu tahu di mana posisi file sekarang.
		// Setelah decompress(), png_read() sudah membaca melewati chunk IDAT
		// berikutnya yang bukan IDAT (atau EOF), sehingga chunk_type dan chunk_size
		// di struct PNG sudah berisi chunk non-IDAT berikutnya.
		// Kita bisa langsung pakai nilai itu daripada baca ulang dari file.
		if(idat_done) {
			// png_read() sudah memposisikan file pointer pada chunk setelah IDAT.
			// png.chunk_type dan png.chunk_size sudah berisi data chunk berikutnya.
			// Proses chunk tersebut tanpa baca ulang dari file.
			if(png.chunk_type == CHUNK_IEND || feof(file)) {
				break;
			}
			// Skip chunk tidak dikenal setelah IDAT
			for(uint32_t i = 0; i < png.chunk_size; i++)
				fgetc(file);
			fget32(file); // Skip CRC
			// Baca chunk berikutnya
			png.chunk_size = fget32(file);
			png.chunk_type = fget32(file);
			continue;
		}

		png.chunk_size = fget32(file);
		png.chunk_type = fget32(file);

		if(feof(file)) {
			break;
		}

		if(chunk == 0 && png.chunk_type != CHUNK_IHDR) {
			fprintf(stderr, "PNG: No IHDR chunk 0x%08x\n", png.chunk_type);
			return NULL;
		} else if(chunk == 0) {
			// Baca IHDR
			png.ihdr.width = fget32(file);
			png.ihdr.height = fget32(file);
			png.ihdr.bit_depth = fgetc(file);
			png.ihdr.color_type = fgetc(file);
			png.ihdr.compression_method = fgetc(file);
			png.ihdr.filter_method = fgetc(file);
			png.ihdr.interlace_method = fgetc(file);

			if(png.ihdr.bit_depth != 1 && png.ihdr.bit_depth != 2 &&
			   png.ihdr.bit_depth != 4 && png.ihdr.bit_depth != 8 &&
			   png.ihdr.bit_depth != 16) {
				fprintf(stderr, "PNG: Invalid bit depth %d\n", png.ihdr.bit_depth);
				return NULL;
			}
			if(png.ihdr.color_type != 0 && png.ihdr.color_type != 2 &&
			   png.ihdr.color_type != 4 && png.ihdr.color_type != 6) {
				if(png.ihdr.color_type == 3)
					fprintf(stderr, "PNG: Indexed color is not supported!\n");
				else
					fprintf(stderr, "PNG: Invalid color type %d\n", png.ihdr.color_type);
				return NULL;
			}
			if(png.ihdr.compression_method != 0) {
				fprintf(stderr, "PNG: Invalid compression method %d\n", png.ihdr.compression_method);
				return NULL;
			}
			if(png.ihdr.filter_method != 0) {
				fprintf(stderr, "PNG: Invalid filter method %d\n", png.ihdr.filter_method);
				return NULL;
			}
			if(png.ihdr.interlace_method != 0 && png.ihdr.interlace_method != 1) {
				fprintf(stderr, "PNG: Invalid interlace method %d\n", png.ihdr.interlace_method);
				return NULL;
			}
			if(png.ihdr.interlace_method == 1) {
				fprintf(stderr, "PNG: Adam7 interlacing not supported yet!\n");
				return NULL;
			}

			// Alokasi framebuffer
			png.image = new Framebuffer(png.ihdr.width, png.ihdr.height);
			if (!png.image || !png.image->data) {
				fprintf(stderr, "PNG: Failed to allocate framebuffer memory\n");
				return NULL;
			}

			// Skip sisa byte IHDR yang tidak digunakan
			for(size_t i = 13; i < png.chunk_size; i++)
				fgetc(file);

		} else if(png.chunk_type == CHUNK_IDAT) {
			// FIX UTAMA: Proses SEMUA IDAT chunks dalam satu panggilan decompress().
			// png_read() akan otomatis melanjutkan ke chunk IDAT berikutnya saat chunk_size habis.
			// Loop utama TIDAK perlu (dan TIDAK boleh) tahu berapa banyak IDAT chunk ada.

			// Baca zlib header (hanya 2 byte pertama dari IDAT pertama)
			uint8_t zlib_method = fgetc(file);
			if((zlib_method & 0xF) != 0x8) {
				fprintf(stderr, "PNG: Unsupported zlib compression type 0x%x\n", zlib_method & 0xF);
				delete png.image;
				return NULL;
			}
			uint8_t zlib_flags = fgetc(file);
			if(zlib_flags & 0x20) {
				fprintf(stderr, "PNG: zlib presets are not supported\n");
				delete png.image;
				return NULL;
			}
			png.chunk_size -= 2; // sudah baca 2 byte zlib header

			// Inisialisasi dan jalankan decompressor.
			// decompress() akan terus memanggil png_read() sampai deflate stream selesai.
			// png_read() akan otomatis lompat ke chunk IDAT berikutnya jika chunk_size habis.
			// Setelah decompress() selesai, png_read() mungkin sudah membaca header
			// chunk non-IDAT berikutnya (IEND atau lainnya) ke dalam png.chunk_type.
			DEFLATE def;
			def.arg = &png;
			def.write = png_write;
			def.read = png_read;

			if(decompress(&def) < 0) {
				fprintf(stderr, "PNG: Decompression failed\n");
				delete png.image;
				return NULL;
			}

			// FIX: Setelah decompress selesai, png_read() mungkin sudah:
			// 1. Membaca sampai chunk_size == 0 pada chunk IDAT terakhir, lalu
			// 2. Mencoba baca chunk berikutnya dan menemukan bukan IDAT.
			// Dalam kasus itu, png.chunk_type sudah berisi chunk setelah IDAT.
			// JANGAN skip CRC lagi untuk chunk ini — atur flag agar loop tahu.
			idat_done = true;

			// Jika png_read berhenti di tengah IDAT (chunk_size > 0, misalnya karena
			// STATE_DONE), kita perlu skip sisa byte IDAT dan CRC-nya sendiri.
			// Kemudian baca chunk berikutnya untuk diserahkan ke logika idat_done di atas.
			if(png.chunk_size > 0) {
				// Masih ada sisa byte di IDAT terakhir yang belum dibaca
				for(uint32_t i = 0; i < png.chunk_size; i++)
					fgetc(file);
				fget32(file); // Skip CRC chunk IDAT terakhir ini
				// Baca header chunk berikutnya untuk idat_done handler
				png.chunk_size = fget32(file);
				png.chunk_type = fget32(file);
			}
			// Jika chunk_size == 0, png_read sudah mengkonsumsi CRC dan baca
			// header chunk berikutnya ke png.chunk_type — langsung lanjut ke idat_done.

			continue; // Lanjut ke iterasi berikutnya (idat_done == true)

		} else if(png.chunk_type == CHUNK_IEND) {
			break;
		} else {
			// Skip chunk tidak dikenal
			for(uint32_t i = 0; i < png.chunk_size; i++)
				fgetc(file);
		}

		fget32(file); // Skip CRC (untuk IHDR dan chunk non-IDAT lainnya)
		chunk++;
	}

	return png.image;
}

Framebuffer* Gfx::load_png(const std::string& filename) {
	auto* file = fopen(filename.c_str(), "r");
	if(!file) {
		fprintf(stderr, "PNG: Failed to open file: %s\n", filename.c_str());
		return nullptr;
	}
	auto* ret = load_png_from_file(file);
	fclose(file);
	return ret;
}