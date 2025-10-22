// SPDX-License-Identifier: Unlicense

#include "ImageLoader.h"
#include "Globals.h"
#include <pd_api.h>
#include <assert.h>

typedef struct TgaHeader
{
	uint8_t id_size;
	uint8_t color_map_type;
	uint8_t image_type;
	uint8_t color_map_desc[5];
	uint16_t x_origin;
	uint16_t y_origin;
	uint16_t width;
	uint16_t height;
	uint8_t bits_per_pixel;
	uint8_t image_desc;
} TgaHeader;

static_assert(sizeof(TgaHeader) == 18, "TgaHeader should be 18 bytes");


uint8_t* read_tga_file_grayscale(const char* path, int* out_w, int* out_h)
{
    PlaydateAPI* pd = _G.pd;

	*out_w = *out_h = 0;
	SDFile* file = pd->file->open(path, kFileRead);
	if (!file)
		return NULL;

	uint8_t* res = NULL;
	TgaHeader header;
	int bytes = pd->file->read(file, &header, sizeof(header));
	if (bytes != sizeof(header)
		|| (header.image_type != 3) // only support grayscale images
		|| (header.bits_per_pixel != 8) // only support 8bpp
		|| (header.width == 0 || header.width > 2048 || header.height == 0 || header.height > 2048) // out of bounds sizes
		)
	{
		pd->file->close(file);
		return res;
	}
		

	*out_w = header.width;
	*out_h = header.height;

	int image_size = header.width * header.height;
	res = (uint8_t*)malloc(image_size);

	pd->file->seek(file, header.id_size, SEEK_CUR);
	bytes = pd->file->read(file, res, image_size);
	if (bytes != image_size) {
		free(res);
		res = nullptr;
	}
	pd->file->close(file);
	return res;
}
