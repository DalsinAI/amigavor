/* VoR for AmigaOS (from the Ceferino, Penguin Command, Critical Mass,
 * TecnoballZ and Biniax-2 ports): the SDL_image 1.2 calls it makes, for its
 * PNGs and JPEGs (amiga/sdl_image_lite.c, on libpng and libjpeg). The
 * OpenGPU kit's SDL 1.2 (sdl12-compat) comes without an SDL_image 1.2. The
 * AmiCradle Team. GPL 2 or later, like VoR. */
#ifndef AMIGA_SDL_IMAGE_LITE_H
#define AMIGA_SDL_IMAGE_LITE_H
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
SDL_Surface *IMG_Load(const char *file);
SDL_Surface *IMG_LoadPNG_RW(SDL_RWops *src);
int IMG_InvertAlpha(int on);
const SDL_version *IMG_Linked_Version(void);
#define IMG_GetError SDL_GetError
#define IMG_SetError SDL_SetError
#ifdef __cplusplus
}
#endif
#endif
