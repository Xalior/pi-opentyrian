//
// sdl_audio_glue.cpp — a mono game on a stereo device, and the sample-rate
// conversion its sound effects need.
//
// TWO PROBLEMS, BOTH AT THE EDGE BETWEEN THE GAME AND THE LIBRARY.
//
// 1. OpenTyrian is a MONO program. Everything it produces — the OPL music it
//    synthesises in software, and the digitised sound effects it mixes on top
//    — is one channel, and it asks SDL for a one-channel device. The Pi's
//    HDMI sound device is two-channel and circle-libsdl2 offers nothing else,
//    so the library hands back a stereo device. Neither side is wrong and
//    neither can give way: the hardware is stereo, and the game's whole audio
//    path is written around one channel.
//
//    Left alone this does not fail, which is what makes it worth writing
//    down. The library would ask the game to fill a stereo buffer, the game
//    would fill it with mono samples, and every sound would come out at twice
//    its proper pitch with the two channels carrying alternate samples.
//
//    So the game's callback is intercepted here. SDL_OpenAudioDevice is
//    wrapped: a request for one channel is turned into a request for two, and
//    the callback the library ends up with is the one below, which asks the
//    game for half as many bytes and then writes each sample to both
//    channels. The game is told it got the mono device it asked for.
//
// 2. OpenTyrian's sound effects are stored as 8-bit samples at 11025 Hz and
//    have to be converted once, at load time, to the format and rate the
//    device is actually running at. It does that with SDL_BuildAudioCVT and
//    SDL_ConvertAudio, which the library does not implement — it has no need
//    of them, because its own audio path is a single format end to end.
//    They are implemented here for the one conversion the game asks for:
//    8-bit signed mono to 16-bit signed mono, upward in rate.
//
#include <SDL2/SDL.h>

#include <cstdlib>
#include <cstring>

extern "C" {

SDL_AudioDeviceID __real_SDL_OpenAudioDevice(const char *device, int iscapture,
                                             const SDL_AudioSpec *desired,
                                             SDL_AudioSpec *obtained,
                                             int allowed_changes);

} // extern "C"

namespace
{

// The game's own callback and its argument, held while the device is open.
SDL_AudioCallback s_monoCallback = nullptr;
void             *s_monoUserdata = nullptr;

// Scratch for one callback's worth of mono samples. Allocated when the
// device is opened, at the size the device will ask for.
Uint8            *s_monoBuffer = nullptr;
int               s_monoBytes = 0;

// Called by the library with a stereo buffer. Fill half of it from the game,
// then spread those samples across both channels, working backwards so the
// expansion never overwrites a sample it has not read yet.
void StereoFromMono(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;

    const int monoBytes = len / 2;
    if (s_monoCallback == nullptr || s_monoBuffer == nullptr
        || monoBytes > s_monoBytes)
    {
        memset(stream, 0, (size_t)len);
        return;
    }

    s_monoCallback(s_monoUserdata, s_monoBuffer, monoBytes);

    const Sint16 *src = (const Sint16 *)s_monoBuffer;
    Sint16 *dst = (Sint16 *)stream;
    for (int i = monoBytes / 2 - 1; i >= 0; i--)
    {
        dst[i * 2]     = src[i];
        dst[i * 2 + 1] = src[i];
    }
}

// How much bigger a converted buffer gets, rounded up. SDL's contract is
// that the caller allocates len * len_mult bytes and the conversion happens
// in place, so this must never be an underestimate.
int LengthMultiplier(double ratio)
{
    int mult = 1;
    while (mult < ratio)
        mult *= 2;
    return mult;
}

} // namespace

extern "C" {

// ---- the mono game on the stereo device -------------------------------------

SDL_AudioDeviceID __wrap_SDL_OpenAudioDevice(const char *device, int iscapture,
                                             const SDL_AudioSpec *desired,
                                             SDL_AudioSpec *obtained,
                                             int allowed_changes)
{
    if (desired == nullptr || iscapture || desired->channels != 1)
        return __real_SDL_OpenAudioDevice(device, iscapture, desired, obtained,
                                          allowed_changes);

    SDL_AudioSpec ask = *desired;
    ask.channels = 2;
    ask.callback = StereoFromMono;
    ask.userdata = nullptr;

    SDL_AudioSpec got;
    memset(&got, 0, sizeof(got));

    SDL_AudioDeviceID id = __real_SDL_OpenAudioDevice(device, iscapture, &ask,
                                                      &got, allowed_changes);
    if (id == 0)
        return 0;

    s_monoCallback = desired->callback;
    s_monoUserdata = desired->userdata;

    // The device's buffer is stereo; the game's half of it is mono.
    s_monoBytes = (int)got.size / 2;
    free(s_monoBuffer);
    s_monoBuffer = (Uint8 *)calloc(1, (size_t)(s_monoBytes > 0 ? s_monoBytes : 1));
    if (s_monoBuffer == nullptr)
    {
        SDL_CloseAudioDevice(id);
        SDL_SetError("out of memory allocating the mono mixing buffer");
        return 0;
    }

    // Report the mono device the caller asked for. The rate is whatever the
    // hardware settled on, and the caller has to read it back — OpenTyrian
    // does, and paces its OPL synthesis by it.
    if (obtained != nullptr)
    {
        *obtained = got;
        obtained->channels = 1;
        obtained->size = (Uint32)s_monoBytes;
        obtained->callback = desired->callback;
        obtained->userdata = desired->userdata;
    }

    return id;
}

// ---- sample conversion ------------------------------------------------------

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, SDL_AudioFormat src_format,
                      Uint8 src_channels, int src_rate,
                      SDL_AudioFormat dst_format, Uint8 dst_channels,
                      int dst_rate)
{
    if (cvt == nullptr || src_rate <= 0 || dst_rate <= 0)
    {
        SDL_SetError("SDL_BuildAudioCVT: invalid arguments");
        return -1;
    }
    if (src_channels != 1 || dst_channels != 1)
    {
        SDL_SetError("SDL_BuildAudioCVT: only mono conversion is available");
        return -1;
    }
    if (src_format != AUDIO_S8 || dst_format != AUDIO_S16SYS)
    {
        SDL_SetError("SDL_BuildAudioCVT: only 8-bit signed to 16-bit signed "
                     "conversion is available");
        return -1;
    }

    memset(cvt, 0, sizeof(*cvt));
    cvt->src_format = src_format;
    cvt->dst_format = dst_format;
    cvt->rate_incr = (double)dst_rate / (double)src_rate;
    // One byte per sample becomes two, and the rate change stretches it
    // further.
    cvt->len_ratio = 2.0 * cvt->rate_incr;
    cvt->len_mult = LengthMultiplier(cvt->len_ratio);
    cvt->needed = 1;
    return 1;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    if (cvt == nullptr || cvt->buf == nullptr || !cvt->needed)
    {
        SDL_SetError("SDL_ConvertAudio: nothing to convert");
        return -1;
    }
    if (cvt->len < 0)
    {
        SDL_SetError("SDL_ConvertAudio: negative length");
        return -1;
    }

    const int srcCount = cvt->len;                    // one byte per sample
    const int dstCount = (int)(srcCount * cvt->rate_incr);
    cvt->len_cvt = dstCount * (int)sizeof(Sint16);

    if (srcCount == 0 || dstCount == 0)
    {
        cvt->len_cvt = 0;
        return 0;
    }

    // In place, back to front: the destination is at least twice the size of
    // the source and shares its buffer, so writing forwards would overwrite
    // source samples still to be read.
    const Sint8 *src = (const Sint8 *)cvt->buf;
    Sint16 *dst = (Sint16 *)cvt->buf;

    for (int i = dstCount - 1; i >= 0; i--)
    {
        // Nearest source sample. Point sampling rather than interpolation:
        // these are 8-bit effects that were point-sampled in the original
        // game, and anything smoother would not sound more faithful.
        int j = (int)(i / cvt->rate_incr);
        if (j >= srcCount)
            j = srcCount - 1;
        dst[i] = (Sint16)(src[j] * 256);
    }

    return 0;
}

} // extern "C"
