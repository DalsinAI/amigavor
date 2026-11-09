VoR 0.5.8 for AmigaOS 3.x
=========================

VoR, Variations on Rockdodger (Jason Woofenden, Joshua Grams, after Paul
Holt's Rockdodger): steer your ship through a stream of tumbling rocks for
as long as you can, while the blast wave behind you pushes the field along;
easy and normal games, a high score table for each. For AmigaOS 3.x on a
68040 or 68060 with an FPU and an RTG screen, on OpenGPU's SDL 1.2
(sdl12-compat).

Base: vor_0.5.8.orig.tar.gz from Debian (sha256
9003089708327e1a3169b412e16725ccb669dd21465d49abec5aa4e6e4cce09a); Debian
has no patches for it. Branch team2/vor-amiga.

Status: plays in the lab (Instance-45 on GFOpenUp 0.8.19, 6 November
2026): the title over its drifting rocks, Space and a normal game (the
ship thrusting and turning, the rocks streaming past, the time), game over
after 13 seconds, and Escape back to the Shell. Screenshots from that run
are in amiga/screenshots (vor-title.png, vor-game.png, vor-game-over.png).
The lab is muted, so the music and sound were not heard there.


What you need
-------------

- A 68040 or 68060 with an FPU, AmigaOS 3.x, an RTG card (or the
  AmigaChrome lab).
- GFOpenUp 0.8.18 or later (LIBS:OpenGPU/SDL2.module). Everything else is
  in the program.


How to play
-----------

Open the VoR icon, or run VoR from a Shell. It opens a 640x480 window
(VoR -f for the full screen). Space starts a normal game, 1 an easy one.
The cursor keys steer: tap them, and remember to slow down. P pauses, F
switches to the full screen, 3 saves a screenshot, Escape quits.
ReadMe-upstream in the drawer is upstream's README. The pictures, sound and
music are in the drawer's data/; the high scores are kept in
user/.vor-scores.


Building
--------

    make -f amiga/Makefile.amigaos -j2 dist

with GFOpenAmigaGCC 16.2.0-open4, the OpenNDK headers, the OpenGPU kit and
the Team's static libpng and zlib (see the Makefile's head). The drawer is
dist/VoR (and dist/VoR.info).


What was changed
----------------

- main.c: the frame counter's "start" is vor_start on AmigaOS (libnix's
  startup code has a "start" of its own, and the program would not link).
- amiga/config.h stands in for configure's (libnix's nanosleep declared).
- $HOME is the drawer's user/ (amiga/amiga_env.c, -Wl,--wrap=getenv), for
  the high scores; the data is PROGDIR:data.
- amiga/Makefile.amigaos builds Makefile.in's vor, with the prebuilt
  data/font.png.
- No SDL_image or SDL_mixer 1.2 for OpenGPU's SDL 1.2: the Team's
  amiga/sdl_image_lite.c (PNG on libpng) and amiga/sdl_mixer_lite.c (WAV,
  with libmikmod 3.3.16 for the music: amiga/libmikmod, its player, XM
  loader and no-sound driver, at most 16 voices), from the Ceferino port.
- amiga/stack.c: 256 KB of stack.


Licences
--------

VoR and its pictures: GPL 2 or later (COPYING.txt); its sound and music
("VoR Theme", data/mph.xm): public domain, as upstream's README says.
mt.c: BSD-3-clause (Makoto Matsumoto, Takuji Nishimura). libmikmod: LGPL 2
or later (COPYING-libmikmod.txt). The Amiga files (amiga/): the AmiCradle
Team, GPL 2 or later, except the mixer, which comes from the Team's Wizznic
port and is GPL 3 or later, so the program as built here is GPL 3 or
later; the icons are our own GlowIcons (amiga/icons/art/vor.svg).
