#include <stdlib.h>
#include <string.h>
#include <algorithm>

#ifdef USE_WASM_SIMD
#include <emmintrin.h>
#include <xmmintrin.h>
#include <smmintrin.h>
#include <wasm_simd128.h>
#endif

#include <chrono>

#include <emscripten/emscripten.h>

int main()
{
	return 0;
}

#define ALIGNMENT 16

#ifdef __cplusplus
extern "C"
{
#endif
	void *_me_aligned_malloc(const uint32_t acquired)
	{
		const uint32_t size = acquired + ALIGNMENT;
		uint8_t *head = (uint8_t *)malloc(size);
		auto ret = (uint8_t *)(((uint64_t)head + ALIGNMENT) & (~(uint64_t)(ALIGNMENT - 1)));
		*((uint32_t *)ret - 1) = (ret - head);
		return ret;
	}

	EMSCRIPTEN_KEEPALIVE void *createNV12Buffer(uint32_t width, uint32_t height)
	{
		return _me_aligned_malloc((width * height * 3) >> 1);
	}

	EMSCRIPTEN_KEEPALIVE void *createRGBABuffer(uint32_t width, uint32_t height)
	{
		return _me_aligned_malloc(width * height * 4);
	}

	EMSCRIPTEN_KEEPALIVE void *createRGBAfBuffer(uint32_t width, uint32_t height)
	{
		return _me_aligned_malloc(width * height * 4 * 4);
	}

	EMSCRIPTEN_KEEPALIVE void destroyBuffer(void *ptr)
	{
		uint32_t offset = *((uint32_t *)ptr - 1);
		free((uint8_t *)ptr - offset);
		return;
	}

#ifdef USE_WASM_SIMD
	void NV12toRgbaf(const uint8_t *nv12, float *rgb, uint32_t width, uint32_t height)
	{
		const int walign = 4;
		const int halign = 2;
		if (width % walign || height % halign)
			return;

		const uint8_t *lumaPtr = nv12;
		const uint8_t *chromaPtr = nv12 + width * height;

		const uint32_t innerLoop = 64;
		const uint32_t outc = 4;
		alignas(16) float buffer[innerLoop * 2 * 4];

		float *rgbPtr = rgb;
		for (int hi = 0; hi < height; hi += halign)
		{
			const uint8_t *lumaScan0 = &lumaPtr[0];
			const uint8_t *lumaScan1 = &lumaPtr[width];
			const uint8_t *chromaScan0 = chromaPtr;
			const uint8_t *chromaScan1 = &chromaPtr[width];
			float *rgbScan0 = rgbPtr;
			float *rgbScan1 = &rgbPtr[width * outc];

			for (int wi = 0; wi < width; wi += innerLoop)
			{
				float *rgbLocal0 = &buffer[0];
				float *rgbLocal1 = &buffer[innerLoop * outc];

				const uint32_t wunroll = (width - wi) > innerLoop ? innerLoop : (width - wi);

				__m128 _16 = _mm_set_ps1(-16.0f);
				__m128 _yi = _mm_set_ps1(1.164f / 255.0f);
				for (int wii = 0; wii < wunroll; wii += walign)
				{
					float luma00 = (float)lumaScan0[wi + wii];
					float luma01 = (float)lumaScan0[wi + wii + 1];
					float luma02 = (float)lumaScan0[wi + wii + 2];
					float luma03 = (float)lumaScan0[wi + wii + 3];

					__m128 rgb00f = _mm_set_ps(235.0722f, luma00, luma00, luma00);
					__m128 rgb01f = _mm_set_ps(235.0722f, luma01, luma01, luma01);
					__m128 rgb02f = _mm_set_ps(235.0722f, luma02, luma02, luma02);
					__m128 rgb03f = _mm_set_ps(235.0722f, luma03, luma03, luma03);

					rgb00f = _mm_add_ps(rgb00f, _16);
					rgb01f = _mm_add_ps(rgb01f, _16);
					rgb02f = _mm_add_ps(rgb02f, _16);
					rgb03f = _mm_add_ps(rgb03f, _16);

					rgb00f = _mm_mul_ps(rgb00f, _yi);
					rgb01f = _mm_mul_ps(rgb01f, _yi);
					rgb02f = _mm_mul_ps(rgb02f, _yi);
					rgb03f = _mm_mul_ps(rgb03f, _yi);

					float luma10 = (float)lumaScan1[wi + wii];
					float luma11 = (float)lumaScan1[wi + wii + 1];
					float luma12 = (float)lumaScan1[wi + wii + 2];
					float luma13 = (float)lumaScan1[wi + wii + 3];

					__m128 rgb10f = _mm_set_ps(235.0722f, luma10, luma10, luma10);
					__m128 rgb11f = _mm_set_ps(235.0722f, luma11, luma11, luma11);
					__m128 rgb12f = _mm_set_ps(235.0722f, luma12, luma12, luma12);
					__m128 rgb13f = _mm_set_ps(235.0722f, luma13, luma13, luma13);

					rgb10f = _mm_add_ps(rgb10f, _16);
					rgb11f = _mm_add_ps(rgb11f, _16);
					rgb12f = _mm_add_ps(rgb12f, _16);
					rgb13f = _mm_add_ps(rgb13f, _16);

					rgb10f = _mm_mul_ps(rgb10f, _yi);
					rgb11f = _mm_mul_ps(rgb11f, _yi);
					rgb12f = _mm_mul_ps(rgb12f, _yi);
					rgb13f = _mm_mul_ps(rgb13f, _yi);

					_mm_store_ps(&rgbLocal0[wii * outc], rgb00f);
					_mm_store_ps(&rgbLocal0[wii * outc + 4], rgb01f);
					_mm_store_ps(&rgbLocal0[wii * outc + 8], rgb02f);
					_mm_store_ps(&rgbLocal0[wii * outc + 12], rgb03f);

					_mm_store_ps(&rgbLocal1[wii * outc], rgb10f);
					_mm_store_ps(&rgbLocal1[wii * outc + 4], rgb11f);
					_mm_store_ps(&rgbLocal1[wii * outc + 8], rgb12f);
					_mm_store_ps(&rgbLocal1[wii * outc + 12], rgb13f);
				}

				__m128 _128 = _mm_set_ps1(-128.0f);
				__m128 ui0f = _mm_set_ps(0.0f, 2.018f / 255.0f, -0.391f / 255.0f, 0.0f);
				__m128 vi0f = _mm_set_ps(0.0f, 0.0f, -0.813f / 255.0f, 1.596f / 255.0f);
				__m128 _zero = _mm_set_ps1(0.0f);
				__m128 _one = _mm_set_ps1(1.0f);
				for (int wii = 0; wii < wunroll; wii += walign)
				{
					float *rgbScan00 = &rgbScan0[(wi + wii) * outc];
					float *rgbScan10 = &rgbScan1[(wi + wii) * outc];

					__m128 rgb00f = _mm_load_ps(&rgbLocal0[wii * outc]);
					__m128 rgb10f = _mm_load_ps(&rgbLocal1[wii * outc]);
					__m128 rgb01f = _mm_load_ps(&rgbLocal0[wii * outc + 4]);
					__m128 rgb11f = _mm_load_ps(&rgbLocal1[wii * outc + 4]);
					__m128 rgb02f = _mm_load_ps(&rgbLocal0[wii * outc + 8]);
					__m128 rgb12f = _mm_load_ps(&rgbLocal1[wii * outc + 8]);
					__m128 rgb03f = _mm_load_ps(&rgbLocal0[wii * outc + 12]);
					__m128 rgb13f = _mm_load_ps(&rgbLocal1[wii * outc + 12]);

					float u00 = (float)chromaScan0[wii + wi];
					float v00 = (float)chromaScan0[wii + wi + 1];
					float u01 = (float)chromaScan0[wii + wi + 2];
					float v01 = (float)chromaScan0[wii + wi + 3];

					__m128 u00f = _mm_set_ps(128.0f, u00, u00, u00);
					__m128 u01f = _mm_set_ps(128.0f, u01, u01, u01);
					__m128 v00f = _mm_set_ps(128.0f, v00, v00, v00);
					__m128 v01f = _mm_set_ps(128.0f, v01, v01, v01);

					u00f = _mm_add_ps(u00f, _128);
					u01f = _mm_add_ps(u01f, _128);
					v00f = _mm_add_ps(v00f, _128);
					v01f = _mm_add_ps(v01f, _128);

					u00f = _mm_mul_ps(u00f, ui0f);
					u01f = _mm_mul_ps(u01f, ui0f);
					v00f = _mm_mul_ps(v00f, vi0f);
					v01f = _mm_mul_ps(v01f, vi0f);

					rgb00f = _mm_add_ps(rgb00f, u00f);
					rgb00f = _mm_add_ps(rgb00f, v00f);
					rgb01f = _mm_add_ps(rgb01f, u00f);
					rgb01f = _mm_add_ps(rgb01f, v00f);
					// rgb00f = _mm_max_ps(_mm_min_ps(rgb00f, _one), _zero);
					// rgb01f = _mm_max_ps(_mm_min_ps(rgb01f, _one), _zero);

					rgb10f = _mm_add_ps(rgb10f, u00f);
					rgb10f = _mm_add_ps(rgb10f, v00f);
					rgb11f = _mm_add_ps(rgb11f, u00f);
					rgb11f = _mm_add_ps(rgb11f, v00f);
					// rgb10f = _mm_max_ps(_mm_min_ps(rgb10f, _one), _zero);
					// rgb11f = _mm_max_ps(_mm_min_ps(rgb11f, _one), _zero);
					_mm_store_ps(&rgbScan00[0], rgb00f);
					_mm_store_ps(&rgbScan00[4], rgb01f);

					rgb02f = _mm_add_ps(rgb02f, u01f);
					rgb02f = _mm_add_ps(rgb02f, v01f);
					rgb12f = _mm_add_ps(rgb12f, u01f);
					rgb12f = _mm_add_ps(rgb12f, v01f);
					// rgb02f = _mm_max_ps(_mm_min_ps(rgb02f, _one), _zero);
					// rgb12f = _mm_max_ps(_mm_min_ps(rgb12f, _one), _zero);
					_mm_store_ps(&rgbScan10[0], rgb10f);
					_mm_store_ps(&rgbScan10[4], rgb11f);

					rgb03f = _mm_add_ps(rgb03f, u01f);
					rgb03f = _mm_add_ps(rgb03f, v01f);
					rgb13f = _mm_add_ps(rgb13f, u01f);
					rgb13f = _mm_add_ps(rgb13f, v01f);
					// rgb03f = _mm_max_ps(_mm_min_ps(rgb03f, _one), _zero);
					// rgb13f = _mm_max_ps(_mm_min_ps(rgb13f, _one), _zero);
					_mm_store_ps(&rgbScan00[8], rgb02f);
					_mm_store_ps(&rgbScan00[12], rgb03f);
					_mm_store_ps(&rgbScan10[8], rgb12f);
					_mm_store_ps(&rgbScan10[12], rgb13f);
				}
			}

			lumaPtr += width * halign;
			chromaPtr += width * (halign >> 1);
			rgbPtr += width * outc * halign;
		}
	}

	void RgbatoNV12_shift(const uint8_t *rgba, uint8_t *nv12, uint32_t width, uint32_t height)
	{
		const int walign = 4;
		const int halign = 2;
		if (width % walign || height % halign)
			return;

		const uint8_t *rgbaPtr = rgba;

		const uint32_t innerLoop = 64;
		const uint32_t outc = 4;

		uint8_t *lumaPtr = nv12;
		uint8_t *chromaPtr = &nv12[width * height];

		alignas(16) int32_t buffer[innerLoop * 3];

		__m128i zero = _mm_set1_epi32(0);
		__m128i _16 = _mm_set1_epi32(16);
		__m128i _128 = _mm_set1_epi32(128);
		__m128i yi = _mm_set_epi16(0, 25, 129, 66, 0, 25, 129, 66);
		__m128i ui = _mm_set_epi16(0, 112, -74, -38, 0, 112, -74, -38);
		__m128i vi = _mm_set_epi16(0, -18, -94, 112, 0, -18, -94, 112);
		__m128i shufflag = _mm_set_epi8(0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 12, 8, 4, 0);

		for (int hi = 0; hi < height; hi += halign)
		{
			const uint8_t *rgbScan0 = &rgbaPtr[0];
			const uint8_t *rgbScan1 = &rgbaPtr[width * 4];
			uint8_t *lumaScan0 = &lumaPtr[0];
			uint8_t *lumaScan1 = &lumaPtr[width];
			uint8_t *chromaScan = &chromaPtr[0];

			for (int wi = 0; wi < width; wi += innerLoop)
			{
				const uint32_t wunroll = (width - wi) > innerLoop ? innerLoop : (width - wi);
				for (int wii = 0; wii < wunroll; wii += walign)
				{
					__m128i y00 = _mm_load_si128((__m128i *)&rgbScan0[(wi + wii) * 4]);
					__m128i y01 = _mm_load_si128((__m128i *)&rgbScan1[(wi + wii) * 4]);

					__m128i rgb00 = _mm_unpacklo_epi8(y00, zero);
					__m128i rgb01 = _mm_unpackhi_epi8(y00, zero);
					__m128i rgb10 = _mm_unpacklo_epi8(y01, zero);
					__m128i rgb11 = _mm_unpackhi_epi8(y01, zero);

					y00 = _mm_madd_epi16(rgb00, yi);
					y01 = _mm_madd_epi16(rgb01, yi);
					__m128i y10 = _mm_madd_epi16(rgb10, yi);
					__m128i y11 = _mm_madd_epi16(rgb11, yi);
					y00 = _mm_hadd_epi32(y00, y01);
					y10 = _mm_hadd_epi32(y10, y11);
					y00 = _mm_srai_epi32(y00, 8);
					y00 = _mm_add_epi32(y00, _16);
					y10 = _mm_srai_epi32(y10, 8);
					y10 = _mm_add_epi32(y10, _16);

					y00 = _mm_shuffle_epi8(y00, shufflag);
					y10 = _mm_shuffle_epi8(y10, shufflag);

					_mm_store_si128((__m128i *)(&buffer[wii]), y00);
					_mm_store_si128((__m128i *)(&buffer[innerLoop + wii]), y10);

					y01 = _mm_madd_epi16(rgb01, ui);
					y11 = _mm_madd_epi16(rgb11, ui);

					y00 = _mm_madd_epi16(rgb00, ui);
					y10 = _mm_madd_epi16(rgb10, ui);

					y01 = _mm_add_epi32(y01, y11);
					y00 = _mm_add_epi32(y00, y10);
					y00 = _mm_hadd_epi32(y00, y01);
					// y00 = _mm_srai_epi32(y00, 8);

					rgb00 = _mm_madd_epi16(rgb00, vi);
					rgb10 = _mm_madd_epi16(rgb10, vi);

					rgb01 = _mm_madd_epi16(rgb01, vi);
					rgb11 = _mm_madd_epi16(rgb11, vi);

					rgb00 = _mm_add_epi32(rgb00, rgb10);
					rgb01 = _mm_add_epi32(rgb01, rgb11);

					rgb00 = _mm_hadd_epi32(rgb00, rgb01);
					// v00 = _mm_srai_epi32(v00, 8);

					y10 = _mm_unpacklo_epi32(y00, rgb00);
					y11 = _mm_unpackhi_epi32(y00, rgb00);
					y00 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(y10), _mm_castsi128_ps(y11), 0b01000100));
					y01 = _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(y10), _mm_castsi128_ps(y11), 0b11101110));
					y00 = _mm_add_epi32(y00, y01);

					// y00 = _mm_srai_epi32(y00, 2);
					y00 = _mm_srai_epi32(y00, 10);
					y00 = _mm_add_epi32(y00, _128);
					y00 = _mm_shuffle_epi8(y00, shufflag);
					_mm_store_si128((__m128i *)(&buffer[innerLoop * 2 + wii]), y00);
				}
				for (int wii = 0; wii < wunroll; wii += walign)
				{
					*(int32_t *)&lumaScan0[wi + wii] = buffer[wii];
					*(int32_t *)&lumaScan1[wi + wii] = buffer[innerLoop + wii];
					*(int32_t *)&chromaScan[wi + wii] = buffer[innerLoop * 2 + wii];
				}
			}

			rgbaPtr += width * outc * halign;
			lumaPtr += width * halign;
			chromaPtr += width * (halign >> 1);
		}
	}
#else

void NV12toRgbaf(const uint8_t *nv12, float *rgb, uint32_t width, uint32_t height)
{
	const int walign = 4;
	const int halign = 2;
	if (width % walign || height % halign)
		return;

	const uint8_t *lumaPtr = nv12;
	const uint8_t *chromaPtr = nv12 + width * height;

	const uint32_t innerLoop = 128;
	const uint32_t outc = 4;

	float *rgbPtr = rgb;
	for (int hi = 0; hi < height; hi += halign)
	{
		const uint8_t *lumaScan0 = &lumaPtr[0];
		const uint8_t *lumaScan1 = &lumaPtr[width];
		const uint8_t *chromaScan0 = &chromaPtr[0];
		const uint8_t *chromaScan1 = &chromaPtr[width];
		float *rgbScan0 = rgbPtr;
		float *rgbScan1 = &rgbPtr[width * outc];

		for (int wi = 0; wi < width; wi += innerLoop)
		{
			float *rgbScan0w = &rgbScan0[wi * outc];
			float *rgbScan1w = &rgbScan1[wi * outc];

			const uint32_t wunroll = (width - wi) > innerLoop ? innerLoop : (width - wi);

			const float _yi = 1.164f / 255.0f;
			for (int wii = 0; wii < wunroll; wii += walign)
			{
				float luma00 = (float)lumaScan0[wi + wii + 0] - 16.0f;
				float luma01 = (float)lumaScan0[wi + wii + 1] - 16.0f;
				float luma02 = (float)lumaScan0[wi + wii + 2] - 16.0f;
				float luma03 = (float)lumaScan0[wi + wii + 3] - 16.0f;

				luma00 = luma00 * _yi;
				luma01 = luma01 * _yi;
				luma02 = luma02 * _yi;
				luma03 = luma03 * _yi;

				float luma10 = (float)lumaScan1[wi + wii + 0] - 16.0f;
				float luma11 = (float)lumaScan1[wi + wii + 1] - 16.0f;
				float luma12 = (float)lumaScan1[wi + wii + 2] - 16.0f;
				float luma13 = (float)lumaScan1[wi + wii + 3] - 16.0f;

				luma10 = luma10 * _yi;
				luma11 = luma11 * _yi;
				luma12 = luma12 * _yi;
				luma13 = luma13 * _yi;

				rgbScan0w[wii * outc + 0] = luma00;
				rgbScan0w[wii * outc + 1] = luma00;
				rgbScan0w[wii * outc + 2] = luma00;
				rgbScan0w[wii * outc + 3] = 1.0;
				rgbScan0w[wii * outc + 4] = luma01;
				rgbScan0w[wii * outc + 5] = luma01;
				rgbScan0w[wii * outc + 6] = luma01;
				rgbScan0w[wii * outc + 7] = 1.0;
				rgbScan0w[wii * outc + 8] = luma02;
				rgbScan0w[wii * outc + 9] = luma02;
				rgbScan0w[wii * outc + 10] = luma02;
				rgbScan0w[wii * outc + 11] = 1.0;
				rgbScan0w[wii * outc + 12] = luma02;
				rgbScan0w[wii * outc + 13] = luma02;
				rgbScan0w[wii * outc + 14] = luma02;
				rgbScan0w[wii * outc + 15] = 1.0;

				rgbScan1w[wii * outc + 0] = luma10;
				rgbScan1w[wii * outc + 1] = luma10;
				rgbScan1w[wii * outc + 2] = luma10;
				rgbScan1w[wii * outc + 3] = 1.0;
				rgbScan1w[wii * outc + 4] = luma11;
				rgbScan1w[wii * outc + 5] = luma11;
				rgbScan1w[wii * outc + 6] = luma11;
				rgbScan1w[wii * outc + 7] = 1.0;
				rgbScan1w[wii * outc + 8] = luma12;
				rgbScan1w[wii * outc + 9] = luma12;
				rgbScan1w[wii * outc + 10] = luma12;
				rgbScan1w[wii * outc + 11] = 1.0;
				rgbScan1w[wii * outc + 12] = luma13;
				rgbScan1w[wii * outc + 13] = luma13;
				rgbScan1w[wii * outc + 14] = luma13;
				rgbScan1w[wii * outc + 15] = 1.0;
			}

			const float vgf = -0.813f / 255.0f;
			const float vrf = 1.596f / 255.0f;
			const float ugf = -0.391f / 255.0f;
			const float ubf = 2.018f / 255.0f;
			for (int wii = 0; wii < wunroll; wii += walign)
			{
				float *rgbScan0w = &rgbScan0[wi * outc];
				float *rgbScan1w = &rgbScan1[wi * outc];

				float u00 = (float)chromaScan0[wii + wi] - 128.0f;
				float v00 = (float)chromaScan0[wii + wi + 1] - 128.0f;
				float u01 = (float)chromaScan0[wii + wi + 2] - 128.0f;
				float v01 = (float)chromaScan0[wii + wi + 3] - 128.0f;

				{
					float ro = vrf * v00;
					float go = vgf * v00 + ugf * u00;
					float bo = ubf * u00;

					float &_00r = rgbScan0w[wii * outc + 0];
					float &_00g = rgbScan0w[wii * outc + 1];
					float &_00b = rgbScan0w[wii * outc + 2];

					float &_10r = rgbScan1w[wii * outc + 0];
					float &_10g = rgbScan1w[wii * outc + 1];
					float &_10b = rgbScan1w[wii * outc + 2];

					float &_01r = rgbScan0w[wii * outc + 4];
					float &_01g = rgbScan0w[wii * outc + 5];
					float &_01b = rgbScan0w[wii * outc + 6];

					float &_11r = rgbScan1w[wii * outc + 4];
					float &_11g = rgbScan1w[wii * outc + 5];
					float &_11b = rgbScan1w[wii * outc + 6];

					_00r += ro;
					_01r += ro;
					_10r += ro;
					_11r += ro;

					_00g += go;
					_01g += go;
					_10g += go;
					_11g += go;

					_00b += bo;
					_01b += bo;
					_10b += bo;
					_11b += bo;
				}

				{
					float ro = vrf * v01;
					float go = vgf * v01 + ugf * u01;
					float bo = ubf * u01;

					float &_02r = rgbScan0w[wii * outc + 8];
					float &_02g = rgbScan0w[wii * outc + 9];
					float &_02b = rgbScan0w[wii * outc + 10];

					float &_12r = rgbScan1w[wii * outc + 8];
					float &_12g = rgbScan1w[wii * outc + 9];
					float &_12b = rgbScan1w[wii * outc + 10];

					float &_03r = rgbScan0w[wii * outc + 12];
					float &_03g = rgbScan0w[wii * outc + 13];
					float &_03b = rgbScan0w[wii * outc + 14];

					float &_13r = rgbScan1w[wii * outc + 12];
					float &_13g = rgbScan1w[wii * outc + 13];
					float &_13b = rgbScan1w[wii * outc + 14];

					_02r += ro;
					_03r += ro;
					_12r += ro;
					_13r += ro;

					_02g += go;
					_03g += go;
					_12g += go;
					_13g += go;

					_02b += bo;
					_03b += bo;
					_12b += bo;
					_13b += bo;
				}
			}
		}

		lumaPtr += width * halign;
		chromaPtr += width * (halign >> 1);
		rgbPtr += width * outc * halign;
	}
}

void NV12toRgba_shift(const uint8_t *nv12, uint8_t *rgb, uint32_t width, uint32_t height)
{
	const int walign = 2;
	const int halign = 2;
	if (width % walign || height % halign)
		return;

	const uint8_t *lumaPtr = nv12;
	const uint8_t *chromaPtr = nv12 + width * height;

	const uint32_t innerLoop = 64;
	const uint32_t outc = 4;

	uint8_t *rgbPtr = rgb;
	const int32_t _yi = 298;
	const int32_t vgf = -208;
	const int32_t vrf = 409;
	const int32_t ugf = -100;
	const int32_t ubf = 517;

	for (int hi = 0; hi < height; hi += halign)
	{
		const uint8_t *lumaScan0 = &lumaPtr[0];
		const uint8_t *lumaScan1 = &lumaPtr[width];
		const uint8_t *chromaScan0 = &chromaPtr[0];
		const uint8_t *chromaScan1 = &chromaPtr[width];
		uint8_t *rgbScan0 = rgbPtr;
		uint8_t *rgbScan1 = &rgbPtr[width * outc];

		for (int wi = 0; wi < width; wi += innerLoop)
		{
			const uint32_t wunroll = (width - wi) > innerLoop ? innerLoop : (width - wi);

			uint8_t *rgbScan0w = &rgbScan0[wi * outc];
			uint8_t *rgbScan1w = &rgbScan1[wi * outc];

			for (int wii = 0; wii < wunroll; wii += walign)
			{
				int32_t luma00 = (int32_t)lumaScan0[wi + wii + 0] - 16;
				int32_t luma01 = (int32_t)lumaScan0[wi + wii + 1] - 16;

				luma00 *= _yi;
				luma01 *= _yi;

				int32_t luma10 = (int32_t)lumaScan1[wi + wii + 0] - 16;
				int32_t luma11 = (int32_t)lumaScan1[wi + wii + 1] - 16;

				luma10 *= _yi;
				luma11 *= _yi;

				int32_t u00 = (int32_t)chromaScan0[wii + wi] - 128;
				int32_t v00 = (int32_t)chromaScan0[wii + wi + 1] - 128;

				int32_t ro = vrf * v00;
				int32_t go = vgf * v00 + ugf * u00;
				int32_t bo = ubf * u00;

				uint8_t &_00r = rgbScan0w[wii * outc + 0];
				uint8_t &_00g = rgbScan0w[wii * outc + 1];
				uint8_t &_00b = rgbScan0w[wii * outc + 2];
				uint8_t &_00a = rgbScan0w[wii * outc + 3];

				uint8_t &_10r = rgbScan1w[wii * outc + 0];
				uint8_t &_10g = rgbScan1w[wii * outc + 1];
				uint8_t &_10b = rgbScan1w[wii * outc + 2];
				uint8_t &_10a = rgbScan1w[wii * outc + 3];

				uint8_t &_01r = rgbScan0w[wii * outc + 4];
				uint8_t &_01g = rgbScan0w[wii * outc + 5];
				uint8_t &_01b = rgbScan0w[wii * outc + 6];
				uint8_t &_01a = rgbScan0w[wii * outc + 7];

				uint8_t &_11r = rgbScan1w[wii * outc + 4];
				uint8_t &_11g = rgbScan1w[wii * outc + 5];
				uint8_t &_11b = rgbScan1w[wii * outc + 6];
				uint8_t &_11a = rgbScan1w[wii * outc + 7];

				*(int32_t *)&rgbScan0w[wii * outc + 0] = (std::max(std::min((luma00 + ro), 65535), 0) >> 8) | (std::max(std::min((luma00 + go), 65535), 0) & 0x0000ff00) | ((std::max(std::min((luma00 + bo), 65535), 0) << 8) & 0x00ff0000) | 0xff000000;
				*(int32_t *)&rgbScan0w[wii * outc + 4] = (std::max(std::min((luma01 + ro), 65535), 0) >> 8) | (std::max(std::min((luma01 + go), 65535), 0) & 0x0000ff00) | ((std::max(std::min((luma01 + bo), 65535), 0) << 8) & 0x00ff0000) | 0xff000000;
				*(int32_t *)&rgbScan1w[wii * outc + 0] = (std::max(std::min((luma10 + ro), 65535), 0) >> 8) | (std::max(std::min((luma10 + go), 65535), 0) & 0x0000ff00) | ((std::max(std::min((luma10 + bo), 65535), 0) << 8) & 0x00ff0000) | 0xff000000;
				*(int32_t *)&rgbScan1w[wii * outc + 4] = (std::max(std::min((luma11 + ro), 65535), 0) >> 8) | (std::max(std::min((luma11 + go), 65535), 0) & 0x0000ff00) | ((std::max(std::min((luma11 + bo), 65535), 0) << 8) & 0x00ff0000) | 0xff000000;
			}
		}

		lumaPtr += width * halign;
		chromaPtr += width * (halign >> 1);
		rgbPtr += width * outc * halign;
	}
}

#endif

	EMSCRIPTEN_KEEPALIVE void YUV32toNV12(const uint8_t *yuvU32, uint8_t *nv12, uint32_t width, uint32_t height)
	{
		const int walign = 4;
		const int halign = 4;
		if (width % walign || height % halign)
			return;

		const uint32_t outc = 4;

		const uint8_t *yuvPtr = yuvU32;
		uint8_t *lumaPtr = nv12;
		uint8_t *chromaPtr = nv12 + width * height;

		const uint32_t wstride = width * outc;
		for (int hi = 0; hi < height; hi += halign)
		{
			const uint8_t *yuvScan0 = &yuvPtr[0];
			const uint8_t *yuvScan1 = &yuvPtr[wstride];
			const uint8_t *yuvScan2 = &yuvPtr[wstride * 2];
			const uint8_t *yuvScan3 = &yuvPtr[wstride * 3];
			for (int wi = 0; wi < width; wi += walign)
			{
				const uint32_t offset = wi * outc;
				uint8_t *lumaTmp = &lumaPtr[wi];
				lumaTmp[0] = yuvScan0[offset];
				lumaTmp[width] = yuvScan1[offset];
				lumaTmp[width * 2] = yuvScan2[offset];
				lumaTmp[width * 3] = yuvScan3[offset];
				lumaTmp[1] = yuvScan0[offset + 4];
				lumaTmp[width + 1] = yuvScan1[offset + 4];
				lumaTmp[width * 2 + 1] = yuvScan2[offset + 4];
				lumaTmp[width * 3 + 1] = yuvScan3[offset + 4];
				lumaTmp[2] = yuvScan0[offset + 8];
				lumaTmp[width + 2] = yuvScan1[offset + 8];
				lumaTmp[width * 2 + 2] = yuvScan2[offset + 8];
				lumaTmp[width * 3 + 2] = yuvScan3[offset + 8];
				lumaTmp[3] = yuvScan0[offset + 12];
				lumaTmp[width + 3] = yuvScan1[offset + 12];
				lumaTmp[width * 2 + 3] = yuvScan2[offset + 12];
				lumaTmp[width * 3 + 3] = yuvScan3[offset + 12];

				uint8_t *chromaTmp = &chromaPtr[wi];
				chromaTmp[0] = yuvScan0[offset + 1];
				chromaTmp[1] = yuvScan0[offset + 2];
				chromaTmp[width] = yuvScan2[offset + 1];
				chromaTmp[width + 1] = yuvScan2[offset + 2];

				chromaTmp[2] = yuvScan0[offset + 9];
				chromaTmp[3] = yuvScan0[offset + 10];
				chromaTmp[width + 2] = yuvScan2[offset + 9];
				chromaTmp[width + 3] = yuvScan2[offset + 10];
			}

			yuvPtr += width * outc * halign;
			lumaPtr += width * halign;
			chromaPtr += width * (halign >> 1);
		}
		return;
	}

	EMSCRIPTEN_KEEPALIVE float printValuef(void *rgb, uint32_t index)
	{
		float *p = (float *)rgb;
		return p[index];
	}

	EMSCRIPTEN_KEEPALIVE uint8_t printValue(void *rgb, uint32_t index)
	{
		uint8_t *p = (uint8_t *)rgb;
		return p[index];
	}

	EMSCRIPTEN_KEEPALIVE int32_t speedtest(void *nv12, void *rgba, uint32_t width, uint32_t height, int32_t testCnt)
	{
		auto start = std::chrono::high_resolution_clock::now();
		int32_t testC = testCnt;
		while (testC--)
		{
			NV12toRgba_shift((const uint8_t *)nv12, (uint8_t *)rgba, width, height);
		}
		auto end = std::chrono::high_resolution_clock::now();
		return (end - start).count() / 1E3 / testCnt;
	}

	EMSCRIPTEN_KEEPALIVE int32_t speedtestf(void *nv12, void *rgb, uint32_t width, uint32_t height, int32_t testCnt)
	{
		auto start = std::chrono::high_resolution_clock::now();
		int32_t testC = testCnt;
		while (testC--)
		{
			NV12toRgbaf((const uint8_t *)nv12, (float *)rgb, width, height);
		}
		auto end = std::chrono::high_resolution_clock::now();
		return (end - start).count() / 1E3 / testCnt;
	}

	EMSCRIPTEN_KEEPALIVE int32_t speedtest_YUV32toNV12(void *rgba, void *nv12, uint32_t width, uint32_t height, int32_t testCnt)
	{
		auto start = std::chrono::high_resolution_clock::now();
		int32_t testC = testCnt;
		while (testC--)
		{
			YUV32toNV12((const uint8_t *)rgba, (uint8_t *)nv12, width, height);
		}
		auto end = std::chrono::high_resolution_clock::now();
		return (end - start).count() / 1E3 / testCnt;
	}

#ifdef __cplusplus
}
#endif
