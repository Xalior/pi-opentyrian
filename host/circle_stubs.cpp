//
// circle_stubs.cpp — the SDL2 entry points OpenTyrian references that
// circle-libsdl2 does not provide.
//
// Three kinds of thing live here, and the difference matters.
//
// WINDOW GEOMETRY. There is one screen and it is always full. A window here
// is the virtual display the host kernel declared before the game started,
// and its size is a fact about the boot, not something an application may
// change. So the calls that would move or resize a window accept what they
// are given and report success: the game asks, nothing happens, and the game
// carries on with the size SDL_GetWindowSize keeps telling it. OpenTyrian
// draws into a texture and scales it to whatever the window says it is, so
// this is the whole of what it needs.
//
// KEY NAMES. OpenTyrian writes its key bindings to its configuration file by
// name and reads them back the same way, so the names have to be SDL2's own.
// The table below is that list. This is not a stand-in for anything the
// library is expected to grow: naming a key is a keyboard-layout question,
// and the library deals in scancodes.
//
// STRING HELPERS. SDL_strlcpy is SDL's own bounded string copy, used by the
// game in a handful of places. It is a few lines and it belongs to whoever
// links it.
//
// Nothing here pretends to work when it does not. Anything that genuinely
// cannot be done on this platform is absent rather than faked, so it shows up
// as a link error while it is still cheap to deal with.
//
#include <SDL2/SDL.h>

#include <cstring>

namespace
{

// SDL2's scancode names, indexed by scancode. The gaps are keys with no
// name, which SDL2 reports as an empty string rather than an error.
//
// The list stops after the modifier block at 224, which is the last scancode
// a keyboard produces. Anything beyond it — the media and application keys,
// the international and locale keys — reads as unnamed, which is what a
// binding for a key this platform has no driver for should look like.
const char *const ScancodeNames[] = {
    /*   0 */ "", "", "", "",
    /*   4 */ "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M",
    /*  17 */ "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
    /*  30 */ "1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
    /*  40 */ "Return", "Escape", "Backspace", "Tab", "Space",
    /*  45 */ "-", "=", "[", "]", "\\", "#", ";", "'", "`", ",", ".", "/",
    /*  57 */ "CapsLock",
    /*  58 */ "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
    /*  68 */ "F11", "F12",
    /*  70 */ "PrintScreen", "ScrollLock", "Pause", "Insert", "Home",
    /*  75 */ "PageUp", "Delete", "End", "PageDown",
    /*  79 */ "Right", "Left", "Down", "Up",
    /*  83 */ "Numlock",
    /*  84 */ "Keypad /", "Keypad *", "Keypad -", "Keypad +", "Keypad Enter",
    /*  89 */ "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4", "Keypad 5",
    /*  94 */ "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9", "Keypad 0",
    /*  99 */ "Keypad .",
    /* 100 */ "", "Application", "Power", "Keypad =",
    /* 104 */ "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21",
    /* 113 */ "F22", "F23", "F24",
    /* 116 */ "Execute", "Help", "Menu", "Select", "Stop", "Again", "Undo",
    /* 123 */ "Cut", "Copy", "Paste", "Find", "Mute", "VolumeUp",
    /* 129 */ "VolumeDown",
};

const int ScancodeNamesCount =
    (int)(sizeof(ScancodeNames) / sizeof(ScancodeNames[0]));

// The modifier keys sit at the top of the scancode range, far past the block
// above, so they are named separately rather than padding the table out with
// ninety empty strings.
const char *const ModifierNames[] = {
    /* 224 */ "Left Ctrl", "Left Shift", "Left Alt", "Left GUI",
    /* 228 */ "Right Ctrl", "Right Shift", "Right Alt", "Right GUI",
};

const int MODIFIER_FIRST = SDL_SCANCODE_LCTRL;   // 224
const int MODIFIER_COUNT =
    (int)(sizeof(ModifierNames) / sizeof(ModifierNames[0]));

bool SameNameIgnoringCase(const char *a, const char *b)
{
    for (;; a++, b++)
    {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return false;
        if (ca == '\0')
            return true;
    }
}

} // namespace

extern "C" {

// ---- window geometry --------------------------------------------------------

void SDL_SetWindowSize(SDL_Window *, int, int) {}

void SDL_SetWindowPosition(SDL_Window *, int, int) {}

// There is nothing but full screen, so every request for it is already
// satisfied and a request to leave it changes nothing either.
int SDL_SetWindowFullscreen(SDL_Window *, Uint32) { return 0; }

// ---- key names --------------------------------------------------------------

const char *SDL_GetScancodeName(SDL_Scancode scancode)
{
    const int code = (int)scancode;
    if (code >= 0 && code < ScancodeNamesCount)
        return ScancodeNames[code];
    if (code >= MODIFIER_FIRST && code < MODIFIER_FIRST + MODIFIER_COUNT)
        return ModifierNames[code - MODIFIER_FIRST];
    return "";
}

SDL_Scancode SDL_GetScancodeFromName(const char *name)
{
    if (name == nullptr || *name == '\0')
    {
        SDL_SetError("no key name given");
        return SDL_SCANCODE_UNKNOWN;
    }

    for (int i = 0; i < ScancodeNamesCount; i++)
        if (ScancodeNames[i][0] != '\0'
            && SameNameIgnoringCase(name, ScancodeNames[i]))
            return (SDL_Scancode)i;

    for (int i = 0; i < MODIFIER_COUNT; i++)
        if (SameNameIgnoringCase(name, ModifierNames[i]))
            return (SDL_Scancode)(MODIFIER_FIRST + i);

    SDL_SetError("unknown key name");
    return SDL_SCANCODE_UNKNOWN;
}

// ---- string helpers ---------------------------------------------------------

// Copy at most maxlen-1 characters and always terminate. The return value is
// the length the source would have needed, so a caller can tell whether the
// copy was truncated.
size_t SDL_strlcpy(char *dst, const char *src, size_t maxlen)
{
    const size_t srclen = strlen(src);
    if (maxlen > 0)
    {
        const size_t len = srclen < maxlen - 1 ? srclen : maxlen - 1;
        memcpy(dst, src, len);
        dst[len] = '\0';
    }
    return srclen;
}

} // extern "C"
