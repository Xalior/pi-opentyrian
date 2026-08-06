//
// sdl_audio_glue.cpp — a mono game on a stereo device.
//
// OpenTyrian is a MONO program. Everything it produces — the OPL music it
// synthesises in software, and the digitised sound effects it mixes on top —
// is one channel, and it asks SDL for a one-channel device. The Pi's HDMI
// sound device is two-channel and circle-libsdl2's SDL_OpenAudioDevice always
// hands back a stereo device, ignoring the channel count it was asked for and
// the caller's allowed-changes flags. Neither side is wrong and neither can
// give way: the hardware is stereo, and the game's whole audio path is
// written around one channel.
//
// Left alone this does not fail, which is what makes it worth writing down.
// The library would ask the game to fill a stereo buffer, the game would
// fill it with mono samples, and every sound would come out at twice its
// proper pitch with the two channels carrying alternate samples.
//
// So the game's callback is intercepted here. SDL_OpenAudioDevice is
// wrapped: a request for one channel is turned into a request for two, and
// the callback the library ends up with is the one below, which asks the
// game for half as many bytes and then writes each sample to both channels.
// The game is told it got the mono device it asked for.
//
// The sample-rate conversion OpenTyrian's sound effects need (8-bit signed
// mono at 11025 Hz to whatever the device runs at) is SDL_BuildAudioCVT and
// SDL_ConvertAudio, which the library now implements itself.
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

} // extern "C"
