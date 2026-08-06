# pi-opentyrian

**OpenTyrian — the open-source port of the DOS shooter Tyrian — running
directly on a Raspberry Pi with no operating system.** The board powers on and
the game is what boots: no Linux, no desktop, no launcher, and nothing else
running beside it.

It builds for the Raspberry Pi 3, Pi 4 and Pi 5, all three from one source
tree.

## What this is

[OpenTyrian](https://github.com/opentyrian/opentyrian) is an ordinary SDL2
application, written in C. This repository is the thin layer that lets it run
with nothing underneath: a [Circle](https://github.com/rsta2/circle) kernel
that brings the board up, and
[circle-libsdl2](https://github.com/Xalior/circle-libsdl2), an SDL2
implementation built on Circle's bare-metal drivers.

The game's own source is not copied or modified here. It is a submodule,
pinned at an upstream commit, and the build reads it without ever writing to
it. Everywhere the game and the library do not line up, the difference is
resolved in this repository's own `host/` directory, using the linker's
`--wrap` option so that neither submodule has to change.

Three processor cores are given separate work:

- **Core 0** owns the hardware. Circle's world lives here — interrupts, USB,
  the SD card, sound — and no other core touches a device.
- **Core 1** runs the game and nothing else, including the software synthesis
  of the OPL chip the original game's music was written for.
- **Core 2** puts finished frames on the screen. The game draws at 320x200 and
  scales that to 640x400 itself, exactly as it does on a desktop; the picture
  is then scaled once more, at the end, onto whatever the screen is really
  showing.

## The data files you have to supply

**This repository contains no game data and cannot be run without it.**
OpenTyrian is the game's program; the graphics, sounds, levels and music data
belong to Tyrian itself and are not distributed here.

The Tyrian 2.1 data was released as freeware by the game's original
developers. OpenTyrian's own README names where it is published:

> https://camanis.net/tyrian/tyrian21.zip

Unpack that archive and put its contents — `tyrian1.lvl`, `tyrian.shp`,
`voices.snd` and the rest — into the `tyrian/` directory on the SD card. That
directory is also where the game writes its settings, its key bindings and its
saved games.

Nothing else is needed, and nothing about obtaining the data involves working
around a licence, a payment or a copy protection scheme. If a source asks you
to do any of those things, it is the wrong source.

## Building

You need a Linux or macOS machine, GNU make, and the Arm GNU toolchain for
`aarch64-none-elf` (release 15.2.Rel1). Put its `bin` directory on your `PATH`,
or unpack it into `toolchains/` in this repository.

```sh
git clone --recursive https://github.com/Xalior/pi-opentyrian.git
cd pi-opentyrian
make deps       # long: builds newlib and libc++ from source, once per board
make kernels    # the three board images
make verify     # confirms each image exists and is not empty
```

`make deps` is the slow step, and it is slow once. It builds a complete C and
C++ world for each board, because each board's world is compiled for its own
processor.

Each of those worlds needs the LLVM source tree that libc++ is built from. By
default a copy is fetched next to this repository. If you build several
projects of this kind, set `CIRCLE_LLVM` to one shared checkout and they will
all use it instead of fetching their own:

```sh
make deps CIRCLE_LLVM=/path/to/shared/llvm-checkout
```

The images land in `host/build/<board>/`:

| Board | Image |
|---|---|
| Pi 3 | `host/build/rpi3/kernel8.img` |
| Pi 4 | `host/build/rpi4/kernel8-rpi4.img` |
| Pi 5 | `host/build/rpi5/kernel_2712.img` |

## Putting it on a card

```sh
make card
```

This stages a complete card into `build/sd-card/`: the Raspberry Pi firmware,
the three kernel images, and the boot configuration. Copy the contents onto a
FAT-formatted SD card, then add the Tyrian data files to the `tyrian/`
directory as described above.

`tools/mkcard` can also write a mounted card directly:

```sh
tools/mkcard /Volumes/PI-TYRIAN
```

Every file it downloads is checked against a recorded hash, so the same
repository state always produces the same card.

## Controls

OpenTyrian is played entirely from the keyboard. Plug a USB keyboard into the
board. The default keys are the game's own:

| Key | Action |
|---|---|
| Arrow keys | move the ship |
| Space | fire |
| Enter | change rear weapon mode |
| Ctrl and Alt | fire the left and right sidekicks |
| Escape | menu |

The bindings can be changed in the game's own options menu, and are saved to
the card.

## Where the pieces are

| Path | What it holds |
|---|---|
| `host/` | the Circle kernel, the SDL2 gap layer, the boot configuration |
| `mk/toolchain.mk` | finds the cross compiler |
| `tools/mkcard` | builds an SD card |
| `opentyrian/` | the game, as a submodule, unmodified |
| `circle-libsdl2/` | the SDL2 implementation, as a submodule |

Inside `host/`, each file says at the top what it is for. The ones worth
knowing about:

- `kernel.cpp` brings the board up and calls the game.
- `circle_syscalls.cpp` puts the library's any-core file service underneath
  the C library, so the game's ordinary `fopen` works from the core it runs
  on.
- `sdl_indexed.cpp` supplies the 8-bit palette surfaces Tyrian draws into.
  The library implements 32-bit surfaces only, because for its own renderer a
  surface is just a staging buffer; the game's 256-colour screen needs a real
  one.
- `sdl_audio_glue.cpp` puts the game's mono audio onto the Pi's stereo sound
  device, and converts its 8-bit sound effects to the rate the device runs at.
- `circle_stubs.cpp` holds the small remainder: key names, window geometry
  that cannot change on a machine with one screen, and one string helper.

## Bringing up a board that says nothing

A bare-metal board that boots and produces no output tells you nothing about
which half is at fault: the layer that starts the machine, or the game running
on top of it. Two things in this repository exist to separate them.

**The kernel reports each bring-up step as it passes**, by writing straight to
the serial port rather than through the logger — because the logger, the
interrupt system and the timer are themselves among the things being started,
and a failure in any of them would otherwise have no way to say so. The last
`[init]` line on the wire names the step that did not finish.

**A stub image links the scaffolding with no game in it at all:**

```sh
make -C host RAPI_BOARD=rpi5 STUB=1
```

It builds to `host/build/rpi5-stub/`, under its own name so it can never be
confused with a real image. If the stub image logs, the kernel, the world and
the link are sound and the fault is in the game or in the layer between it and
the library. If the stub image is silent, none of the game's code is involved.

Nothing reboots. Every failure path parks the board so the serial log survives
and the machine can be inspected; a power cycle is what starts it again.

> **This is instrumentation and it is a debt.** `host/stub_opentyrian.cpp`,
> the `STUB` switch in `host/Makefile`, and the per-step reporting in
> `CKernel::Initialize` are here to bring the port up on real hardware. They
> should come out once it is proven, leaving the plain kernel behind.

## What is not here

- **Network play.** OpenTyrian's two-player mode needs SDL2_net and a TCP/IP
  stack, neither of which exists under this kernel. The game is built without
  it, exactly as its own build system allows.
- **Mouse control.** The game's menus can be driven by mouse on a desktop.
  Here they are driven by the keyboard, which is how the game was played
  originally.
- **A window.** There is one screen and it is always full.

## Licence

GPL-3.0. OpenTyrian is GPL-2.0-or-later and Circle is GPL-3.0; this layer
takes the later of the two so the whole may be distributed together. See
`LICENSE`.

Tyrian's data files are the original developers' freeware release and are
covered by their own terms, not by this licence. They are not distributed
here.
