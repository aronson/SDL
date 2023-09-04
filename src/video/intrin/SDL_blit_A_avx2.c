//
// Created by Isaac Aronson on 9/1/23.
//
#include "../../SDL_internal.h"

#if SDL_HAVE_BLIT_A

#include "../SDL_blit.h"
#include "SDL_blit_A_sse4.1.h"

#ifndef _MSC_VER
__attribute__((target("avx2")))
#endif
/**
 * Using the AVX2 instruction set, blit eight pixels with JellySquid's alpha blending routine.
 * @param src A pointer to four 32-bit pixels of ARGB format to blit into dst
 * @param dst A pointer to four 32-bit pixels of ARGB format to retain visual data for while alpha blending
 * @return A 128-bit wide vector of four alpha-blended pixels in ARGB format
 */
__m128i MixRGBA_AVX2(__m128i src, __m128i dst) {
    // Unpack 4 32-bit ARGB 8 bit elements into a vector of 256 bits wide
    // This comprises the upper part of a 16-bit integer set to blank, lower set to 8-bit value
    __m256i src_color = _mm256_cvtepu8_epi16(src);
    // Similarly unpack the pixels currently in the screen buffer
    __m256i dst_color = _mm256_cvtepu8_epi16(dst);
    // We can ignore the high bits (-1) because of our extended vector packing
    // We instead select the subcomponent of the source vector for each of the 4 pixels for each channel
    const __m256i SHUFFLE_ALPHA = _mm256_set_epi8(
         -1, 30, -1, 30, -1, 30, -1, 30,
         -1, 22, -1, 22, -1, 22, -1, 22,
         -1, 14, -1, 14, -1, 14, -1, 14,
         -1, 6, -1, 6, -1, 6, -1, 6);
    // Calling the shuffle routine, we pull out corresponding duplicates of the alpha value for other channels
    __m256i alpha = _mm256_shuffle_epi8(src_color, SHUFFLE_ALPHA);
    // Subtract src colors from destination colors to reason with the actual difference the alpha may convey upon the destination
    __m256i sub = _mm256_sub_epi16(src_color, dst_color);
    // Change this difference based on the intensity of alpha through a multiply operation that will result in wide integers
    __m256i mul = _mm256_mullo_epi16(sub, alpha);
    /**
     * With an 8-bit shuffle, one can only move integers within a lane. The 256-bit AVX2 lane is actually 4 64-bit
     * lanes. We pack the integers into the start of each lane. The second shuffle operates on these 64-bit integers to
     * put them into the correct order for transport back to the surface as SDL expects.
     */
    const __m256i SHUFFLE_REDUCE = _mm256_set_epi8(
            -1, -1, -1, -1, -1, -1, -1, -1,
            31, 29, 27, 25, 23, 21, 19, 17,
            -1, -1, -1, -1, -1, -1, -1, -1,
            15, 13, 11, 9, 7, 5, 3, 1);
    __m256i reduced = _mm256_shuffle_epi8(mul, SHUFFLE_REDUCE);
    __m256i packed = _mm256_permute4x64_epi64(reduced, _MM_SHUFFLE(3, 1, 2, 0));
    // Take the lower 128 bits of the packed 256-bit vector to reduce it back to a 128-bit register
    __m128i mix = _mm256_castsi256_si128(packed);
    // Then with the new 128-bit vector mix the alpha-blended color data into the original destination surface
    return _mm_add_epi8(mix, dst);
}

void BlitNtoNPixelAlpha_AVX2(SDL_BlitInfo *info)
{
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    SDL_PixelFormat *srcfmt = info->src_fmt;

    int chunks = width / 4;

    while (height--) {
        /* Process 4-wide chunks of source color data that may be in wrong format */
        for (int i = 0; i < chunks; i += 1) {
            __m128i c_src = convertPixelFormatsx4(_mm_loadu_si128((__m128i*) (src + i * 16)), srcfmt);
            __m128i c_dst = _mm_loadu_si128((__m128i*) (dst + i * 16));

            /* Alpha-blend in 4-wide chunk from src into destination */
            __m128i c_mix = MixRGBA_AVX2(c_src, c_dst);
            _mm_storeu_si128((__m128i*) (dst + i * 16), c_mix);
        }

        /* Handle remaining pixels when width is not a multiple of 4 */
        if (width % 4 != 0) {
            int remaining_pixels = width % 4;
            int offset = width - remaining_pixels;
            if (remaining_pixels >= 2) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                __m128i c_src = _mm_loadu_si64(src_ptr);
                __m128i c_dst;
                __m128i c_mix;
                c_src = convertPixelFormatsx4(c_src, srcfmt);
                c_dst = _mm_loadu_si64(dst_ptr);
                c_mix = MixRGBA_SSE4_1(c_src, c_dst);
                _mm_storeu_si64(dst_ptr, c_mix);
                remaining_pixels -= 2;
                offset += 2;
            }
            if (remaining_pixels == 1) {
                Uint32 *src_ptr = ((Uint32*)(src + (offset * 4)));
                Uint32 *dst_ptr = ((Uint32*)(dst + (offset * 4)));
                Uint32 pixel = convertPixelFormat(*src_ptr, srcfmt);
                /* Old GCC has bad or no _mm_loadu_si32 */
                #if defined(__GNUC__) && (__GNUC__ < 11)
                __m128i c_src = _mm_set_epi32(0, 0, 0, pixel);
                __m128i c_dst = _mm_set_epi32(0, 0, 0, *dst_ptr);
                #else
                __m128i c_src = _mm_loadu_si32(&pixel);
                __m128i c_dst = _mm_loadu_si32(dst_ptr);
                #endif
                __m128i mixed_pixel = MixRGBA_SSE4_1(c_src, c_dst);
                _mm_storeu_si32(dst_ptr, mixed_pixel);
            }
        }

        src += 4 * width;
        dst += 4 * width;

        src += srcskip;
        dst += dstskip;
    }

}

#endif
