/*
 * shim_audio.c - the picocalc-text-starter audio driver API (audio.h, by
 * Blair Leduc) re-implemented on SDL2's audio device for the Windows/Linux
 * desktop build (2026-09-23, Thomas: "Windows sounds"). The real audio.c
 * drives PWM/PIO hardware that has no desktop equivalent; this generates
 * the same stereo square waves in software instead, at the same
 * left/right frequencies platformer.c already asks for -- audio_play_sound()
 * plays a tone that continues until changed or stopped (used by the
 * tick-driven sfx_play_*()/sfx_update() state machine), audio_stop()
 * silences it. Frequency changes happen on the main thread but the tone is
 * generated on SDL's own audio callback thread, so every access to the
 * shared frequency/playing state is wrapped in SDL_LockAudioDevice().
 *
 * Author: Thomas Dzubin
 */
#include "shim.h"
#include "audio.h"

static SDL_AudioDeviceID dev;
static double  phase_l, phase_r;          /* 0..1, audio thread only      */
static volatile double freq_l, freq_r;    /* Hz, 0 = silent               */
static volatile bool   playing;

static void audio_callback(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    int16_t *buf = (int16_t *)stream;
    int frames = len / (int)sizeof(int16_t) / 2;   /* stereo */

    for (int i = 0; i < frames; i++) {
        int16_t l = 0, r = 0;

        if (playing) {
            if (freq_l > 0) {
                l = (int16_t)((phase_l < 0.5) ? SHIM_AUDIO_AMPLITUDE : -SHIM_AUDIO_AMPLITUDE);
                phase_l += freq_l / SHIM_AUDIO_RATE;
                if (phase_l >= 1.0) phase_l -= 1.0;
            }
            if (freq_r > 0) {
                r = (int16_t)((phase_r < 0.5) ? SHIM_AUDIO_AMPLITUDE : -SHIM_AUDIO_AMPLITUDE);
                phase_r += freq_r / SHIM_AUDIO_RATE;
                if (phase_r >= 1.0) phase_r -= 1.0;
            }
        }
        buf[i * 2]     = l;
        buf[i * 2 + 1] = r;
    }
}

void audio_init(void)
{
    if (dev)
        return;
    shim_init();   /* SDL_Init(), incl. SDL_INIT_AUDIO -- see shim_core.c */

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq     = SHIM_AUDIO_RATE;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = SHIM_AUDIO_SAMPLES;
    want.callback = audio_callback;

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        fprintf(stderr, "SDL audio device open failed: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(dev, 0);
}

/* Play a stereo sound asynchronously (continues until changed/stopped) --
 * same non-blocking contract as the real audio_play_sound().             */
void audio_play_sound(uint32_t left_frequency, uint32_t right_frequency)
{
    if (!dev)
        return;
    SDL_LockAudioDevice(dev);
    freq_l = left_frequency;
    freq_r = right_frequency;
    playing = true;
    SDL_UnlockAudioDevice(dev);
}

void audio_stop(void)
{
    if (!dev)
        return;
    SDL_LockAudioDevice(dev);
    playing = false;
    SDL_UnlockAudioDevice(dev);
}

bool audio_is_playing(void) { return playing; }

/* Blocking variants, matching the real audio.c's own behaviour. During
 * gameplay platformer.c never uses these (it goes through the
 * non-blocking audio_play_sound() plus its own tick-driven sfx_update()
 * state machine, so physics is never frozen for a sound's duration -- see
 * platformer_config.h's Sound effects comment); the win screen does, for
 * its countdown beeps and win song, where there is no physics to freeze. */
void audio_play_sound_blocking(uint32_t left_frequency, uint32_t right_frequency, uint32_t duration_ms)
{
    audio_play_sound(left_frequency, right_frequency);
    sleep_ms(duration_ms);
    audio_stop();
}

void audio_play_note_blocking(const audio_note_t *note)
{
    if (!note)
        return;
    audio_play_sound_blocking(note->left_frequency, note->right_frequency, note->duration_ms);
}

/* real audio.c reads this as a sentinel: a 0 duration_ms marks the end of
 * a song's note array (see its own audio_play_song_blocking()).          */
void audio_play_song_blocking(const audio_song_t *song)
{
    if (!song)
        return;
    for (int i = 0; song->notes[i].duration_ms != 0; i++) {
        audio_play_sound_blocking(song->notes[i].left_frequency,
                                   song->notes[i].right_frequency,
                                   song->notes[i].duration_ms);
        if (song->notes[i].left_frequency != SILENCE || song->notes[i].right_frequency != SILENCE)
            sleep_ms(20);   /* small gap between notes, same as the real driver */
    }
    audio_stop();
}
