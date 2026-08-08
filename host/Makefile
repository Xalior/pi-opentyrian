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

BOARDS      = rpi3 rpi4 rpi5
RAPI_BOARD ?= rpi5

ROOT       = ..
# WHERE THE SHIM COMES FROM.
#
# Unset, this repository's own pinned copy — what a clone of this repository
# gets, and what CI builds. A parent proving one shim across several games
# points SHIM at the single checkout it is proving, so the library is built
# once rather than once per game.
SHIM       ?= $(ROOT)/circle-libsdl2
OPENTYRIAN = $(ROOT)/opentyrian/src

OBJDIR = build/$(RAPI_BOARD)

# Circle's own image names, which the Raspberry Pi firmware selects between
# from the model sections of config.txt: the Pi 3 world boots kernel8.img,
# the Pi 4 world kernel8-rpi4.img, the Pi 5 world kernel_2712.img.
IMAGE_rpi3 = kernel8
IMAGE_rpi4 = kernel8-rpi4
IMAGE_rpi5 = kernel_2712
IMAGE = $(IMAGE_$(RAPI_BOARD))

# Before the world is included, so an unknown board is named as an unknown
# board. After the include it arrives as a missing Config.mk under a directory
# whose name nobody recognises.
ifeq ($(filter $(RAPI_BOARD),$(BOARDS)),)
$(error unknown RAPI_BOARD '$(RAPI_BOARD)' — expected one of: $(BOARDS))
endif

# The board's circle-stdlib world. A `make deps` that has not been run, or has
# not finished, leaves this file absent; make would then report that it has no
# rule to make a Config.mk, which describes the symptom and not the cause.
# WHERE THE BUILT WORLD COMES FROM.
#
# The default is this game's own nested shim, so a clone of this repository
# alone builds and uses its own worlds and nothing needs setting. A parent
# that keeps ONE set of built worlds for several games — they are gigabytes
# each, and identical when the pin and the configuration match — points
# CIRCLE_WORLDS at them.
CIRCLE_WORLDS ?= $(SHIM)
WORLD = $(CIRCLE_WORLDS)/circle-stdlib-$(RAPI_BOARD)
ifeq ($(wildcard $(WORLD)/Config.mk),)
$(error the $(RAPI_BOARD) circle-stdlib world is not configured ($(WORLD)/Config.mk is missing) — run 'make deps-$(RAPI_BOARD)' at the repository root)
endif

include $(WORLD)/Config.mk

TARGET = $(OBJDIR)/$(IMAGE)

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := image

# ---------------------------------------------------------------------------
# Where the game keeps its files on the card
# ---------------------------------------------------------------------------
#
# EVERYTHING THIS GAME TOUCHES LIVES IN ONE DIRECTORY ON THE CARD. A card
# carries several games, and two of them writing their settings into the FAT
# root would each silently overwrite the other's. Under /games/ rather than at
# the root so a dozen of them do not sprawl across it.
#
# A build-time constant, not a boot switch: a card's layout is decided when the
# card is staged, and a game that could be relocated at boot would only mean
# two places to look for the same file.
#
# TYRIAN_DIR is upstream OpenTyrian's OWN build knob — its src/file.c reads it
# to find the data files — so pointing it here needs no patch and no fork. This
# port sets it from RAPI_GAME_DIR, the name every game in this family uses, and
# game_paths.cpp answers the saved-game and settings path with the same value.
RAPI_GAME_DIR ?= /games/tyrian
TYRIAN_DIR    ?= $(RAPI_GAME_DIR)
DEFINE        += -DRAPI_GAME_DIR='"$(RAPI_GAME_DIR)"'

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

# The kernel wrapper, the syscall layer and the SDL2 gap layer: this
# project's own code.
HOST_SRCS = main.cpp kernel.cpp defaults.cpp circle_syscalls.cpp \
	circle_stubs.cpp game_paths.cpp
HOST_OBJS = $(addprefix $(OBJDIR)/,$(HOST_SRCS:.cpp=.o))

# The SD card's read cache, which lives in circle-libsdl2 rather than here.
#
# It is not part of SDL and does not use it: it is Circle device-layer code
# that wraps the card and is picked up by name, so FatFs and every file the
# game opens go through it without knowing. It is kept in that repository
# because that is where the shared pieces are today, and it is compiled
# straight into this kernel — there is no library to link, and no other port
# has to agree with this one about how it is built.
DISKCACHE_DIR = $(SHIM)/circle-diskcache
DISKCACHE_OBJ = $(OBJDIR)/diskcache.o

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

OT_OBJS = $(addprefix $(OBJDIR)/ot/,$(OT_SRCS:.c=.o))

OBJS = $(HOST_OBJS) $(DISKCACHE_OBJ) $(OT_OBJS)

STANDARD = -std=gnu++17

# OpenTyrian is C99, as its own build declares. Circle's Rules.mk defaults to
# gnu99 and the game uses no GNU extension, so the two agree; it is stated
# here so the language the game is compiled in is a fact in the file.
C_STANDARD = -std=gnu99 $(RAPI_CFLAGS)

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

# Dependency files are written as a side effect of compiling rather than by
# rules of their own, and Circle's Rules.mk is told to keep its hands off
# (CHECK_DEPS) so its `-M -MG` .d rules are not defined for these objects.
#
# -MG lists a header make has no rule for as a prerequisite of the object,
# which stops the build; GNU make 3.81 stops with exit 2 and prints not one
# line saying why, which is why this build asks for 4.0 in mk/toolchain.mk. A
# .d left over from an earlier build naming a header that has since moved or
# been deleted stops it the same way, and is why "delete the board's object
# directory" kept being the cure.
#
# -MD writes the dependency file alongside the object it belongs to, so the
# two can never disagree about the flags that produced them, and -MP adds a
# rule with no recipe for every header named, so a header that disappears
# means a rebuild instead of a dead stop.
DEPS       = $(OBJS:.o=.d)
CHECK_DEPS = 0
DEPFLAGS   = -MD -MP

# Circle's Rules.mk defines a link rule for $(TARGET).img with its own linker
# script; sdl-app.mk below defines the one this port actually uses. Two
# recipes for one target makes make warn on every board of every build — a
# warning that reads like a fault and is not one. Pointing TARGET at a name
# nothing builds while Rules.mk is read attaches its rule there instead, so
# there is only ever one recipe for the image.
SDL_APP_IMAGE := $(TARGET)
TARGET := $(OBJDIR)/.circle-unused
include $(CIRCLEHOME)/Rules.mk
TARGET := $(SDL_APP_IMAGE)

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
	-I $(DISKCACHE_DIR) \
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
SHIM_LIB = $(SHIM)/libSDL2-$(RAPI_BOARD).a

LIBS = --whole-archive $(SHIM_LIB) --no-whole-archive \
	$(CIRCLE_STDLIB_LIBS)

# The shim archive is built in circle-libsdl2 and never here — it is a shared
# dependency, and a rule here would rebuild it at whatever board and crossing
# count this makefile happens to be running with. This rule exists only for the
# case where it is absent: it runs then, and only then, because a target with a
# recipe and no prerequisites is up to date as soon as the file exists.
$(SHIM_LIB):
	@echo "$@ is missing: the $(RAPI_BOARD) shim archive has not been built."
	@echo "Run: $(MAKE) -C $(SHIM) libSDL2-$(RAPI_BOARD).a BOARD=$(RAPI_BOARD)"
	@exit 1

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
# Game entry points, redirected at link time
# ---------------------------------------------------------------------------
#
# The same mechanism, for a place where what the game asks for and what the
# environment offers do not line up. Implemented in this directory, and
# explained where it is written.
#
#   get_user_directory     the game's own; there is no environment to read
#                                                   (game_paths.cpp)
WRAPPED_GAME = get_user_directory

# The link is driven by the cross ld directly (Circle's Rules.mk sets
# LD = <prefix>ld for GCC builds), so these are plain linker options, not
# compiler-driver -Wl, ones.
LDFLAGS += $(addprefix --wrap=,$(WRAPPED_SYSCALLS) $(WRAPPED_GAME))

# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

# This project's own objects, board-scoped. These more-specific rules win
# over Rules.mk's in-place %.o/%.d, so nothing is written beside the source.
$(OBJDIR)/%.o: %.cpp | $(OBJDIR)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) $(DEPFLAGS) -c -o $@ $<

# The disk cache, which lives outside this directory. Its own rule because
# the pattern rule above only reaches sources beside this makefile, and its
# object goes in this board's tree like every other.
$(DISKCACHE_OBJ): $(DISKCACHE_DIR)/diskcache.cpp | $(OBJDIR)
	@echo "  CPP   $@"
	@$(CPP) $(CPPFLAGS) $(DEPFLAGS) -c -o $@ $<

# OpenTyrian's objects, mirroring its source tree under $(OBJDIR)/ot/. The
# source tree itself is a submodule pinned at an upstream commit, and is
# never written to.
$(OBJDIR)/ot/%.o: $(OPENTYRIAN)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) $(C_STANDARD) $(DEPFLAGS) -c -o $@ $<

# OpenTyrian's main() is the program's entry point on a desktop, but here
# main() belongs to the Circle kernel (main.cpp in this directory), which
# calls OpenTyrian once the board is up. Renaming the entry point through
# the preprocessor for this one translation unit keeps the upstream source
# untouched: nothing else in the game declares or calls main, and the kernel
# calls the new name.
$(OBJDIR)/ot/opentyr.o: $(OPENTYRIAN)/opentyr.c
	@mkdir -p $(dir $@)
	@echo "  CC    $@ (entry point renamed)"
	@$(CC) $(CFLAGS) $(C_STANDARD) $(DEPFLAGS) -Dmain=opentyrian_main -c -o $@ $<

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

# This board from nothing. An incremental build is a decision made from
# timestamps, and a timestamp says when a file was written rather than what is
# in it; anything read off the resulting image describes what the last build
# happened to put there. Take a measurement that has to be right off one of
# these.
.PHONY: rebuild
rebuild:
	+@$(NOT_DRY_RUN)
	@$(MAKE) clean-board RAPI_BOARD=$(RAPI_BOARD)
	@$(MAKE) image RAPI_BOARD=$(RAPI_BOARD)
