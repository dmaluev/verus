#include <cstdlib>
#include "..\zlib-1.2.12\zlib.h"

static unsigned char* my_compress(unsigned char* data, int data_len, int* out_len, int quality)
{
	uLong size = compressBound(data_len);
	unsigned char* pBuffer = static_cast<unsigned char*>(malloc(size));
	if (!pBuffer)
		return nullptr;
	if (compress2(pBuffer, &size, data, data_len, quality) != Z_OK)
	{
		free(pBuffer);
		return nullptr;
	}
	*out_len = size;
	return pBuffer;
}

#define STBIW_ZLIB_COMPRESS my_compress
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NO_INCLUDE_JSON
#include "..\json.hpp"
#include "tiny_gltf.h"
