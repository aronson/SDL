//
// Created by Isaac Aronson on 9/1/23.
//
#include "../../SDL_internal.h"

#if SDL_HAVE_BLIT_A

#include "../SDL_blit.h"

#ifndef _MSC_VER
__attribute__((target("sse4.1")))
#endif
/**
 * Using the SSE4.1 instruction set, blit four pixels with JellySquid's alpha blending routine.
 * @param src A pointer to two 32-bit pixels of ARGB format to blit into dst
 * @param dst A pointer to two 32-bit pixels of ARGB format to retain visual data for while alpha blending
 * @return A 128-bit wide vector of two alpha-blended pixels in ARGB format
 */
__m128i MixRGBA_SSE4_1(__m128i src, __m128i dst) {
    // Unpack 2 32-bit ARGB 8 bit elements into a vector of 128 bits wide
    __m128i src_color = _mm_cvtepu8_epi16(src);
    // Similarly unpack the pixels currently in the screen buffer
    __m128i dst_color = _mm_cvtepu8_epi16(dst);
    /**
     * Combines a shuffle and an _mm_cvtepu8_epi16 operation into one operation by moving the lower 8 bits of the alpha
     * channel around to create 16-bit integers.
     */
    const __m128i SHUFFLE_ALPHA = _mm_set_epi8(
         -1, 7, -1, 7, -1, 7, -1, 7,
         -1, 3, -1, 3, -1, 3, -1, 3);
    // This extracts 4 x 2 copies of the alpha channel relevant to each 8bit so we can place them next to each other
    __m128i alpha = _mm_shuffle_epi8(src, SHUFFLE_ALPHA);
    // This subtracts the src color from the destination color to find the difference the alpha channel represents
    // The subtraction happens on 16-bit integers?
    __m128i sub = _mm_sub_epi16(src_color, dst_color);
    // This sets the relative intensity of the subtracted difference of each channel element against the desired alpha intensity
    // We are going to take the low bits of the of intermediate integers, because alpha was in the lower section of our shuffle
    __m128i mul = _mm_mullo_epi16(sub, alpha);
    // In the second row of this constant, we take the lower 8 bits of each packed 16-bit integer in the vector, and
    // pack them into 8-bit integers.
    const __m128i SHUFFLE_REDUCE = _mm_set_epi8(
        -1, -1, -1, -1, -1, -1, -1, -1,
        15, 13, 11, 9, 7, 5, 3, 1);
    __m128i reduced = _mm_shuffle_epi8(mul, SHUFFLE_REDUCE);

    // Return the result of adding the reduced set of differences to the destination rect
    return _mm_add_epi8(reduced, dst);
}

Uint32 convertPixelFormat(Uint32 color, const SDL_PixelFormat* srcFormat) {
    Uint8 a = (color >> srcFormat->Ashift) & 0xFF;
    Uint8 r = (color >> srcFormat->Rshift) & 0xFF;
    Uint8 g = (color >> srcFormat->Gshift) & 0xFF;
    Uint8 b = (color >> srcFormat->Bshift) & 0xFF;

    return (a << 24) | (r << 16) | (g << 8) | b;
}

#ifndef _MSC_VER
__attribute__((target("sse4.1")))
#endif
/*
 * This helper function converts arbitrary pixel format data into ARGB form with a 4 pixel-wide shuffle
 */
__m128i convertPixelFormatsx4(__m128i colors, const SDL_PixelFormat* srcFormat) {
    // Create shuffle masks based on the source SDL_PixelFormat to ARGB
    __m128i srcShuffleMask = _mm_set_epi8(
        srcFormat->Ashift / 8 + 12, srcFormat->Rshift / 8 + 12, srcFormat->Gshift / 8 + 12, srcFormat->Bshift / 8 + 12,
        srcFormat->Ashift / 8 + 8, srcFormat->Rshift / 8 + 8, srcFormat->Gshift / 8 + 8, srcFormat->Bshift / 8 + 8,
        srcFormat->Ashift / 8 + 4, srcFormat->Rshift / 8 + 4, srcFormat->Gshift / 8 + 4, srcFormat->Bshift / 8 + 4,
        srcFormat->Ashift / 8, srcFormat->Rshift / 8, srcFormat->Gshift / 8, srcFormat->Bshift / 8
    );

    // Shuffle the colors
    return _mm_shuffle_epi8(colors, srcShuffleMask);
}

#ifndef _MSC_VER
__attribute__((target("sse4.1")))
#endif
void BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo* info) {
    int width = info->dst_w;
    int height = info->dst_h;
    Uint8 *src = info->src;
    int srcskip = info->src_skip;
    Uint8 *dst = info->dst;
    int dstskip = info->dst_skip;
    SDL_PixelFormat *srcfmt = info->src_fmt;

    int chunks = width / 4;
    Uint8 *buffer = (Uint8*)SDL_malloc(chunks * 16 * sizeof(Uint8));

    while (height--) {
        /* Process 4-wide chunks of source color data that may be in wrong format into buffer */
        for (int i = 0; i < chunks; i += 1) {
            __m128i colors = _mm_loadu_si128((__m128i*)(src + i * 16));
            _mm_storeu_si128((__m128i*)(buffer + i * 16), convertPixelFormatsx4(colors, srcfmt));
        }

        /* Alpha-blend in 2-wide chunks from buffer into destination */
        for (int i = 0; i < chunks * 2; i += 1) {
            __m128i c_src = _mm_loadu_si64((buffer + (i * 8)));
            __m128i c_dst = _mm_loadu_si64((dst + i * 8));
            __m128i c_mix = MixRGBA_SSE4_1(c_src, c_dst);
            _mm_storeu_si64(dst + i * 8, c_mix);
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

    SDL_free(buffer);
}

#endif
