//
// sdl_indexed.cpp — indexed-colour surfaces and pixel formats.
//
// WHY THIS EXISTS. Tyrian is a 256-colour VGA game, and OpenTyrian keeps
// that shape: it draws every frame into a 320x200 surface of 8-bit palette
// indices, then a scaler in the game reads that surface and writes 32-bit
// pixels into a streaming texture. circle-libsdl2 implements surfaces at 32
// bits per pixel only, because for its own renderer a surface is nothing but
// a staging buffer for a texture upload — nothing in the library ever reads
// an 8-bit one.
//
// So the 8-bit surface is supplied here instead. It is the same kind of
// object: memory the game writes into, with a width, a height, a pitch and a
// format record saying what a pixel means. Nothing in the library is asked to
// understand it; the only code that reads these pixels is OpenTyrian's own
// scaler, which turns them into the 32-bit texture the library does
// understand.
//
// HOW IT IS PUT UNDERNEATH THE GAME. The linker's --wrap (see the Makefile),
// exactly as the file syscalls are redirected. SDL_CreateRGBSurface and
// SDL_FreeSurface exist in the library, so redefining them here would be a
// duplicate symbol. --wrap renames the references instead: a request for an
// 8-bit surface is served here, and a request for anything else is handed
// straight to the library through __real_. Nothing in either submodule is
// edited, and the library keeps its own behaviour for its own callers.
//
// A surface made here is recognised on the way back out by its format id
// being SDL_PIXELFORMAT_INDEX8, which no library surface ever carries, so the
// two kinds of surface can be freed through one entry point without a
// registry to keep in step.
//
// The rest of the file is the pixel-format machinery OpenTyrian uses and the
// library has no need of: naming a format, allocating a format record,
// packing a colour into one. SDL_CreateTexture is wrapped for a related
// reason — the game asks for SDL_PIXELFORMAT_RGB888 and the library offers
// SDL_PIXELFORMAT_ARGB8888, which is the same four bytes in the same order
// with the top one unused.
//
#include <SDL2/SDL.h>

#include <cstdlib>
#include <cstring>

extern "C" {

SDL_Surface *__real_SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                         int depth, Uint32 Rmask, Uint32 Gmask,
                                         Uint32 Bmask, Uint32 Amask);
void __real_SDL_FreeSurface(SDL_Surface *surface);
SDL_Texture *__real_SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format,
                                      int access, int w, int h);

} // extern "C"

namespace
{

// The 32-bit format the library's surfaces and textures use: red at bit 16,
// green at bit 8, blue at bit 0, top byte unused. SDL2 calls this
// SDL_PIXELFORMAT_RGB888, and it is byte-for-byte what an ARGB8888 texture
// expects, which is why the game can stage into one and upload the other.
SDL_PixelFormat s_xrgb8888 = {
    SDL_PIXELFORMAT_RGB888,
    nullptr,                  // no palette
    32,                       // BitsPerPixel
    4,                        // BytesPerPixel
    { 0, 0 },                 // padding
    0x00FF0000,               // Rmask
    0x0000FF00,               // Gmask
    0x000000FF,               // Bmask
    0x00000000,               // Amask
    0, 0, 0, 8,               // Rloss, Gloss, Bloss, Aloss
    16, 8, 0, 0,              // Rshift, Gshift, Bshift, Ashift
    1,                        // refcount
    nullptr                   // next
};

// The same four bytes, named with an alpha channel. Handed back when a
// caller asks for SDL_PIXELFORMAT_ARGB8888 so both spellings work.
SDL_PixelFormat s_argb8888 = {
    SDL_PIXELFORMAT_ARGB8888,
    nullptr,
    32,
    4,
    { 0, 0 },
    0x00FF0000,
    0x0000FF00,
    0x000000FF,
    0xFF000000,
    0, 0, 0, 0,
    16, 8, 0, 24,
    1,
    nullptr
};

bool IsIndexed(const SDL_Surface *s)
{
    return s != nullptr && s->format != nullptr
        && s->format->format == SDL_PIXELFORMAT_INDEX8;
}

// Every 8-bit surface owns its format record and its palette, because a
// palette belongs to one surface. They are allocated together with the
// surface and released together with it.
struct IndexedSurface
{
    SDL_Surface     surface;
    SDL_PixelFormat format;
    SDL_Palette     palette;
    SDL_Color       colors[256];
};

} // namespace

extern "C" {

// ---- surfaces ---------------------------------------------------------------

SDL_Surface *__wrap_SDL_CreateRGBSurface(Uint32 flags, int width, int height,
                                         int depth, Uint32 Rmask, Uint32 Gmask,
                                         Uint32 Bmask, Uint32 Amask)
{
    if (depth != 8)
        return __real_SDL_CreateRGBSurface(flags, width, height, depth,
                                           Rmask, Gmask, Bmask, Amask);

    if (width <= 0 || height <= 0)
    {
        SDL_SetError("surface dimensions must be positive");
        return nullptr;
    }

    IndexedSurface *is = (IndexedSurface *)calloc(1, sizeof(IndexedSurface));
    if (is == nullptr)
    {
        SDL_SetError("out of memory allocating surface");
        return nullptr;
    }

    // One byte per pixel, rows packed with no padding. OpenTyrian reads the
    // pitch rather than assuming, but its scalers are written for exactly
    // this layout and a padded row would be a silent skew.
    const int pitch = width;
    is->surface.pixels = calloc(1, (size_t)pitch * height);
    if (is->surface.pixels == nullptr)
    {
        free(is);
        SDL_SetError("out of memory allocating surface pixels");
        return nullptr;
    }

    is->palette.ncolors = 256;
    is->palette.colors = is->colors;
    is->palette.version = 1;
    is->palette.refcount = 1;

    is->format.format = SDL_PIXELFORMAT_INDEX8;
    is->format.palette = &is->palette;
    is->format.BitsPerPixel = 8;
    is->format.BytesPerPixel = 1;
    is->format.refcount = 1;

    is->surface.format = &is->format;
    is->surface.w = width;
    is->surface.h = height;
    is->surface.pitch = pitch;
    is->surface.clip_rect = { 0, 0, width, height };
    is->surface.refcount = 1;

    // flags stays zero, so SDL_MUSTLOCK is false and the game may write
    // straight into pixels — which it asserts on at start-up and then does.
    return &is->surface;
}

void __wrap_SDL_FreeSurface(SDL_Surface *surface)
{
    if (surface == nullptr)
        return;
    if (!IsIndexed(surface))
    {
        __real_SDL_FreeSurface(surface);
        return;
    }

    if (--surface->refcount > 0)
        return;

    // The surface is the first member of its allocation, so the surface
    // pointer is the allocation pointer, and the format and palette go with
    // it.
    IndexedSurface *is = (IndexedSurface *)surface;
    free(is->surface.pixels);
    free(is);
}

// Clear a rectangle to one colour. The colour is a pixel value in the
// surface's own format: a palette index on an 8-bit surface, a packed
// 32-bit pixel on the library's.
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color)
{
    if (dst == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("SDL_FillRect: no destination surface");
        return -1;
    }

    SDL_Rect r = rect != nullptr ? *rect : dst->clip_rect;

    // Clip to the surface. A rectangle entirely outside it is not an error;
    // it simply fills nothing.
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > dst->w) r.w = dst->w - r.x;
    if (r.y + r.h > dst->h) r.h = dst->h - r.y;
    if (r.w <= 0 || r.h <= 0)
        return 0;

    Uint8 *row = (Uint8 *)dst->pixels + (size_t)r.y * dst->pitch;

    if (dst->format->BytesPerPixel == 1)
    {
        row += r.x;
        for (int y = 0; y < r.h; y++, row += dst->pitch)
            memset(row, (int)(color & 0xFF), (size_t)r.w);
        return 0;
    }

    if (dst->format->BytesPerPixel == 4)
    {
        row += (size_t)r.x * 4;
        for (int y = 0; y < r.h; y++, row += dst->pitch)
        {
            Uint32 *p = (Uint32 *)row;
            for (int x = 0; x < r.w; x++)
                p[x] = color;
        }
        return 0;
    }

    SDL_SetError("SDL_FillRect: unsupported pixel size");
    return -1;
}

// Copy a rectangle from one surface to another of the same pixel size. No
// conversion, no colour keying, no alpha blending: OpenTyrian's only use is
// duplicating one 320x200 indexed screen into another.
//
// SDL_BlitSurface is a macro for this name.
int SDL_UpperBlit(SDL_Surface *src, const SDL_Rect *srcrect,
                  SDL_Surface *dst, SDL_Rect *dstrect)
{
    if (src == nullptr || dst == nullptr
        || src->pixels == nullptr || dst->pixels == nullptr)
    {
        SDL_SetError("SDL_BlitSurface: missing surface");
        return -1;
    }
    if (src->format->BytesPerPixel != dst->format->BytesPerPixel)
    {
        SDL_SetError("SDL_BlitSurface: pixel formats differ and conversion "
                     "is not available");
        return -1;
    }

    SDL_Rect s = srcrect != nullptr ? *srcrect
                                    : SDL_Rect{ 0, 0, src->w, src->h };
    int dx = dstrect != nullptr ? dstrect->x : 0;
    int dy = dstrect != nullptr ? dstrect->y : 0;

    // Clip against both surfaces, moving the source origin with the
    // destination so the two stay aligned.
    if (s.x < 0) { s.w += s.x; dx -= s.x; s.x = 0; }
    if (s.y < 0) { s.h += s.y; dy -= s.y; s.y = 0; }
    if (dx < 0)  { s.w += dx;  s.x -= dx; dx = 0; }
    if (dy < 0)  { s.h += dy;  s.y -= dy; dy = 0; }
    if (s.x + s.w > src->w) s.w = src->w - s.x;
    if (s.y + s.h > src->h) s.h = src->h - s.y;
    if (dx + s.w > dst->w)  s.w = dst->w - dx;
    if (dy + s.h > dst->h)  s.h = dst->h - dy;

    if (dstrect != nullptr)
    {
        dstrect->x = dx;
        dstrect->y = dy;
        dstrect->w = s.w > 0 ? s.w : 0;
        dstrect->h = s.h > 0 ? s.h : 0;
    }
    if (s.w <= 0 || s.h <= 0)
        return 0;

    const int bpp = src->format->BytesPerPixel;
    const Uint8 *sp = (const Uint8 *)src->pixels
                    + (size_t)s.y * src->pitch + (size_t)s.x * bpp;
    Uint8 *dp = (Uint8 *)dst->pixels
              + (size_t)dy * dst->pitch + (size_t)dx * bpp;

    for (int y = 0; y < s.h; y++, sp += src->pitch, dp += dst->pitch)
        memcpy(dp, sp, (size_t)s.w * bpp);

    return 0;
}

// ---- pixel formats ----------------------------------------------------------

SDL_PixelFormat *SDL_AllocFormat(Uint32 pixel_format)
{
    switch (pixel_format)
    {
    case SDL_PIXELFORMAT_RGB888:
        return &s_xrgb8888;
    case SDL_PIXELFORMAT_ARGB8888:
        return &s_argb8888;
    default:
        SDL_SetError("only 32-bit pixel formats are available");
        return nullptr;
    }
}

// The two records above are static and shared, so there is nothing to give
// back. A format belonging to an 8-bit surface is released with its surface
// and must not be released here either.
void SDL_FreeFormat(SDL_PixelFormat *format) { (void)format; }

const char *SDL_GetPixelFormatName(Uint32 format)
{
    switch (format)
    {
    case SDL_PIXELFORMAT_INDEX8:   return "SDL_PIXELFORMAT_INDEX8";
    case SDL_PIXELFORMAT_RGB565:   return "SDL_PIXELFORMAT_RGB565";
    case SDL_PIXELFORMAT_RGB888:   return "SDL_PIXELFORMAT_RGB888";
    case SDL_PIXELFORMAT_ARGB8888: return "SDL_PIXELFORMAT_ARGB8888";
    case SDL_PIXELFORMAT_UNKNOWN:  return "SDL_PIXELFORMAT_UNKNOWN";
    default:                       return "SDL_PIXELFORMAT_UNKNOWN";
    }
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b)
{
    if (format == nullptr || format->BytesPerPixel != 4)
        return 0;
    return ((Uint32)r << format->Rshift)
         | ((Uint32)g << format->Gshift)
         | ((Uint32)b << format->Bshift)
         | format->Amask;
}

Uint32 SDL_MapRGBA(const SDL_PixelFormat *format, Uint8 r, Uint8 g, Uint8 b,
                   Uint8 a)
{
    if (format == nullptr || format->BytesPerPixel != 4)
        return 0;
    return ((Uint32)r << format->Rshift)
         | ((Uint32)g << format->Gshift)
         | ((Uint32)b << format->Bshift)
         | (((Uint32)a << format->Ashift) & format->Amask);
}

// ---- textures ---------------------------------------------------------------

// SDL_PIXELFORMAT_RGB888 and SDL_PIXELFORMAT_ARGB8888 are the same four
// bytes in the same order; they differ only in whether the top byte is
// called alpha. The library accepts the second spelling, OpenTyrian asks
// with the first, and the pixels the game writes are identical either way.
SDL_Texture *__wrap_SDL_CreateTexture(SDL_Renderer *renderer, Uint32 format,
                                      int access, int w, int h)
{
    if (format == SDL_PIXELFORMAT_RGB888)
        format = SDL_PIXELFORMAT_ARGB8888;
    return __real_SDL_CreateTexture(renderer, format, access, w, h);
}

} // extern "C"
