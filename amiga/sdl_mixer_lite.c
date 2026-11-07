/* The SDL_mixer 1.2 calls VoR makes, on SDL 1.2's audio
 * (see include/SDL/SDL_mixer.h), from the Team's Ceferino and Critical Mass
 * mixer
 * (itself the TecnoballZ, Wizznic, Gigalomania, Holotz's Castle and
 * BlockOut II one).
 *
 * Output: 16-bit signed stereo, native order, 22050 Hz, whatever the game
 * asks; SDL_OpenAudio is given no "obtained" spec, so SDL converts if the
 * device wants something else.
 *
 * Samples: WAV, 8/16-bit PCM, mono or stereo, any rate, converted once at
 * load to 16-bit mono at the output rate, so the 68k only adds them up,
 * each with its channel's volume and left/right panning.
 *
 * Music: the game's FastTracker module (data/mph.xm, "VoR Theme"), played by
 * libmikmod (amiga/libmikmod) as SDL_mixer 1.2 plays it: mikmod's no-sound
 * driver, its software mixer (VC_WriteBytes) filling the music's part of
 * each buffer, at most 16 voices (Biniax-2 port).
 *
 * The files are read with stdio, not SDL_RWops: sdl12-compat's
 * SDL_LoadWAV_RW never returned in the lab (OpenLieroX port).
 * The AmiCradle Team. GPL 3 or later, like Wizznic. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SDL_mixer.h"
#include <mikmod.h>

#define OUT_RATE 22050
#define MAX_CHANS 16
#define SEG 512

struct channel {
	Mix_Chunk *chunk;
	Uint32 pos;	/* in samples */
	int volume;	/* 0..128 */
	int left, right;	/* 0..255 */
	int loops;	/* plays left after this one; -1 for ever */
	int paused;
	Uint32 fade_total, fade_left;	/* fading out over fade_total samples */
	Uint32 expire;	/* samples left before it stops, 0: none */
};

/* as SDL_mixer 1.2's: the game finds the MODULE after the first int */
struct _Mix_Music {
	int type;
	MODULE *module;
};

static struct channel chans[MAX_CHANS];
static int nchans = 8;
static int reserved;	/* Mix_ReserveChannels: not given to channel -1 */
static int opened;
static int outchans = 2;

/* music */
static Mix_Music *mus;
static int mus_playing, mus_paused, mus_loops;
static int mus_volume = MIX_MAX_VOLUME;
static int fade_mode;	/* 0 none, 1 in, 2 out */
static Uint32 fade_total, fade_left;

static Sint32 accl[SEG], accr[SEG];
static Sint16 modbuf[SEG * 2];
static int mikmod_ready;
static void (*music_finished)(void);

static void music_mix(int n)
{
	Mix_Music *m = mus;
	int i, gain, fade = 256;

	if (!m || !mus_playing || mus_paused || Player_Paused())
		return;
	if (!Player_Active()) {
		mus_playing = 0;
		if (music_finished)
			music_finished();
		return;
	}
	if (fade_mode && fade_total) {
		Uint32 done = fade_total - fade_left;
		fade = (int)(((fade_mode == 1 ? done : fade_left) * 256u) / fade_total);
		fade_left = fade_left > (Uint32)n ? fade_left - (Uint32)n : 0;
		if (fade_left == 0) {
			if (fade_mode == 2) {
				mus_playing = 0;
				fade_mode = 0;
				Player_Stop();
				return;
			}
			fade_mode = 0;
		}
	}
	gain = (mus_volume * fade) >> 1;	/* 0..16384 */
	VC_WriteBytes((SBYTE *)modbuf, (ULONG)n * 4);
	for (i = 0; i < n; i++) {
		accl[i] += ((Sint32)modbuf[2 * i] * gain) >> 14;
		accr[i] += ((Sint32)modbuf[2 * i + 1] * gain) >> 14;
	}
}

static void mix_callback(void *user, Uint8 *stream, int len)
{
	Sint16 *out = (Sint16 *)stream;
	int frames = len / (2 * outchans), i, c;
	(void)user;

	while (frames > 0) {
		int n = frames > SEG ? SEG : frames;
		memset(accl, 0, sizeof(Sint32) * n);
		memset(accr, 0, sizeof(Sint32) * n);
		music_mix(n);
		for (c = 0; c < nchans; c++) {
			struct channel *ch = &chans[c];
			const Sint16 *src;
			int left, todo, base, gl, gr, done = 0;
			if (!ch->chunk || ch->paused)
				continue;
			while (done < n && ch->chunk) {
				left = (int)((ch->chunk->alen - ch->pos) / 2);
				todo = left < n - done ? left : n - done;
				if (ch->fade_total && (Uint32)todo > ch->fade_left)
					todo = (int)ch->fade_left;
				src = (const Sint16 *)(ch->chunk->abuf + ch->pos);
				base = ch->volume * ch->chunk->volume;	/* 0..16384 */
				if (ch->fade_total)
					base = (int)((Sint32)base * (Sint32)(ch->fade_left >> 4) / (Sint32)((ch->fade_total >> 4) + 1));
				gl = (base * ch->left) >> 8;
				gr = (base * ch->right) >> 8;
				for (i = 0; i < todo; i++) {
					Sint32 s = src[i];
					accl[done + i] += (s * gl) >> 14;
					accr[done + i] += (s * gr) >> 14;
				}
				done += todo;
				ch->pos += (Uint32)todo * 2;
				if (ch->expire) {
					if (ch->expire <= (Uint32)todo) {
						ch->expire = 0;
						ch->chunk = NULL;
						break;
					}
					ch->expire -= (Uint32)todo;
				}
				if (ch->fade_total) {
					ch->fade_left -= (Uint32)todo;
					if (ch->fade_left == 0) {
						ch->chunk = NULL;
						break;
					}
				}
				if (ch->pos >= ch->chunk->alen) {
					if (ch->loops == 0) {
						ch->chunk = NULL;
						break;
					}
					if (ch->loops > 0)
						ch->loops--;
					ch->pos = 0;
				}
			}
		}
		if (outchans == 2) {
			for (i = 0; i < n; i++) {
				Sint32 l = accl[i], r = accr[i];
				*out++ = (Sint16)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
				*out++ = (Sint16)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
			}
		} else {
			for (i = 0; i < n; i++) {
				Sint32 v = (accl[i] + accr[i]) >> 1;
				*out++ = (Sint16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
			}
		}
		frames -= n;
	}
}

int Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize)
{
	SDL_AudioSpec want;
	int c;
	(void)frequency;
	(void)format;

	if (opened)
		return 0;
	memset(&want, 0, sizeof(want));
	want.freq = OUT_RATE;
	want.format = AUDIO_S16SYS;
	want.channels = (Uint8)(channels == 1 ? 1 : 2);
	want.samples = (Uint16)(chunksize >= 1024 ? 1024 : 512);
	want.callback = mix_callback;
	/* no "obtained": SDL converts to this format if the device differs */
	if (SDL_OpenAudio(&want, NULL) < 0)
		return -1;
	outchans = want.channels;
	if (!mikmod_ready) {
		MikMod_RegisterDriver(&drv_nos);
		MikMod_RegisterLoader(&load_xm);
		md_mixfreq = OUT_RATE;
		md_mode = DMODE_16BITS | DMODE_STEREO | DMODE_SOFT_MUSIC | DMODE_SOFT_SNDFX;
		md_device = 0;
		md_volume = 96;
		md_musicvolume = 128;
		md_pansep = 64;
		if (MikMod_Init(""))
			fprintf(stderr, "MikMod_Init: %s\n", MikMod_strerror(MikMod_errno));
		else
			mikmod_ready = 1;
	}
	for (c = 0; c < MAX_CHANS; c++) {
		chans[c].chunk = NULL;
		chans[c].volume = MIX_MAX_VOLUME;
		chans[c].left = chans[c].right = 255;
	}
	opened = 1;
	SDL_PauseAudio(0);
	return 0;
}

void Mix_CloseAudio(void)
{
	if (!opened)
		return;
	SDL_CloseAudio();
	opened = 0;
	mus = NULL;
	mus_playing = 0;
	if (mikmod_ready) {
		Player_Stop();
		MikMod_Exit();
		mikmod_ready = 0;
	}
}

int Mix_AllocateChannels(int numchans)
{
	if (numchans < 0)
		return nchans;
	SDL_LockAudio();
	nchans = numchans > MAX_CHANS ? MAX_CHANS : numchans;
	SDL_UnlockAudio();
	return nchans;
}

/* ---- WAV files ---- */

static Uint32 le32(const Uint8 *p) { return p[0] | (p[1] << 8) | ((Uint32)p[2] << 16) | ((Uint32)p[3] << 24); }
static Uint16 le16(const Uint8 *p) { return (Uint16)(p[0] | (p[1] << 8)); }

struct wav {
	int format, channels, rate, bits, blockalign, spb;
	const Uint8 *data;
	Uint32 datalen;
};

static int parse_wav(const Uint8 *d, Uint32 size, struct wav *w)
{
	const Uint8 *fmt = NULL;
	Uint32 fmtlen = 0, pos = 12;

	memset(w, 0, sizeof(*w));
	if (size < 12 || memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) {
		SDL_SetError("not a WAV file");
		return 0;
	}
	while (pos + 8 <= size) {
		Uint32 len = le32(d + pos + 4);
		if (len > size - pos - 8)
			len = size - pos - 8;	/* some files claim more than they have */
		if (!memcmp(d + pos, "fmt ", 4) && len >= 16) {
			fmt = d + pos + 8;
			fmtlen = len;
		} else if (!memcmp(d + pos, "data", 4)) {
			w->data = d + pos + 8;
			w->datalen = len;
		}
		pos += 8 + len + (len & 1);
	}
	if (!fmt || !w->data) {
		SDL_SetError("WAV without fmt or data");
		return 0;
	}
	w->format = le16(fmt);
	w->channels = le16(fmt + 2);
	w->rate = (int)le32(fmt + 4);
	w->blockalign = le16(fmt + 12);
	w->bits = le16(fmt + 14);
	if (w->format == 0x11 && fmtlen >= 20)
		w->spb = le16(fmt + 18);
	if (w->channels < 1 || w->channels > 2 || w->rate <= 0) {
		SDL_SetError("WAV: only mono or stereo");
		return 0;
	}
	return 1;
}

/* PCM (8-bit unsigned or 16-bit signed little-endian, mono or stereo, any
 * rate) to 16-bit native mono at the output rate (linear interpolation) */
static Sint16 *pcm_convert(const struct wav *w, Uint32 *outframes)
{
	int step, channels = w->channels, bits = w->bits;
	Uint32 frames, n, i;
	const Uint8 *data = w->data;
	Sint16 *out;

	if (w->format != 1 || (bits != 8 && bits != 16)) {
		SDL_SetError("WAV: only 8/16-bit PCM or IMA ADPCM");
		return NULL;
	}
	step = channels * (bits / 8);
	frames = w->datalen / (Uint32)step;
	if (frames == 0) {
		SDL_SetError("WAV: empty");
		return NULL;
	}
	n = (Uint32)(((unsigned long long)frames * OUT_RATE) / (Uint32)w->rate);
	if (n == 0)
		n = 1;
	out = (Sint16 *)malloc((size_t)n * 2);
	if (!out) {
		SDL_SetError("WAV: out of memory");
		return NULL;
	}
	for (i = 0; i < n; i++) {
		Uint32 s0, s1;
		int f, c;
		Sint32 a = 0, b = 0;
		if (w->rate == OUT_RATE) {
			s0 = s1 = i;
			f = 0;
		} else {
			unsigned long long sp = ((unsigned long long)i * (Uint32)w->rate << 16) / OUT_RATE;
			s0 = (Uint32)(sp >> 16);
			s1 = s0 + 1 < frames ? s0 + 1 : s0;
			f = (int)(sp & 0xFFFF);
		}
		for (c = 0; c < channels; c++) {
			if (bits == 8) {
				a += ((int)data[s0 * step + c] - 128) << 8;
				b += ((int)data[s1 * step + c] - 128) << 8;
			} else {
				a += (Sint16)le16(data + s0 * step + c * 2);
				b += (Sint16)le16(data + s1 * step + c * 2);
			}
		}
		a /= channels;
		b /= channels;
		out[i] = (Sint16)(a + (((b - a) * f) >> 16));
	}
	*outframes = n;
	return out;
}

static Uint8 *read_file(const char *file, Uint32 *size)
{
	FILE *f = fopen(file, "rb");
	long len;
	Uint8 *d = NULL;

	if (!f) {
		SDL_SetError("cannot open %s", file);
		return NULL;
	}
	if (fseek(f, 0, SEEK_END) == 0 && (len = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
		d = (Uint8 *)malloc((size_t)len);
		if (d && fread(d, 1, (size_t)len, f) != (size_t)len) {
			free(d);
			d = NULL;
		}
		*size = (Uint32)len;
	}
	fclose(f);
	if (!d)
		SDL_SetError("cannot read %s", file);
	return d;
}

Mix_Chunk *Mix_LoadWAV(const char *file)
{
	Uint32 size = 0, n = 0;
	Uint8 *d;
	struct wav w;
	Sint16 *pcm = NULL;
	Mix_Chunk *chunk;

	if (!opened || !file) {
		SDL_SetError("Mix_LoadWAV: audio not open");
		return NULL;
	}
	d = read_file(file, &size);
	if (!d)
		return NULL;
	if (parse_wav(d, size, &w))
		pcm = pcm_convert(&w, &n);
	free(d);
	if (!pcm)
		return NULL;
	chunk = (Mix_Chunk *)malloc(sizeof(*chunk));
	if (!chunk) {
		free(pcm);
		return NULL;
	}
	chunk->allocated = 1;
	chunk->abuf = (Uint8 *)pcm;
	chunk->alen = n * 2;
	chunk->volume = MIX_MAX_VOLUME;
	return chunk;
}

Mix_Chunk *Mix_LoadWAV_RW(SDL_RWops *src, int freesrc)
{
	Uint8 head[12], *d;
	Uint32 size, n = 0;
	struct wav w;
	Sint16 *pcm = NULL;
	Mix_Chunk *chunk;

	if (!opened || !src) {
		SDL_SetError("Mix_LoadWAV_RW: audio not open");
		return NULL;
	}
	if (SDL_RWread(src, head, 12, 1) < 1 || memcmp(head, "RIFF", 4) != 0) {
		SDL_SetError("Mix_LoadWAV_RW: not a RIFF file");
		if (freesrc)
			SDL_RWclose(src);
		return NULL;
	}
	size = le32(head + 4) + 8;
	if (size < 12 || size > 64u * 1024 * 1024 || !(d = (Uint8 *)malloc(size))) {
		SDL_SetError("Mix_LoadWAV_RW: bad size");
		if (freesrc)
			SDL_RWclose(src);
		return NULL;
	}
	memcpy(d, head, 12);
	if (size > 12 && SDL_RWread(src, d + 12, size - 12, 1) < 1) {
		free(d);
		SDL_SetError("Mix_LoadWAV_RW: short file");
		if (freesrc)
			SDL_RWclose(src);
		return NULL;
	}
	if (freesrc)
		SDL_RWclose(src);
	if (parse_wav(d, size, &w))
		pcm = pcm_convert(&w, &n);
	free(d);
	if (!pcm)
		return NULL;
	chunk = (Mix_Chunk *)malloc(sizeof(*chunk));
	if (!chunk) {
		free(pcm);
		return NULL;
	}
	chunk->allocated = 1;
	chunk->abuf = (Uint8 *)pcm;
	chunk->alen = n * 2;
	chunk->volume = MIX_MAX_VOLUME;
	return chunk;
}

void Mix_HookMusicFinished(void (*music_finished_fn)(void))
{
	SDL_LockAudio();
	music_finished = music_finished_fn;
	SDL_UnlockAudio();
}

Mix_Fading Mix_FadingMusic(void)
{
	if (!mus_playing)
		return MIX_NO_FADING;
	return fade_mode == 1 ? MIX_FADING_IN : fade_mode == 2 ? MIX_FADING_OUT : MIX_NO_FADING;
}

void Mix_FreeChunk(Mix_Chunk *chunk)
{
	int c;
	if (!chunk)
		return;
	if (opened) {
		SDL_LockAudio();
		for (c = 0; c < MAX_CHANS; c++)
			if (chans[c].chunk == chunk)
				chans[c].chunk = NULL;
		SDL_UnlockAudio();
	}
	free(chunk->abuf);
	free(chunk);
}

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
	int c, found = -1;

	if (!opened || !chunk || chunk->alen == 0)
		return -1;
	SDL_LockAudio();
	if (channel >= 0 && channel < nchans)
		found = channel;
	else
		for (c = reserved; c < nchans; c++)
			if (!chans[c].chunk) {
				found = c;
				break;
			}
	if (found >= 0) {
		chans[found].chunk = chunk;
		chans[found].pos = 0;
		chans[found].loops = loops;
		chans[found].paused = 0;
		chans[found].fade_total = chans[found].fade_left = 0;
		chans[found].expire = 0;
	}
	SDL_UnlockAudio();
	if (found < 0)
		SDL_SetError("Mix_PlayChannel: no free channel");
	return found;
}

int Mix_Playing(int channel)
{
	int c, n = 0;
	if (channel >= 0)
		return channel < MAX_CHANS && chans[channel].chunk != NULL;
	for (c = 0; c < MAX_CHANS; c++)
		n += chans[c].chunk != NULL;
	return n;
}

int Mix_Volume(int channel, int volume)
{
	int c, old;
	if (channel >= MAX_CHANS)
		return 0;
	old = chans[channel < 0 ? 0 : channel].volume;
	if (volume < 0)
		return old;
	if (volume > MIX_MAX_VOLUME)
		volume = MIX_MAX_VOLUME;
	SDL_LockAudio();
	if (channel < 0)
		for (c = 0; c < MAX_CHANS; c++)
			chans[c].volume = volume;
	else
		chans[channel].volume = volume;
	SDL_UnlockAudio();
	return old;
}

int Mix_HaltChannel(int channel)
{
	int c;
	if (!opened)
		return 0;
	SDL_LockAudio();
	for (c = 0; c < MAX_CHANS; c++)
		if (channel < 0 || c == channel)
			chans[c].chunk = NULL;
	SDL_UnlockAudio();
	return 0;
}

/* Gigalomania pauses the aircraft loops with the game and fades them out */
void Mix_Pause(int channel)
{
	int c;
	SDL_LockAudio();
	for (c = 0; c < MAX_CHANS; c++)
		if (channel < 0 || c == channel)
			chans[c].paused = 1;
	SDL_UnlockAudio();
}

void Mix_Resume(int channel)
{
	int c;
	SDL_LockAudio();
	for (c = 0; c < MAX_CHANS; c++)
		if (channel < 0 || c == channel)
			chans[c].paused = 0;
	SDL_UnlockAudio();
}

int Mix_Paused(int channel)
{
	if (channel < 0 || channel >= MAX_CHANS)
		return 0;
	return chans[channel].chunk != NULL && chans[channel].paused;
}

int Mix_FadeOutChannel(int channel, int ms)
{
	int c, n = 0;
	Uint32 total = (Uint32)(ms > 0 ? ms : 1) * (OUT_RATE / 10) / 100;
	if (!opened)
		return 0;
	SDL_LockAudio();
	for (c = 0; c < MAX_CHANS; c++)
		if ((channel < 0 || c == channel) && chans[c].chunk && !chans[c].fade_total) {
			chans[c].fade_total = chans[c].fade_left = total ? total : 1;
			n++;
		}
	SDL_UnlockAudio();
	return n;
}

int Mix_SetPanning(int channel, Uint8 left, Uint8 right)
{
	if (channel < 0 || channel >= MAX_CHANS)
		return 0;
	SDL_LockAudio();
	chans[channel].left = left;
	chans[channel].right = right;
	SDL_UnlockAudio();
	return 1;
}

int Mix_RegisterEffect(int chan, Mix_EffectFunc_t f, Mix_EffectDone_t d, void *arg)
{
	(void)chan;
	(void)f;
	(void)d;
	(void)arg;
	SDL_SetError("Mix_RegisterEffect: not in this mixer");
	return 0;
}

/* ---- music ---- */

Mix_Music *Mix_LoadMUS(const char *file)
{
	Mix_Music *m;
	MODULE *mod;

	if (!opened || !mikmod_ready || !file) {
		SDL_SetError("Mix_LoadMUS: audio not open");
		return NULL;
	}
	SDL_LockAudio();
	mod = Player_Load(file, 16, 0);
	SDL_UnlockAudio();
	if (!mod) {
		SDL_SetError("Mix_LoadMUS: %s: %s", file, MikMod_strerror(MikMod_errno));
		return NULL;
	}
	m = (Mix_Music *)calloc(1, sizeof(*m));
	if (!m) {
		Player_Free(mod);
		SDL_SetError("Mix_LoadMUS: out of memory");
		return NULL;
	}
	m->type = 0;
	m->module = mod;
	return m;
}

void Mix_FreeMusic(Mix_Music *m)
{
	if (!m)
		return;
	SDL_LockAudio();
	if (mus == m) {
		Player_Stop();
		mus = NULL;
		mus_playing = 0;
	}
	Player_Free(m->module);
	SDL_UnlockAudio();
	free(m);
}

int Mix_FadeInMusicPos(Mix_Music *m, int loops, int ms, double position)
{
	if (!opened || !m) {
		SDL_SetError("Mix_FadeInMusicPos: no music");
		return -1;
	}
	SDL_LockAudio();
	mus = m;
	m->module->wrap = loops != 0;	/* from the start again at the end */
	m->module->loop = 1;
	m->module->fadeout = 0;
	Player_Start(m->module);
	if (position > 0)
		Player_SetPosition((UWORD)position);
	mus_loops = loops;
	mus_playing = 1;
	mus_paused = 0;
	if (ms > 0) {
		fade_mode = 1;
		fade_total = fade_left = (Uint32)ms * OUT_RATE / 1000u;
	} else
		fade_mode = 0;
	SDL_UnlockAudio();
	return 0;
}

int Mix_SetMusicPosition(double position)
{
	if (!opened || !mus)
		return -1;
	SDL_LockAudio();
	Player_SetPosition((UWORD)position);
	SDL_UnlockAudio();
	return 0;
}

int Mix_FadeInMusic(Mix_Music *m, int loops, int ms)
{
	return Mix_FadeInMusicPos(m, loops, ms, 0.0);
}

int Mix_PlayMusic(Mix_Music *m, int loops)
{
	return Mix_FadeInMusicPos(m, loops, 0, 0.0);
}

int Mix_FadeOutMusic(int ms)
{
	int r = 0;
	if (!opened)
		return 0;
	SDL_LockAudio();
	if (mus_playing) {
		if (ms <= 0)
			mus_playing = 0;
		else {
			fade_mode = 2;
			fade_total = fade_left = (Uint32)ms * OUT_RATE / 1000u;
		}
		r = 1;
	}
	SDL_UnlockAudio();
	return r;
}

int Mix_HaltMusic(void)
{
	if (!opened)
		return 0;
	SDL_LockAudio();
	if (mus_playing)
		Player_Stop();
	mus_playing = 0;
	fade_mode = 0;
	SDL_UnlockAudio();
	return 0;
}

void Mix_PauseMusic(void)
{
	mus_paused = 1;
}

void Mix_ResumeMusic(void)
{
	mus_paused = 0;
}

int Mix_PausedMusic(void)
{
	return mus_paused;
}

int Mix_PlayingMusic(void)
{
	return mus_playing;
}

int Mix_VolumeMusic(int volume)
{
	int old = mus_volume;
	if (volume < 0)
		return old;
	if (volume > MIX_MAX_VOLUME)
		volume = MIX_MAX_VOLUME;
	mus_volume = volume;
	return old;
}

/* ---- the rest of the channel calls JLib makes (Holotz's Castle) ---- */

static Uint32 ms_to_samples(int ms)
{
	return (Uint32)(ms > 0 ? ms : 1) * (OUT_RATE / 10) / 100;
}

Mix_Chunk *Mix_GetChunk(int channel)
{
	if (channel < 0 || channel >= MAX_CHANS)
		return NULL;
	return chans[channel].chunk;
}

int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
	if (!opened)
		return 0;
	if (frequency)
		*frequency = OUT_RATE;
	if (format)
		*format = AUDIO_S16SYS;
	if (channels)
		*channels = outchans;
	return 1;
}

int Mix_ExpireChannel(int channel, int ticks)
{
	int c, n = 0;
	SDL_LockAudio();
	for (c = 0; c < MAX_CHANS; c++)
		if ((channel < 0 || c == channel) && chans[c].chunk) {
			chans[c].expire = ticks > 0 ? ms_to_samples(ticks) : 0;
			n++;
		}
	SDL_UnlockAudio();
	return n;
}

int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks)
{
	int c = Mix_PlayChannel(channel, chunk, loops);
	if (c >= 0 && ticks > 0) {
		SDL_LockAudio();
		chans[c].expire = ms_to_samples(ticks);
		SDL_UnlockAudio();
	}
	return c;
}

int Mix_FadeInChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ms, int ticks)
{
	(void)ms;	/* no fade in: it starts at full volume */
	return Mix_PlayChannelTimed(channel, chunk, loops, ticks);
}

int Mix_FadeInChannel(int channel, Mix_Chunk *chunk, int loops, int ms)
{
	return Mix_FadeInChannelTimed(channel, chunk, loops, ms, -1);
}

int Mix_VolumeChunk(Mix_Chunk *chunk, int volume)
{
	int old;
	if (!chunk)
		return -1;
	old = chunk->volume;
	if (volume >= 0)
		chunk->volume = (Uint8)(volume > MIX_MAX_VOLUME ? MIX_MAX_VOLUME : volume);
	return old;
}

const SDL_version *Mix_Linked_Version(void)
{
	static SDL_version v = { SDL_MIXER_MAJOR_VERSION, SDL_MIXER_MINOR_VERSION, SDL_MIXER_PATCHLEVEL };
	return &v;
}

int Mix_ReserveChannels(int num)
{
	if (num < 0)
		num = 0;
	if (num > nchans)
		num = nchans;
	reserved = num;
	return num;
}
