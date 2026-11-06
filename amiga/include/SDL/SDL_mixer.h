/* VoR for AmigaOS (from the Ceferino, Critical Mass, TecnoballZ,
 * Wizznic, Gigalomania, Holotz's Castle and BlockOut II ports): the
 * SDL_mixer 1.2 calls it makes, on SDL 1.2's own audio, with libmikmod for
 * the music (amiga/sdl_mixer_lite.c). The OpenGPU kit's SDL 1.2
 * (sdl12-compat) comes without an SDL_mixer 1.2. The AmiCradle Team. GPL 3
 * or later, like Wizznic. */
#ifndef AMIGA_SDL_MIXER_LITE_H
#define AMIGA_SDL_MIXER_LITE_H
#include <SDL.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SDL_MIXER_MAJOR_VERSION 1
#define SDL_MIXER_MINOR_VERSION 2
#define SDL_MIXER_PATCHLEVEL 12
#define MIX_MAX_VOLUME 128
#define MIX_DEFAULT_FORMAT AUDIO_S16SYS
#define MIX_DEFAULT_FREQUENCY 22050
#define MIX_DEFAULT_CHANNELS 2
#define MIX_CHANNEL_POST (-2)

typedef struct _Mix_Music Mix_Music;

typedef struct Mix_Chunk {
	int allocated;
	Uint8 *abuf;	/* 16-bit mono at the output rate */
	Uint32 alen;
	Uint8 volume;
} Mix_Chunk;

/* JLib casts a channel number to it (and reads no fading from it) */
typedef enum { MIX_NO_FADING, MIX_FADING_OUT, MIX_FADING_IN } Mix_Fading;

typedef void (*Mix_EffectFunc_t)(int chan, void *stream, int len, void *udata);
typedef void (*Mix_EffectDone_t)(int chan, void *udata);

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
void Mix_CloseAudio(void);
int Mix_AllocateChannels(int numchans);

Mix_Chunk *Mix_LoadWAV(const char *file);
void Mix_FreeChunk(Mix_Chunk *chunk);
int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int Mix_Playing(int channel);
int Mix_Volume(int channel, int volume);
int Mix_HaltChannel(int channel);
void Mix_Pause(int channel);
void Mix_Resume(int channel);
int Mix_Paused(int channel);
int Mix_FadeOutChannel(int channel, int ms);
int Mix_SetPanning(int channel, Uint8 left, Uint8 right);
int Mix_RegisterEffect(int chan, Mix_EffectFunc_t f, Mix_EffectDone_t d, void *arg);

Mix_Music *Mix_LoadMUS(const char *file);
void Mix_FreeMusic(Mix_Music *music);
int Mix_PlayMusic(Mix_Music *music, int loops);
int Mix_FadeInMusic(Mix_Music *music, int loops, int ms);
int Mix_FadeInMusicPos(Mix_Music *music, int loops, int ms, double position);
int Mix_FadeOutMusic(int ms);
int Mix_HaltMusic(void);
void Mix_PauseMusic(void);
void Mix_ResumeMusic(void);
int Mix_PausedMusic(void);
int Mix_PlayingMusic(void);
int Mix_VolumeMusic(int volume);

/* for TecnoballZ: a pattern of the playing module */
int Mix_SetMusicPosition(double position);

/* for Critical Mass */
Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int freesrc);
void Mix_HookMusicFinished(void (*music_finished)(void));
Mix_Fading Mix_FadingMusic(void);

/* for Holotz's Castle (JLib) */
Mix_Chunk *Mix_GetChunk(int channel);
int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels);
int Mix_ExpireChannel(int channel, int ticks);
int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks);
int Mix_FadeInChannel(int channel, Mix_Chunk *chunk, int loops, int ms);
int Mix_FadeInChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ms, int ticks);
int Mix_VolumeChunk(Mix_Chunk *chunk, int volume);
const SDL_version *Mix_Linked_Version(void);

/* for BlockOut II */
int Mix_ReserveChannels(int num);

#define Mix_GetError SDL_GetError
#define Mix_SetError SDL_SetError
#ifdef __cplusplus
}
#endif
#endif
