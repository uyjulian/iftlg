/////////////////////////////////////////////
//                                         //
//    Copyright (C) 2019-2019 Julian Uy    //
//  https://sites.google.com/site/awertyb  //
//                                         //
//   See details of license at "LICENSE"   //
//                                         //
/////////////////////////////////////////////

#include "extractor.h"
#include <stdio.h>
#include <string.h>
#include <tlg.h>

const char *plugin_info[4] = {
    "00IN",
    "TLG Plugin for Susie Image Viewer",
    "*.tlg",
    "TLG file (*.tlg)",
};

const int header_size = 64;

class tMyStream : public tTJSBinaryStream
{
public:
	tMyStream(uint8_t *buf, size_t size) : buf(buf), size(size), cur(0) {
	}

	~tMyStream() {
		
	}

	virtual tjs_uint64 Seek(tjs_int64 offset, tjs_int whence) {
		if (buf) {
			size_t newpos;
			switch(whence) {
			case TJS_BS_SEEK_SET:			newpos = whence;		break;
			case TJS_BS_SEEK_CUR:			newpos = cur + whence;		break;
			case TJS_BS_SEEK_END:			newpos = size + whence;		break;
			default:						newpos = whence;		break;
			}
			if (newpos > size)
				newpos = size;
			if (newpos < 0)
				newpos = 0;
			return newpos;
		}
		return 0;
	}

	virtual tjs_uint Read(void *buffer, tjs_uint length) {
		if (buf) {
			if (cur + length >size)
				length = size - cur;

			memcpy(buffer, buf + cur, length);
			cur += length;
			return length;
		}
		return 0;
	}

	virtual tjs_uint Write(const void *buffer, tjs_uint write_size) {
		return 0;
	}

private:
	uint8_t *buf;
	size_t size;
	size_t cur;
};

typedef struct {
	uint8_t *bitmap_data;
	int width, height;
} bitmap_all_info;

static bool sizeCallback(void *callbackdata, unsigned int width, unsigned int height) {
	bitmap_all_info *bmp_allinfo = (bitmap_all_info*)callbackdata;
	bmp_allinfo->width = width;
	bmp_allinfo->height = height;
	bmp_allinfo->bitmap_data = (uint8_t *)malloc(width * height * 4);
	return true;
}

static void *scanLineCallback(void *callbackdata, int y) {
	bitmap_all_info *bmp_allinfo = (bitmap_all_info*)callbackdata;
	return bmp_allinfo->bitmap_data + (bmp_allinfo->width * y * 4);
}


int getBMPFromTLG(uint8_t *input_data, long file_size,
                   BITMAPFILEHEADER *bitmap_file_header,
                   BITMAPINFOHEADER *bitmap_info_header, uint8_t **data) {
	tMyStream stream(input_data, file_size);
	bitmap_all_info *bmp_allinfo = (bitmap_all_info *)malloc(sizeof(bmp_allinfo));
	int ret = TVPLoadTLG((void *)bmp_allinfo, sizeCallback, scanLineCallback, NULL, &stream);
	int width, height;
	width = bmp_allinfo->width;
	height = bmp_allinfo->height;
	*data = bmp_allinfo->bitmap_data;
	free(bmp_allinfo);

	if (ret)
		return 1;

	memset(bitmap_file_header, 0, sizeof(BITMAPFILEHEADER));
	memset(bitmap_info_header, 0, sizeof(BITMAPINFOHEADER));

	bitmap_file_header->bfType = 'M' * 256 + 'B';
	bitmap_file_header->bfSize = sizeof(BITMAPFILEHEADER) +
	                             sizeof(BITMAPINFOHEADER) +
	                             sizeof(uint8_t) * width * 4 * height;
	bitmap_file_header->bfOffBits =
	    sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bitmap_file_header->bfReserved1 = 0;
	bitmap_file_header->bfReserved2 = 0;

	bitmap_info_header->biSize = 40;
	bitmap_info_header->biWidth = width;
	bitmap_info_header->biHeight = height;
	bitmap_info_header->biPlanes = 1;
	bitmap_info_header->biBitCount = 32;
	bitmap_info_header->biCompression = 0;
	bitmap_info_header->biSizeImage = bitmap_file_header->bfSize;
	bitmap_info_header->biXPelsPerMeter = bitmap_info_header->biYPelsPerMeter =
	    0;
	bitmap_info_header->biClrUsed = 0;
	bitmap_info_header->biClrImportant = 0;
	return 0;
}

BOOL IsSupportedEx(char *filename, char *data) {
	const char header[] = {'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00,
	                 'W', 'E', 'B', 'P', 'V',  'P',  '8'};
	for (unsigned int i = 0; i < sizeof(header); i++) {
		if (header[i] == 0x00)
			continue;
		if (data[i] != header[i])
			return FALSE;
	}
	return TRUE;
}

int GetPictureInfoEx(long data_size, char *data,
                     struct PictureInfo *picture_info) {
	tMyStream stream((uint8_t *)data, data_size);
	int width, height;
	TVPGetInfoTLG(&stream, &width, &height);

	picture_info->left = 0;
	picture_info->top = 0;
	picture_info->width = width;
	picture_info->height = height;
	picture_info->x_density = 0;
	picture_info->y_density = 0;
	picture_info->colorDepth = 32;
	picture_info->hInfo = NULL;

	return SPI_ALL_RIGHT;
}

int GetPictureEx(long data_size, HANDLE *bitmap_info, HANDLE *bitmap_data,
                 SPI_PROGRESS progress_callback, long user_data, char *data) {
	uint8_t *data_u8;
	BITMAPINFOHEADER bitmap_info_header;
	BITMAPFILEHEADER bitmap_file_header;
	BITMAPINFO *bitmap_info_locked;
	unsigned char *bitmap_data_locked;

	if (progress_callback != NULL)
		if (progress_callback(1, 1, user_data))
			return SPI_ABORT;

	getBMPFromTLG((uint8_t *)data, data_size, &bitmap_file_header,
	               &bitmap_info_header, &data_u8);
	*bitmap_info = LocalAlloc(LMEM_MOVEABLE, sizeof(BITMAPINFOHEADER));
	*bitmap_data = LocalAlloc(LMEM_MOVEABLE, bitmap_file_header.bfSize -
	                                             bitmap_file_header.bfOffBits);
	if (*bitmap_info == NULL || *bitmap_data == NULL) {
		if (*bitmap_info != NULL)
			LocalFree(*bitmap_info);
		if (*bitmap_data != NULL)
			LocalFree(*bitmap_data);
		return SPI_NO_MEMORY;
	}
	bitmap_info_locked = (BITMAPINFO *)LocalLock(*bitmap_info);
	bitmap_data_locked = (unsigned char *)LocalLock(*bitmap_data);
	if (bitmap_info_locked == NULL || bitmap_data_locked == NULL) {
		LocalFree(*bitmap_info);
		LocalFree(*bitmap_data);
		return SPI_MEMORY_ERROR;
	}
	bitmap_info_locked->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmap_info_locked->bmiHeader.biWidth = bitmap_info_header.biWidth;
	bitmap_info_locked->bmiHeader.biHeight = bitmap_info_header.biHeight;
	bitmap_info_locked->bmiHeader.biPlanes = 1;
	bitmap_info_locked->bmiHeader.biBitCount = 32;
	bitmap_info_locked->bmiHeader.biCompression = BI_RGB;
	bitmap_info_locked->bmiHeader.biSizeImage = 0;
	bitmap_info_locked->bmiHeader.biXPelsPerMeter = 0;
	bitmap_info_locked->bmiHeader.biYPelsPerMeter = 0;
	bitmap_info_locked->bmiHeader.biClrUsed = 0;
	bitmap_info_locked->bmiHeader.biClrImportant = 0;
	memcpy(bitmap_data_locked, data_u8,
	       bitmap_file_header.bfSize - bitmap_file_header.bfOffBits);

	LocalUnlock(*bitmap_info);
	LocalUnlock(*bitmap_data);
	free(data_u8);

	if (progress_callback != NULL)
		if (progress_callback(1, 1, user_data))
			return SPI_ABORT;

	return SPI_ALL_RIGHT;
}
