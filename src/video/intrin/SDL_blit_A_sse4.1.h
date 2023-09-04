//
// Created by Isaac Aronson on 9/1/23.
//

#ifndef SDL_SDL_BLIT_A_SSE4_1_H
#define SDL_SDL_BLIT_A_SSE4_1_H

Uint32 convertPixelFormat(Uint32 color, const SDL_PixelFormat* srcFormat);

#ifndef _MSC_VER
__attribute__((target("sse4.1")))
#endif
__m128i convertPixelFormatsx4(__m128i colors, const SDL_PixelFormat* srcFormat);

#ifndef _MSC_VER
__attribute__((target("sse4.1")))
#endif
__m128i MixRGBA_SSE4_1(__m128i src, __m128i dst);

void BlitNtoNPixelAlpha_SSE4_1(SDL_BlitInfo *info);

#endif //SDL_SDL_BLIT_A_SSE4_1_H
