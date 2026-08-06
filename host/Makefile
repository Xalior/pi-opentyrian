#
# OpenTyrian as a bootable Raspberry Pi kernel image.
#
# One board per invocation (RAPI_BOARD, default rpi5). The board selects
# three things at once: the circle-stdlib world the kernel is compiled and
# linked against, the shim archive built against that world, and — through
# the world's Config.mk — the CPU tuning and RASPPI value. Each board's
# newlib and libc++ are built for its own architecture, so an image is
# never portable between boards.
#
#   make                     the default board's image
#   make RAPI_BOARD=rpi3     that board's image
#   make clean-board         drop just this board's build/<board>/ tree
#
# Everything this invocation generates lands under build/<board>/: the
# fixed-name kernel objects, every OpenTyrian object, and the final
# .elf/.map/.lst/.img. The object names are shared across boards but their
# contents are not (different -mcpu, different -DRASPPI, different world),
# so a flat build directory would silently relink one board's objects into
# another board's kernel — a corrupt image with no build error. A directory
# per board keeps all three isolated: no `make clean` between boards, and
# three boards can build at the same time without touching each other.
#
# OpenTyrian's own source tree (../opentyrian) is never written into: it is
# read-only input, and every object it produces is redirected here.
#
# Prerequisite: the board's world and shim archive exist —
# `make -C ../circle-libsdl2 deps`.
#

include ../mk/toolchain.mk

RAPI_BOARD ?= rpi5

include ../circle-libsdl2/circle-stdlib-$(RAPI_BOARD)/Config.mk

ROOT       = ..
SHIM       = $(ROOT)/circle-libsdl2
OPENTYRIAN = $(ROOT)/opentyrian/src

# Boot bisection: STUB=1 links the host scaffolding with no game in it at all
# (see stub_opentyrian.cpp). It builds into its own directory and under its own
# name so a stub image can never be mistaken for a real one, on disk or on the
# bench, and so `make verify` keeps checking only the real ones.
#
# Instrumentation, not product. See the README's note about removing it.
STUB ?= 0

# Boot bisection by CONTENT rather than by phase. FATSTUB=1 links the stub
# payload together with the game's objects — all of them, or the first
# OT_LIMIT of them — but leaves out opentyr.o, so the game's entry point is
# still the stub's and not one line of its code is ever called.
#
# That separates two things a plain stub cannot. If an image carrying all of
# the game's code boots while the real one does not, the difference is in what
# the game DOES when it runs. If it stays silent, the mere PRESENCE of those
# objects is enough — a fact about the image's layout, not about its
# behaviour — and OT_LIMIT then bisects to the object responsible.
#
# Instrumentation, not product. See the README.
FATSTUB ?= 0
OT_LIMIT ?= 0

ifeq ($(STUB),1)
OBJDIR = build/$(RAPI_BOARD)-stub
else ifeq ($(FATSTUB),1)
OBJDIR = build/$(RAPI_BOARD)-fat$(OT_LIMIT)
else
OBJDIR = build/$(RAPI_BOARD)
endif

# Circle's own image names, which the Raspberry Pi firmware selects between
# from the model sections of config.txt: the Pi 3 world boots kernel8.img,
# the Pi 4 world kernel8-rpi4.img, the Pi 5 world kernel_2712.img.
IMAGE_rpi3 = kernel8
IMAGE_rpi4 = kernel8-rpi4
IMAGE_rpi5 = kernel_2712
IMAGE = $(IMAGE_$(RAPI_BOARD))
ifeq ($(IMAGE),)
$(error unknown RAPI_BOARD '$(RAPI_BOARD)' — expected rpi3, rpi4 or rpi5)
endif

TARGET = $(OBJDIR)/$(IMAGE)

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := image

# ---------------------------------------------------------------------------
# Where the game keeps its files on the card
# ---------------------------------------------------------------------------
#
# The directory holding the Tyrian data files, compiled into the game the way
# its own build compiles one in. It is also where saved games and settings are
# written (see game_paths.cpp), so one directory on the card holds everything
# belonging to the game. An absolute path, because a bare-metal program has no
# meaningful working directory.
TYRIAN_DIR ?= /tyrian

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

# The kernel wrapper, the syscall layer and the SDL2 gap layer: this
# project's own code.
# beacon.cpp is deliberately FIRST: it carries a static constructor whose
# whole job is to mark the moment the constructor phase begins, and link order
# is what decides when among ours it runs.
HOST_SRCS = beacon.cpp main.cpp kernel.cpp defaults.cpp circle_syscalls.cpp \
	circle_stubs.cpp sdl_indexed.cpp sdl_audio_glue.cpp game_paths.cpp
ifneq ($(filter 1,$(STUB) $(FATSTUB)),)
HOST_SRCS += stub_opentyrian.cpp
endif
HOST_OBJS = $(addprefix $(OBJDIR)/,$(HOST_SRCS:.cpp=.o))

# OpenTyrian's source list, taken the way its own Makefile takes it: every
# C file in its src directory. Upstream enumerates them with a wildcard too,
# so a file added upstream arrives here the moment the pin moves and does not
# have to be noticed by hand.
#
# Network play is left out — not by dropping the file, which upstream's own
# build never does, but by not defining WITH_NETWORK. Everything network.c
# contains is behind that define, so what is left is the handful of variables
# the menus read to find out there is no network game in progress. There is no
# TCP/IP stack under this kernel and no SDL2_net to build against.
OT_SRCS = $(notdir $(wildcard $(OPENTYRIAN)/*.c))

ifeq ($(STUB),1)
# The stub image links none of it.
OT_OBJS =
else ifeq ($(FATSTUB),1)
# Every one of the game's objects, opentyr.o included — it carries other
# functions the rest of the game refers to, so leaving it out would not link.
# Its entry point is renamed aside instead (GAME_ENTRY below), leaving the
# stub's as the one the kernel calls.
#
# These objects are on the link line as objects rather than archive members,
# so every one of them lands in the image whether anything references it or
# not — which is the whole point.
OT_ALL = $(addprefix $(OBJDIR)/ot/,$(OT_SRCS:.c=.o))
ifeq ($(OT_LIMIT),0)
OT_OBJS = $(OT_ALL)
else
OT_OBJS = $(wordlist 1,$(OT_LIMIT),$(OT_ALL))
endif
else
OT_OBJS = $(addprefix $(OBJDIR)/ot/,$(OT_SRCS:.c=.o))
endif

OBJS = $(HOST_OBJS) $(OT_OBJS)

STANDARD = -std=gnu++17

# OpenTyrian is C99, as its own build declares. Circle's Rules.mk defaults to
# gnu99 and the game uses no GNU extension, so the two agree; it is stated
# here so the language the game is compiled in is a fact in the file.
C_STANDARD = -std=gnu99

# Stated, not inherited: this is Circle's own default, written down so the
# build's optimization level is a fact in the file rather than a forensic
# reconstruction.
OPTIMIZE = -O2

# Two warnings upstream turns off in its own build, for the same reasons: the
# game formats fixed-size strings deliberately, and initialises the fields of
# its big static tables that it cares about.
WARNINGS = -Wall -Wno-format-truncation -Wno-missing-field-initializers

# The game's compile-time configuration, in the spelling its own Makefile
# uses. TARGET_UNIX selects the POSIX branches — a two-argument mkdir, and an
# environment-based user directory — which are the ones this platform's C
# library offers. SDL_MAIN_HANDLED keeps SDL_main.h from renaming main(),
# which this build renames itself, below.
DEFINE += -DTARGET_UNIX -DNDEBUG -DSDL_MAIN_HANDLED
DEFINE += -DTYRIAN_DIR='"$(TYRIAN_DIR)"'

include $(CIRCLEHOME)/Rules.mk

# The world must be the multicore one circle-libsdl2 configures: the shim's
# core-split machinery cannot exist without it, and an object compiled
# against a single-core world links without complaint and then misbehaves.
# Every object here inherits the flag through Rules.mk's CPPFLAGS, so this
# check covers the whole build.
ifeq ($(findstring ARM_ALLOW_MULTI_CORE,$(DEFINE)),)
$(error the payload requires the multicore circle-stdlib world — build it with 'make -C ../circle-libsdl2 deps')
endif

# OpenTyrian writes `#include "SDL.h"`, the spelling a desktop sdl2-config
# include path provides, so the shim's SDL2/ directory goes on the path in
# its own right alongside the SDL2/-prefixed one this project's own code uses.
INCLUDE := -I $(SHIM)/include -I $(SHIM)/include/SDL2 -I $(OPENTYRIAN) \
	$(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

# The shim comes in whole, every member, rather than only the members the
# link happens to need. An object file linked straight into the kernel beats
# an archive member of the same name and does it silently, so a definition in
# this directory left behind after the library implemented the real thing
# would quietly win — the game would keep calling ours and nothing would say
# so. Pulling the archive in whole turns that into a duplicate-symbol error at
# link time, which is the only moment it can still be cheap to fix. Where this
# build genuinely has to displace something the library provides, it does it
# with --wrap below, which is visible in this file rather than silent.
LIBS = --whole-archive $(SHIM)/libSDL2-$(RAPI_BOARD).a --no-whole-archive \
	$(CIRCLE_STDLIB_LIBS)

# ---------------------------------------------------------------------------
# Filesystem syscalls, redirected at link time
# ---------------------------------------------------------------------------
#
# The game runs on the application core, and newlib's Circle glue drives
# FatFs and the SD card straight from wherever it is called — which off core
# 0 is illegal. circle_syscalls.cpp puts the shim's any-core I/O service
# underneath the C library instead; these flags are what put it there.
#
# --wrap rather than redefinition, because redefinition cannot work here:
# the glue defines _open, _read, _write, _close, _lseek, _fstat, opendir,
# readdir, closedir, dup and more in a SINGLE object file. Redefining part
# of that set either collides at link time or, worse, leaves the originals
# linked in beside the replacements with no diagnostic. --wrap renames the
# references instead and leaves the vendored glue untouched — and it is what
# keeps __real_* reachable, which is how the service performs the real work
# once it has reached core 0.
#
# Every name here must also appear in circle_syscalls.cpp: a wrap with no
# __wrap_ definition is an undefined reference, so the link is the check.
WRAPPED_SYSCALLS = \
	_open _close _read _write _lseek _fstat _stat lstat _unlink _rename \
	_fcntl mkdir chdir getcwd dup dup2 \
	opendir readdir closedir rewinddir

# ---------------------------------------------------------------------------
# SDL and game entry points, redirected at link time
# ---------------------------------------------------------------------------
#
# The same mechanism, used for four places where what the game asks for and
# what the library offers do not line up. Each one is implemented in this
# directory, calls __real_ for the cases it does not handle, and is explained
# where it is written.
#
#   SDL_CreateRGBSurface   an 8-bit request is served here; every other depth
#   SDL_FreeSurface        goes to the library      (sdl_indexed.cpp)
#   SDL_CreateTexture      RGB888 is the library's ARGB8888 (sdl_indexed.cpp)
#   SDL_OpenAudioDevice    a mono game on a stereo device
#                                                 (sdl_audio_glue.cpp)
#   get_user_directory     the game's own; there is no environment to read
#                                                   (game_paths.cpp)
WRAPPED_SDL = \
	SDL_CreateRGBSurface SDL_FreeSurface SDL_CreateTexture \
	SDL_OpenAudioDevice

WRAPPED_GAME = get_user_directory

# The link is driven by the cross ld directly (Circle's Rules.mk sets
# LD = <prefix>ld for GCC builds), so these are plain linker options, not
# compiler-driver -Wl, ones.
LDFLAGS += $(addprefix --wrap=,$(WRAPPED_SYSCALLS) $(WRAPPED_SDL) $(WRAPPED_GAME))

# A PARTIAL set of the game's objects (FATSTUB with OT_LIMIT) cannot resolve
# among itself: the game's translation units refer to each other freely, and
# half of them is half a program. The unresolved references are left as zero
# rather than refused, which is sound here for one reason only — the stub owns
# the entry point, so not one of those references is ever followed. It is a
# layout experiment, and the layout is all it is asked to produce.
#
# Never applied to a real image: OT_LIMIT is meaningless unless FATSTUB is on,
# and a full FATSTUB link (OT_LIMIT=0) resolves everything by itself.
ifeq ($(FATSTUB),1)
ifneq ($(OT_LIMIT),0)
LDFLAGS += --unresolved-symbols=ignore-all --noinhibit-exec
endif
endif

# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

# This project's own objects, board-scoped. These more-specific rules win
# over Rules.mk's in-place %.o/%.d, so nothing is written beside the source.
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) -c -o $@ $<

$(OBJDIR)/%.d: %.cpp | $(OBJDIR)
	@$(CPP) $(CPPFLAGS) -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

# OpenTyrian's objects, mirroring its source tree under $(OBJDIR)/ot/. The
# source tree itself is a submodule pinned at an upstream commit, and is
# never written to.
$(OBJDIR)/ot/%.o: $(OPENTYRIAN)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(C_STANDARD) -c -o $@ $<

$(OBJDIR)/ot/%.d: $(OPENTYRIAN)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(C_STANDARD) -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

# OpenTyrian's main() is the program's entry point on a desktop, but here
# main() belongs to the Circle kernel (main.cpp in this directory), which
# calls OpenTyrian once the board is up. Renaming the entry point through
# the preprocessor for this one translation unit keeps the upstream source
# untouched: nothing else in the game declares or calls main, and the kernel
# calls the new name.
# Normally the game's main() becomes the entry point the kernel calls. Under
# FATSTUB it is renamed to a name nothing calls, so the object still supplies
# the other functions the game refers to while the stub owns the entry point.
ifeq ($(FATSTUB),1)
GAME_ENTRY = opentyrian_main_unused
else
GAME_ENTRY = opentyrian_main
endif

$(OBJDIR)/ot/opentyr.o: $(OPENTYRIAN)/opentyr.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@ (entry point renamed to $(GAME_ENTRY))"
	@$(CC) $(CFLAGS) $(C_STANDARD) -Dmain=$(GAME_ENTRY) -c -o $@ $<

$(OBJDIR)/ot/opentyr.d: $(OPENTYRIAN)/opentyr.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(C_STANDARD) -Dmain=$(GAME_ENTRY) -M -MG -MT $(@:.d=.o) -MT $@ -MF $@ $<

$(OBJDIR):
	@mkdir -p $(OBJDIR)

-include $(DEPS)

# The image link rule. sdl-app.mk lets the caller choose the linker script,
# so this build supplies its own: the shim's script with room made at image
# offset 0x800 for the patchable-defaults block. Everything else about the
# script — including the TLS-section adjacency binutils 2.44 and later
# require — is inherited from the shim's.
SDL_APP_LDSCRIPT = $(CURDIR)/opentyrian-defaults.ld
include $(SHIM)/sdl-app.mk

# The other half of the guarantee the linker script's ASSERTs begin: check
# the BYTES of the finished image, not just the link. A pre-boot writer
# refuses to stamp an image whose magic is not at 0x800, so an image that
# lost it would boot and then quietly ignore everything ever sent to it.
# Better to have no image than that one.
.PHONY: image
image: $(TARGET).img
	@if [ "`dd if=$(TARGET).img bs=4 skip=512 count=1 2>/dev/null`" = "PM8D" ]; \
	then \
		echo "  GATE  $(TARGET).img defaults block at 0x800: present"; \
	else \
		echo "  GATE  $(TARGET).img: defaults block ABSENT at 0x800 — image deleted"; \
		rm -f $(TARGET).img; \
		exit 1; \
	fi

# Board-scoped clean: drop just this board's tree, leaving the other two
# boards' images and objects untouched.
.PHONY: clean-board
clean-board:
	rm -rf $(OBJDIR)
