/*
 * Copyright (C) 2018 Ivan Schréter (schreter@gmx.net)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This copyright notice MUST APPEAR in all copies of the software!
 */

#include "ScreenshotService.h"

#include <MCUFRIEND_kbv.h>
#include <avr/wdt.h>

/// How many bytes to transfer in one stride.
static constexpr int16_t STRIDE_SIZE = 160;

struct bmp_header
{
  uint16_t bfType = 0x4D42; // BM
  uint32_t bfSize;
  uint32_t bfReserved	= 0;
  uint32_t bfOffBits;

  uint32_t biSize = 52;
  int32_t	biWidth;
  int32_t	biHeight;
  uint16_t biPlanes = 1;
  uint16_t biBitCount = 16;
  uint32_t biCompression = 3; // bitfields
  uint32_t biSizeImage;
  int32_t	biXPelsPerMeter	= 0;
  int32_t	biYPelsPerMeter	= 0;
  uint32_t biClrUsed= 0;
  uint32_t biClrImportant = 0;
  uint32_t red_mask = 0xf800;
  uint32_t green_mask = 0x07e0;
  uint32_t blue_mask = 0x001f;
};

static_assert(sizeof(bmp_header) < STRIDE_SIZE, "BMP header doesn't fit into the buffer");
static_assert(sizeof(bmp_header) == 66, "BMP header size mismatch");

// in-place new operator
inline void* operator new(unsigned /*size*/, void* ptr) { return ptr; }

void ScreenshotService::make(MCUFRIEND_kbv& tft, Print& client) noexcept
{
  auto w = tft.width();
  auto h = tft.height();
  uint16_t buffer[STRIDE_SIZE / 2];
  bmp_header* hdr = new(&buffer) bmp_header;
  hdr->bfSize = uint32_t(w * h * 2) + sizeof(bmp_header);
  hdr->bfOffBits = sizeof(bmp_header);
  hdr->biWidth = w;
  hdr->biHeight = -h;
  hdr->biSizeImage = uint32_t(w * h * 2);
  client.write(reinterpret_cast<const uint8_t*>(hdr), sizeof(bmp_header));
  for (int16_t i = 0; i < h; ++i) {
    for (int16_t j = 0; j < w / (STRIDE_SIZE / 2); ++j) {
      tft.readGRAM(j * (STRIDE_SIZE / 2), i, buffer, STRIDE_SIZE / 2, 1);
      client.write(reinterpret_cast<const uint8_t*>(&buffer), STRIDE_SIZE);
    }
    wdt_reset();  // it takes long to write, make sure watchdog doesn't kill us
  }
}
