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

	Copyright (c) 2026 danko1122q All rights reserved.
*/

/*
 * Baseline JPEG decoder (no external libraries).
 * Supports:
 *   - SOF0 (baseline DCT, YCbCr and grayscale)
 *   - DHT (Huffman tables, DC and AC)
 *   - DQT (quantisation tables, 8-bit precision)
 *   - SOS (scan header, interleaved and sequential)
 *   - DRI (restart intervals)
 *   - JFIF / EXIF APP markers (skipped)
 *
 * Not supported (returns error):
 *   - Progressive / hierarchical / arithmetic JPEG (SOF1-SOF3, SOF5-SOF15)
 *   - 12-bit quantisation tables
 */

#include "JPEG.h"
#include "Framebuffer.h"
#include "Color.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define JPEG_SOI  0xFFD8
#define JPEG_EOI  0xFFD9
#define JPEG_SOS  0xFFDA
#define JPEG_DQT  0xFFDB
#define JPEG_DHT  0xFFC4
#define JPEG_SOF0 0xFFC0
#define JPEG_DRI  0xFFDD
#define JPEG_COM  0xFFFE

// RST markers
#define JPEG_RST0 0xFFD0
#define JPEG_RST7 0xFFD7

// APP markers
#define JPEG_APP0 0xFFE0
#define JPEG_APP15 0xFFEF

#define MAX_COMPONENTS 3
#define MAX_QUANT_TABLES 4
#define MAX_HUFF_TABLES  4
#define BLOCK_SIZE 64

// ---------------------------------------------------------------------------
// IDCT lookup (AAN fixed-point scalars, pre-computed as double for clarity)
// ---------------------------------------------------------------------------

static const double IDCT_S[] = {
	1.0,
	1.387039845,
	1.306562965,
	1.175875602,
	1.0,
	0.785694958,
	0.541196100,
	0.275899379
};

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------

typedef struct {
	uint8_t  table[64];   // de-zigzagged, dequantised later
} QuantTable;

typedef struct {
	uint8_t  counts[16];  // number of codes of each length 1-16
	uint8_t  symbols[256];
	uint16_t codes[256];
	int      code_count;
} HuffTable;

typedef struct {
	uint8_t  id;
	uint8_t  h_samp;      // horizontal sampling factor
	uint8_t  v_samp;      // vertical sampling factor
	uint8_t  quant_id;    // quantisation table id
	uint8_t  dc_huff_id;
	uint8_t  ac_huff_id;

	int      dc_pred;     // DC predictor (reset at restart intervals)

	// Decoded 8x8 blocks for the current MCU row, stored as int16_t
	// Size: ceil(h_samp * v_samp * block_count_per_mcu) — allocated dynamically
	int16_t* blocks;      // raw DCT coefficients * quant
} Component;

typedef struct {
	FILE*         file;
	Gfx::Framebuffer* image;

	uint16_t      width;
	uint16_t      height;
	uint8_t       num_components;

	QuantTable    quant[MAX_QUANT_TABLES];
	bool          quant_valid[MAX_QUANT_TABLES];

	HuffTable     dc_huff[MAX_HUFF_TABLES];
	HuffTable     ac_huff[MAX_HUFF_TABLES];
	bool          dc_huff_valid[MAX_HUFF_TABLES];
	bool          ac_huff_valid[MAX_HUFF_TABLES];

	Component     comp[MAX_COMPONENTS];

	uint16_t      restart_interval; // 0 = no restart

	// Bit-buffer for entropy-coded data
	uint32_t      bit_buf;
	int           bit_count;
	bool          hit_eof;
} JPEG;

// ---------------------------------------------------------------------------
// Zigzag order (JPEG spec Table A.1)
// ---------------------------------------------------------------------------
static const uint8_t ZIGZAG[64] = {
	 0,  1,  8, 16,  9,  2,  3, 10,
	17, 24, 32, 25, 18, 11,  4,  5,
	12, 19, 26, 33, 40, 48, 41, 34,
	27, 20, 13,  6,  7, 14, 21, 28,
	35, 42, 49, 56, 57, 50, 43, 36,
	29, 22, 15, 23, 30, 37, 44, 51,
	58, 59, 52, 45, 38, 31, 39, 46,
	53, 60, 61, 54, 47, 55, 62, 63
};

// ---------------------------------------------------------------------------
// File I/O helpers
// ---------------------------------------------------------------------------

static uint8_t read_u8(JPEG* jpg) {
	int c = fgetc(jpg->file);
	if(c == EOF) jpg->hit_eof = true;
	return (uint8_t)c;
}

static uint16_t read_u16(JPEG* jpg) {
	uint16_t hi = read_u8(jpg);
	uint16_t lo = read_u8(jpg);
	return (hi << 8) | lo;
}

static void skip_bytes(JPEG* jpg, int n) {
	for(int i = 0; i < n; i++) read_u8(jpg);
}

// ---------------------------------------------------------------------------
// Huffman
// ---------------------------------------------------------------------------

static void build_huffman(HuffTable* ht) {
	// Assign canonical codes
	int idx = 0;
	uint16_t code = 0;
	ht->code_count = 0;
	for(int len = 1; len <= 16; len++) {
		for(int i = 0; i < ht->counts[len - 1]; i++) {
			ht->codes[idx] = code;
			idx++;
			code++;
			ht->code_count++;
		}
		code <<= 1;
	}
}

// Read one bit from the entropy-coded stream.
// JPEG byte-stuffing: 0xFF 0x00 => 0xFF data byte.
static int read_bit(JPEG* jpg) {
	if(jpg->bit_count == 0) {
		uint8_t byte = read_u8(jpg);
		if(byte == 0xFF) {
			uint8_t next = read_u8(jpg);
			if(next != 0x00) {
				// Marker encountered; treat as end of entropy data
				jpg->hit_eof = true;
				return 0;
			}
		}
		jpg->bit_buf   = byte;
		jpg->bit_count = 8;
	}
	jpg->bit_count--;
	return (jpg->bit_buf >> jpg->bit_count) & 1;
}

static int read_bits(JPEG* jpg, int n) {
	int val = 0;
	for(int i = 0; i < n; i++) {
		val = (val << 1) | read_bit(jpg);
	}
	return val;
}

static int decode_huffman(JPEG* jpg, HuffTable* ht) {
	int code = 0;
	int idx  = 0;
	for(int len = 1; len <= 16; len++) {
		code = (code << 1) | read_bit(jpg);
		for(int i = 0; i < ht->counts[len - 1]; i++) {
			if(ht->codes[idx] == (uint16_t)code)
				return ht->symbols[idx];
			idx++;
		}
	}
	return -1; // decode failure
}

// Decode a signed coefficient from `bits` raw bits (JPEG sign extension)
static int extend_val(int val, int bits) {
	if(bits == 0) return 0;
	if(val < (1 << (bits - 1)))
		val -= (1 << bits) - 1;
	return val;
}

// ---------------------------------------------------------------------------
// IDCT (2D AAN algorithm)
// ---------------------------------------------------------------------------

static void idct_block(int16_t* block, int16_t* out) {
	double tmp[64];

	// Row-pass
	for(int y = 0; y < 8; y++) {
		double* row = tmp + y * 8;
		double s0 = block[y * 8 + 0];
		double s1 = block[y * 8 + 1];
		double s2 = block[y * 8 + 2];
		double s3 = block[y * 8 + 3];
		double s4 = block[y * 8 + 4];
		double s5 = block[y * 8 + 5];
		double s6 = block[y * 8 + 6];
		double s7 = block[y * 8 + 7];

		// Even part
		double p1 = (s2 + s6) * 0.541196100;
		double t0 = s0 + s4;
		double t1 = s0 - s4;
		double t2 = p1 + s6 * (-1.847759065);
		double t3 = p1 + s2 *  0.765366865;

		double e0 = t0 + t3;
		double e1 = t1 + t2;
		double e2 = t1 - t2;
		double e3 = t0 - t3;

		// Odd part
		double o0 = s7 + s1;
		double o1 = s7 - s1;
		double o2 = s5 + s3;
		double o3 = s5 - s3;

		double p2 = (o0 + o2) * 0.707106781;
		double p3 = (o1 + o3) * 1.414213562;
		double p4 = o0 * (-0.707106781) + p2;
		double p5 = o2 * (-2.613125930) + p3 + p2 * (-1.414213562);
		double p6 = o1 * 1.530734082   + p3 + p2 * (-1.414213562);
		double p7 = o3 * 0.707106781   + p4 + p5;

		double odd0 = p7;
		double odd1 = p6 + p7;
		double odd2 = p5 + p6;
		double odd3 = p4;
		double odd4 = p7 + p5;
		double odd5 = p6;
		double odd6 = p5;
		double odd7 = p4 + p6;

		row[0] = (e0 + odd0) * IDCT_S[0];
		row[7] = (e0 - odd0) * IDCT_S[7];
		row[1] = (e1 + odd1) * IDCT_S[1];
		row[6] = (e1 - odd1) * IDCT_S[6];
		row[2] = (e2 + odd2) * IDCT_S[2];
		row[5] = (e2 - odd2) * IDCT_S[5];
		row[3] = (e3 + odd3) * IDCT_S[3];
		row[4] = (e3 - odd4) * IDCT_S[4];
		(void)odd5; (void)odd6; (void)odd7;
	}

	// Column-pass
	for(int x = 0; x < 8; x++) {
		double s0 = tmp[0 * 8 + x];
		double s1 = tmp[1 * 8 + x];
		double s2 = tmp[2 * 8 + x];
		double s3 = tmp[3 * 8 + x];
		double s4 = tmp[4 * 8 + x];
		double s5 = tmp[5 * 8 + x];
		double s6 = tmp[6 * 8 + x];
		double s7 = tmp[7 * 8 + x];

		double p1 = (s2 + s6) * 0.541196100;
		double t0 = s0 + s4;
		double t1 = s0 - s4;
		double t2 = p1 + s6 * (-1.847759065);
		double t3 = p1 + s2 *  0.765366865;

		double e0 = t0 + t3;
		double e1 = t1 + t2;
		double e2 = t1 - t2;
		double e3 = t0 - t3;

		double o0 = s7 + s1;
		double o1 = s7 - s1;
		double o2 = s5 + s3;
		double o3 = s5 - s3;

		double p2 = (o0 + o2) * 0.707106781;
		double p3 = (o1 + o3) * 1.414213562;
		double p4 = o0 * (-0.707106781) + p2;
		double p5 = o2 * (-2.613125930) + p3 + p2 * (-1.414213562);
		double p6 = o1 * 1.530734082   + p3 + p2 * (-1.414213562);
		double p7 = o3 * 0.707106781   + p4 + p5;

		// Scale factor 1/8 for 2D IDCT normalisation
		const double norm = 1.0 / 8.0;
		out[0 * 8 + x] = (int16_t)((e0 + p7) * IDCT_S[0] * norm + 128.5);
		out[7 * 8 + x] = (int16_t)((e0 - p7) * IDCT_S[7] * norm + 128.5);
		out[1 * 8 + x] = (int16_t)((e1 + (p6 + p7))    * IDCT_S[1] * norm + 128.5);
		out[6 * 8 + x] = (int16_t)((e1 - (p6 + p7))    * IDCT_S[6] * norm + 128.5);
		out[2 * 8 + x] = (int16_t)((e2 + (p5 + p6))    * IDCT_S[2] * norm + 128.5);
		out[5 * 8 + x] = (int16_t)((e2 - (p5 + p6))    * IDCT_S[5] * norm + 128.5);
		out[3 * 8 + x] = (int16_t)((e3 + p4)           * IDCT_S[3] * norm + 128.5);
		out[4 * 8 + x] = (int16_t)((e3 - (p7 + p5))    * IDCT_S[4] * norm + 128.5);
	}
}

static inline int clamp_u8(int v) {
	return v < 0 ? 0 : (v > 255 ? 255 : v);
}

// ---------------------------------------------------------------------------
// DCT block decode (Huffman + dequantisation)
// ---------------------------------------------------------------------------

static bool decode_block(JPEG* jpg, HuffTable* dc_ht, HuffTable* ac_ht,
                         uint8_t* quant_table, int* dc_pred, int16_t* out_block) {
	// --- DC coefficient ---
	int dc_sym = decode_huffman(jpg, dc_ht);
	if(dc_sym < 0) return false;

	int dc_val = 0;
	if(dc_sym > 0)
		dc_val = extend_val(read_bits(jpg, dc_sym), dc_sym);

	*dc_pred += dc_val;

	int16_t dct[64] = {0};
	dct[0] = (int16_t)(*dc_pred * quant_table[0]);

	// --- AC coefficients ---
	int k = 1;
	while(k < 64) {
		int ac_sym = decode_huffman(jpg, ac_ht);
		if(ac_sym < 0) return false;

		if(ac_sym == 0x00) break; // EOB

		int run  = (ac_sym >> 4) & 0xF;
		int size = ac_sym & 0xF;

		if(ac_sym == 0xF0) {
			// ZRL: 16 zeros
			k += 16;
			continue;
		}

		k += run;
		if(k >= 64) break;

		int ac_val = extend_val(read_bits(jpg, size), size);
		dct[ZIGZAG[k]] = (int16_t)(ac_val * quant_table[ZIGZAG[k]]);
		k++;
	}

	// IDCT
	idct_block(dct, out_block);
	return true;
}

// ---------------------------------------------------------------------------
// YCbCr -> RGB conversion (ITU-R BT.601)
// ---------------------------------------------------------------------------

static inline uint8_t ycbcr_r(int Y, int Cb, int Cr) {
	return (uint8_t)clamp_u8((int)(Y + 1.402 * (Cr - 128)));
}
static inline uint8_t ycbcr_g(int Y, int Cb, int Cr) {
	return (uint8_t)clamp_u8((int)(Y - 0.34414 * (Cb - 128) - 0.71414 * (Cr - 128)));
}
static inline uint8_t ycbcr_b(int Y, int Cb, int Cr) {
	return (uint8_t)clamp_u8((int)(Y + 1.772 * (Cb - 128)));
}

// ---------------------------------------------------------------------------
// Marker parsing
// ---------------------------------------------------------------------------

static bool parse_dqt(JPEG* jpg) {
	int length = (int)read_u16(jpg) - 2;
	while(length > 0) {
		uint8_t byte = read_u8(jpg);
		length--;
		int precision = (byte >> 4) & 0xF;
		int table_id  = byte & 0xF;

		if(precision != 0) {
			fprintf(stderr, "JPEG: 16-bit quantisation tables not supported\n");
			return false;
		}
		if(table_id >= MAX_QUANT_TABLES) {
			fprintf(stderr, "JPEG: Invalid quantisation table id %d\n", table_id);
			return false;
		}

		for(int i = 0; i < 64; i++) {
			jpg->quant[table_id].table[ZIGZAG[i]] = read_u8(jpg);
		}
		jpg->quant_valid[table_id] = true;
		length -= 64;
	}
	return true;
}

static bool parse_dht(JPEG* jpg) {
	int length = (int)read_u16(jpg) - 2;
	while(length > 0) {
		uint8_t byte    = read_u8(jpg);
		length--;
		int is_ac    = (byte >> 4) & 1;
		int table_id = byte & 0xF;

		if(table_id >= MAX_HUFF_TABLES) {
			fprintf(stderr, "JPEG: Invalid Huffman table id %d\n", table_id);
			return false;
		}

		HuffTable* ht = is_ac ? &jpg->ac_huff[table_id] : &jpg->dc_huff[table_id];

		int total = 0;
		for(int i = 0; i < 16; i++) {
			ht->counts[i] = read_u8(jpg);
			total += ht->counts[i];
			length--;
		}

		for(int i = 0; i < total; i++) {
			ht->symbols[i] = read_u8(jpg);
			length--;
		}

		build_huffman(ht);

		if(is_ac) jpg->ac_huff_valid[table_id] = true;
		else       jpg->dc_huff_valid[table_id] = true;
	}
	return true;
}

static bool parse_sof0(JPEG* jpg) {
	int length = (int)read_u16(jpg) - 2;
	(void)length;

	uint8_t precision = read_u8(jpg);
	if(precision != 8) {
		fprintf(stderr, "JPEG: Only 8-bit precision supported (got %d)\n", precision);
		return false;
	}

	jpg->height = read_u16(jpg);
	jpg->width  = read_u16(jpg);

	if(jpg->width == 0 || jpg->height == 0) {
		fprintf(stderr, "JPEG: Invalid dimensions %dx%d\n", jpg->width, jpg->height);
		return false;
	}

	jpg->num_components = read_u8(jpg);
	if(jpg->num_components != 1 && jpg->num_components != 3) {
		fprintf(stderr, "JPEG: Unsupported component count %d\n", jpg->num_components);
		return false;
	}

	for(int i = 0; i < jpg->num_components; i++) {
		jpg->comp[i].id       = read_u8(jpg);
		uint8_t samp          = read_u8(jpg);
		jpg->comp[i].h_samp   = (samp >> 4) & 0xF;
		jpg->comp[i].v_samp   = samp & 0xF;
		jpg->comp[i].quant_id = read_u8(jpg);
		jpg->comp[i].dc_pred  = 0;
		jpg->comp[i].blocks   = nullptr;
	}

	return true;
}

static bool parse_dri(JPEG* jpg) {
	read_u16(jpg); // length (always 4)
	jpg->restart_interval = read_u16(jpg);
	return true;
}

// ---------------------------------------------------------------------------
// Scan (SOS) decode
// ---------------------------------------------------------------------------

static bool decode_scan(JPEG* jpg) {
	// Read SOS header
	int sos_length = (int)read_u16(jpg) - 2;
	int num_scan_comp = read_u8(jpg);
	sos_length--;

	if(num_scan_comp != (int)jpg->num_components) {
		fprintf(stderr, "JPEG: Scan component count mismatch\n");
		return false;
	}

	for(int i = 0; i < num_scan_comp; i++) {
		uint8_t comp_id = read_u8(jpg);
		uint8_t tables  = read_u8(jpg);
		sos_length -= 2;

		// Find matching component
		Component* comp = nullptr;
		for(int j = 0; j < jpg->num_components; j++) {
			if(jpg->comp[j].id == comp_id) {
				comp = &jpg->comp[j];
				break;
			}
		}
		if(!comp) {
			fprintf(stderr, "JPEG: Unknown component id %d in scan\n", comp_id);
			return false;
		}

		comp->dc_huff_id = (tables >> 4) & 0xF;
		comp->ac_huff_id = tables & 0xF;
	}

	// Skip Ss, Se, Ah/Al (spectral selection — baseline always 0, 63, 0)
	skip_bytes(jpg, 3);

	// Reset bit buffer and DC predictors
	jpg->bit_buf   = 0;
	jpg->bit_count = 0;
	jpg->hit_eof   = false;

	for(int i = 0; i < jpg->num_components; i++)
		jpg->comp[i].dc_pred = 0;

	// Determine MCU dimensions
	// For simplicity, baseline interleaved: MCU is 8*h_samp x 8*v_samp per component
	int max_h = 1, max_v = 1;
	for(int i = 0; i < jpg->num_components; i++) {
		if(jpg->comp[i].h_samp > max_h) max_h = jpg->comp[i].h_samp;
		if(jpg->comp[i].v_samp > max_v) max_v = jpg->comp[i].v_samp;
	}

	int mcu_w = max_h * 8;
	int mcu_h = max_v * 8;

	int mcus_x = (jpg->width  + mcu_w - 1) / mcu_w;
	int mcus_y = (jpg->height + mcu_h - 1) / mcu_h;

	// Allocate per-component block buffers
	for(int i = 0; i < jpg->num_components; i++) {
		int blocks_per_mcu = jpg->comp[i].h_samp * jpg->comp[i].v_samp;
		free(jpg->comp[i].blocks);
		jpg->comp[i].blocks = (int16_t*)malloc(blocks_per_mcu * BLOCK_SIZE * sizeof(int16_t));
		if(!jpg->comp[i].blocks) {
			fprintf(stderr, "JPEG: Out of memory allocating component blocks\n");
			return false;
		}
	}

	int restart_counter = 0;

	// Decode MCUs
	for(int mcu_y = 0; mcu_y < mcus_y && !jpg->hit_eof; mcu_y++) {
		for(int mcu_x = 0; mcu_x < mcus_x && !jpg->hit_eof; mcu_x++) {

			// Handle restart intervals
			if(jpg->restart_interval > 0) {
				if(restart_counter == (int)jpg->restart_interval) {
					restart_counter = 0;
					// Align to next byte, read RST marker (2 bytes)
					jpg->bit_buf   = 0;
					jpg->bit_count = 0;
					uint8_t m1 = read_u8(jpg);
					uint8_t m2 = read_u8(jpg);
					(void)m1; (void)m2;
					// Reset DC predictors
					for(int i = 0; i < jpg->num_components; i++)
						jpg->comp[i].dc_pred = 0;
				}
				restart_counter++;
			}

			// Decode each component's blocks for this MCU
			for(int ci = 0; ci < jpg->num_components; ci++) {
				Component* comp = &jpg->comp[ci];
				HuffTable* dc_ht = &jpg->dc_huff[comp->dc_huff_id];
				HuffTable* ac_ht = &jpg->ac_huff[comp->ac_huff_id];
				uint8_t*   qt    = jpg->quant[comp->quant_id].table;

				int blocks_h = comp->h_samp;
				int blocks_v = comp->v_samp;

				for(int bv = 0; bv < blocks_v; bv++) {
					for(int bh = 0; bh < blocks_h; bh++) {
						int block_idx = bv * blocks_h + bh;
						int16_t* block = comp->blocks + block_idx * BLOCK_SIZE;

						if(!decode_block(jpg, dc_ht, ac_ht, qt, &comp->dc_pred, block))
							goto decode_error;
					}
				}
			}

			// Write pixels to framebuffer
			int pixel_base_x = mcu_x * mcu_w;
			int pixel_base_y = mcu_y * mcu_h;

			for(int py = 0; py < mcu_h; py++) {
				for(int px = 0; px < mcu_w; px++) {
					int img_x = pixel_base_x + px;
					int img_y = pixel_base_y + py;

					if(img_x >= jpg->width || img_y >= jpg->height)
						continue;

					if(jpg->num_components == 1) {
						// Grayscale
						Component* Y_comp = &jpg->comp[0];
						int bh = px / 8;
						int bv = py / 8;
						if(bh >= Y_comp->h_samp) bh = Y_comp->h_samp - 1;
						if(bv >= Y_comp->v_samp) bv = Y_comp->v_samp - 1;
						int16_t* block = Y_comp->blocks + (bv * Y_comp->h_samp + bh) * BLOCK_SIZE;
						int lx = px % 8;
						int ly = py % 8;
						int Yv = clamp_u8(block[ly * 8 + lx]);
						IMGPTRPIXEL(jpg->image, img_x, img_y) = RGB(Yv, Yv, Yv);
					} else {
						// YCbCr -> RGB
						// Each component may have different sampling factors.
						// Scale pixel coords into the component's block grid.
						int vals[3];
						for(int ci = 0; ci < 3; ci++) {
							Component* comp = &jpg->comp[ci];
							// Map pixel position to this component's sample position
							int sx = (px * comp->h_samp) / max_h;
							int sy = (py * comp->v_samp) / max_v;
							int bh = sx / 8;
							int bv = sy / 8;
							if(bh >= comp->h_samp) bh = comp->h_samp - 1;
							if(bv >= comp->v_samp) bv = comp->v_samp - 1;
							int16_t* block = comp->blocks + (bv * comp->h_samp + bh) * BLOCK_SIZE;
							int lx = sx % 8;
							int ly = sy % 8;
							vals[ci] = clamp_u8(block[ly * 8 + lx]);
						}
						int Y  = vals[0];
						int Cb = vals[1];
						int Cr = vals[2];
						IMGPTRPIXEL(jpg->image, img_x, img_y) =
							RGB(ycbcr_r(Y, Cb, Cr),
							    ycbcr_g(Y, Cb, Cr),
							    ycbcr_b(Y, Cb, Cr));
					}
				}
			}
		}
	}

	return true;

decode_error:
	fprintf(stderr, "JPEG: Error decoding Huffman block\n");
	return false;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

Gfx::Framebuffer* Gfx::load_jpeg_from_file(FILE* file) {
	fseek(file, 0, SEEK_SET);

	// Check SOI marker
	uint16_t soi = 0;
	{
		uint8_t b0 = (uint8_t)fgetc(file);
		uint8_t b1 = (uint8_t)fgetc(file);
		soi = (uint16_t)((b0 << 8) | b1);
	}
	if(soi != JPEG_SOI) {
		fprintf(stderr, "JPEG: Invalid SOI marker 0x%04X\n", soi);
		return nullptr;
	}

	JPEG jpg;
	memset(&jpg, 0, sizeof(jpg));
	jpg.file = file;

	bool sof_parsed  = false;
	bool scan_ok     = false;

	while(true) {
		if(feof(file)) break;

		// Read marker
		uint8_t ff = (uint8_t)fgetc(file);
		if(feof(file)) break;
		if(ff != 0xFF) {
			// Out of sync — scan for next 0xFF
			while(ff != 0xFF && !feof(file))
				ff = (uint8_t)fgetc(file);
			if(feof(file)) break;
		}

		// Skip extra 0xFF padding bytes
		uint8_t marker_lo;
		do {
			marker_lo = (uint8_t)fgetc(file);
		} while(marker_lo == 0xFF && !feof(file));

		uint16_t marker = (uint16_t)(0xFF00 | marker_lo);

		if(marker == JPEG_EOI) break;

		// SOF markers
		if(marker == JPEG_SOF0) {
			if(!parse_sof0(&jpg)) goto error;
			sof_parsed = true;

			// Allocate framebuffer
			jpg.image = new Gfx::Framebuffer(jpg.width, jpg.height);
			if(!jpg.image || !jpg.image->data) {
				fprintf(stderr, "JPEG: Failed to allocate framebuffer\n");
				goto error;
			}
			continue;
		}

		// Progressive / hierarchical / arithmetic — not supported
		if((marker >= 0xFFC1 && marker <= 0xFFC3) ||
		   (marker >= 0xFFC5 && marker <= 0xFFC7) ||
		   (marker >= 0xFFC9 && marker <= 0xFFCF)) {
			fprintf(stderr, "JPEG: Progressive / arithmetic JPEG not supported (marker 0x%04X)\n", marker);
			goto error;
		}

		if(marker == JPEG_DQT) {
			if(!parse_dqt(&jpg)) goto error;
			continue;
		}

		if(marker == JPEG_DHT) {
			if(!parse_dht(&jpg)) goto error;
			continue;
		}

		if(marker == JPEG_DRI) {
			if(!parse_dri(&jpg)) goto error;
			continue;
		}

		if(marker == JPEG_SOS) {
			if(!sof_parsed) {
				fprintf(stderr, "JPEG: SOS before SOF\n");
				goto error;
			}
			if(!decode_scan(&jpg)) goto error;
			scan_ok = true;
			// After scan, entropy data ends; next byte should be a marker.
			// Loop will re-sync on next iteration.
			continue;
		}

		// APP and COM markers — skip
		if((marker >= JPEG_APP0 && marker <= JPEG_APP15) || marker == JPEG_COM) {
			uint16_t len = 0;
			{
				uint8_t h = (uint8_t)fgetc(file);
				uint8_t l = (uint8_t)fgetc(file);
				len = (uint16_t)((h << 8) | l);
			}
			if(len < 2) { fprintf(stderr, "JPEG: Invalid APP/COM length\n"); goto error; }
			for(int i = 0; i < len - 2; i++) fgetc(file);
			continue;
		}

		// Unknown marker with length — skip
		if(marker != JPEG_SOI && marker != JPEG_EOI) {
			uint8_t h = (uint8_t)fgetc(file);
			uint8_t l = (uint8_t)fgetc(file);
			uint16_t len = (uint16_t)((h << 8) | l);
			if(len >= 2) {
				for(int i = 0; i < len - 2; i++) fgetc(file);
			}
		}
	}

	if(!scan_ok) {
		fprintf(stderr, "JPEG: No valid scan data found\n");
		goto error;
	}

	// Free component block buffers
	for(int i = 0; i < jpg.num_components; i++)
		free(jpg.comp[i].blocks);

	return jpg.image;

error:
	for(int i = 0; i < jpg.num_components; i++)
		free(jpg.comp[i].blocks);
	delete jpg.image;
	return nullptr;
}

Gfx::Framebuffer* Gfx::load_jpeg(const std::string& filename) {
	FILE* file = fopen(filename.c_str(), "rb");
	if(!file) {
		fprintf(stderr, "JPEG: Failed to open file: %s\n", filename.c_str());
		return nullptr;
	}
	auto* ret = load_jpeg_from_file(file);
	fclose(file);
	return ret;
}
