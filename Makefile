#
# pi-opentyrian — OpenTyrian as a bootable bare-metal Raspberry Pi image.
#
#   make check-toolchain     report the cross compiler this build will use
#   make deps                the three circle-stdlib worlds and the shim
#                            archives built against them (long: the worlds
#                            build newlib and libc++ from source)
#   make deps-rpi4           the same for one board only, for a machine that
#                            cannot hold three worlds at once
#   make rpi5 | rpi4 | rpi3  one board's kernel image
#   make kernels             all three, built in parallel
#   make verify              truth-gate: every image exists and is non-empty
#   make media               download the freely redistributable game data
#                            into media/. Run by a person, on their own
#                            responsibility — read the README first
#   make netboot             stage the Pi 5 image and its boot configuration
#                            into build/netboot-rpi5/
#   make card                stage the whole card into build/sd-card/, copying
#                            in whatever media/ holds and naming what it does
#                            not. It never downloads anything
#   make clean-boards        drop every board's build tree
#
# The three boards never share mutable state: each has its own circle-stdlib
# world, its own shim archive and its own object directory, so building them
# at the same time is safe and building one never disturbs another.
#

include mk/toolchain.mk

# Stated explicitly because the first rule this file sees comes from an
# included makefile, and that would otherwise decide the default goal.
.DEFAULT_GOAL := kernels

BOARDS ?= rpi3 rpi4 rpi5

IMAGE_rpi3 = kernel8.img
IMAGE_rpi4 = kernel8-rpi4.img
IMAGE_rpi5 = kernel_2712.img

.PHONY: deps kernels verify media netboot card clean-boards $(BOARDS)
.PHONY: $(addprefix deps-,$(BOARDS))

deps:
	$(MAKE) -C circle-libsdl2 deps

# One board's dependencies: its own circle-stdlib world and the shim archive
# built against it. Each world needs the LLVM tree libc++ is built from, which
# is gigabytes, so a machine with a small disk — a CI runner, most obviously —
# builds one board at a time and keeps only that board's world.
# Written as a static pattern rule over the board list rather than a plain
# pattern rule: these targets are phony, and make does not apply pattern rules
# to phony targets — it would quietly answer "nothing to be done" and leave
# the world unbuilt.
$(addprefix deps-,$(BOARDS)): deps-%:
	$(MAKE) -C circle-libsdl2 world BOARD=$*
	$(MAKE) -C circle-libsdl2 libSDL2-$*.a BOARD=$*

$(BOARDS): check-toolchain
	$(MAKE) -C host RAPI_BOARD=$@

# All three at once. Each sub-make owns a different world and a different
# output directory, so there is nothing for them to collide on.
#
# Each board is waited for BY PID, and its status kept. A bare `wait` reports
# only that the shell has no children left — it is success whatever the jobs
# did — so a board that failed to build would leave this target reporting
# success, and the truth-gate would then pass the board's PREVIOUS image,
# still on disk.
kernels: check-toolchain
	@pids=; fail=0; \
	for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b & pids="$$pids $$!"; done; \
	for p in $$pids; do wait $$p || fail=1; done; \
	exit $$fail

# Truth-gate: ask the filesystem, not the exit codes. An image that is
# missing or empty fails here even if the build claimed success.
verify:
	@fail=0; \
	for b in $(BOARDS); do \
		case $$b in \
			rpi3) img=host/build/rpi3/$(IMAGE_rpi3) ;; \
			rpi4) img=host/build/rpi4/$(IMAGE_rpi4) ;; \
			rpi5) img=host/build/rpi5/$(IMAGE_rpi5) ;; \
		esac; \
		if [ -s "$$img" ]; then \
			echo "  OK    $$img ($$(wc -c < $$img | tr -d ' ') bytes)"; \
		else \
			echo "  FAIL  $$img missing or empty"; fail=1; \
		fi; \
	done; \
	exit $$fail

# ---------------------------------------------------------------------------
# Game data
# ---------------------------------------------------------------------------
#
# TWO DIRECTORIES, AND THE SEPARATION BETWEEN THEM IS THE POINT.
#
#   media/           what `make media` downloads. Gitignored, never shipped,
#                    and never part of a build.
#   build/sd-card/   what `make card` stages. It COPIES FROM media/ and
#                    fetches nothing, ever.
#
# `card` does not depend on `media`. A card built without it is a legitimate
# card — complete except for the data, and it says exactly which files are
# absent. That is what a build with no network produces.
#
# WHAT `make media` FETCHES, and it is only ever this one archive:
# `tyrian21.zip`, the Tyrian 2.1 data files that Epic MegaGames released as
# freeware in 2004. OpenTyrian's own upstream README names this exact URL as
# the freeware data source. camanis.net is a plain, unauthenticated file
# listing — no login, no paywall, no click-through gate.
#
# The zip's own tyrian21/ directory unpacks with the 107 files the game
# reads directly: levels, ships, sound and the rest. Nothing in it is
# rewritten or renamed; it lands in media/ exactly as camanis.net serves it.
#
# The download is plain curl. What arrives is checked against the SHA256 and
# MD5 computed from this project's own download — no independently published
# checksum for this archive was found, so neither is authoritative the way
# Doom's shareware MD5 is; they only prove a later fetch produced identical
# bytes. Re-running re-verifies rather than re-downloading, and re-extracts
# only if the unpacked tree is not already there.
MEDIA_DIR = media

TYRIAN_ZIP    = $(MEDIA_DIR)/tyrian21.zip
TYRIAN_URL    = https://camanis.net/tyrian/tyrian21.zip
TYRIAN_SHA256 = 7790d09a2a3addcd33c66ef063d5900eb81cc9c342f4807eb8356364dd1d9277
TYRIAN_MD5    = 2a3b206a6de25ed4b771af073f8ca904
TYRIAN_DATA   = $(MEDIA_DIR)/tyrian21

# sha256sum and md5sum on Linux, shasum and md5 on macOS. Whichever exists;
# if either is missing the target stops rather than accepting a download it
# cannot check.
SHA256SUM := $(firstword $(shell command -v sha256sum 2>/dev/null) \
                         $(shell command -v shasum 2>/dev/null))
MD5SUM    := $(firstword $(shell command -v md5sum 2>/dev/null) \
                         $(shell command -v md5 2>/dev/null))

media:
	@if [ -z "$(SHA256SUM)" ] || [ -z "$(MD5SUM)" ]; then \
		echo "  MEDIA no checksum tool on this machine (sha256sum/shasum and"; \
		echo "        md5sum/md5 are both needed) — refusing to download"; \
		echo "        something that cannot be verified."; \
		exit 1; \
	fi
	@command -v unzip >/dev/null 2>&1 || { \
		echo "  MEDIA no unzip on this machine — refusing to fetch an archive"; \
		echo "        it cannot then unpack."; \
		exit 1; }
	@mkdir -p $(MEDIA_DIR)
	@if [ -f "$(TYRIAN_ZIP)" ]; then \
		echo "  MEDIA $(TYRIAN_ZIP) already here — verifying"; \
	else \
		echo "  MEDIA fetching $(TYRIAN_URL)"; \
		curl -fL --retry 3 -o "$(TYRIAN_ZIP).part" "$(TYRIAN_URL)" || { \
			rm -f "$(TYRIAN_ZIP).part"; \
			echo "  MEDIA download failed"; exit 1; }; \
		mv "$(TYRIAN_ZIP).part" "$(TYRIAN_ZIP)"; \
	fi
	@got=`$(SHA256SUM) -a 256 "$(TYRIAN_ZIP)" 2>/dev/null || $(SHA256SUM) "$(TYRIAN_ZIP)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(TYRIAN_SHA256)" ]; then \
		echo "  MEDIA SHA256 MISMATCH for $(TYRIAN_ZIP)"; \
		echo "        expected $(TYRIAN_SHA256)"; \
		echo "        got      $$got"; \
		echo "        the file has been left in place for inspection, and is"; \
		echo "        NOT safe to put on a card."; \
		exit 1; \
	fi; \
	got=`$(MD5SUM) -q "$(TYRIAN_ZIP)" 2>/dev/null || $(MD5SUM) "$(TYRIAN_ZIP)"`; \
	got=`echo "$$got" | awk '{print $$1}'`; \
	if [ "$$got" != "$(TYRIAN_MD5)" ]; then \
		echo "  MEDIA MD5 MISMATCH for $(TYRIAN_ZIP)"; \
		echo "        expected $(TYRIAN_MD5)"; \
		echo "        got      $$got"; \
		exit 1; \
	fi; \
	head -c 2 "$(TYRIAN_ZIP)" | grep -q PK || { \
		echo "  MEDIA $(TYRIAN_ZIP) does not begin with the PK zip magic"; exit 1; }; \
	echo "  MEDIA $(TYRIAN_ZIP) verified ($$(wc -c < $(TYRIAN_ZIP) | tr -d ' ') bytes)"
	@if [ -f "$(TYRIAN_DATA)/tyrian1.lvl" ]; then \
		echo "  MEDIA $(TYRIAN_DATA)/ already unpacked"; \
	else \
		echo "  MEDIA unpacking $(TYRIAN_ZIP)"; \
		unzip -q -o "$(TYRIAN_ZIP)" -d "$(MEDIA_DIR)" || { \
			echo "  MEDIA unzip failed"; exit 1; }; \
		[ -f "$(TYRIAN_DATA)/tyrian1.lvl" ] || { \
			echo "  MEDIA unpacked tree is missing tyrian1.lvl — the archive"; \
			echo "        did not contain what was expected"; exit 1; }; \
	fi
	@echo "  MEDIA $(TYRIAN_DATA)/ ready"
	@printf '%s\n' \
		"tyrian21.zip — the Tyrian 2.1 data files" \
		"" \
		"Source:   $(TYRIAN_URL)" \
		"Fetched:  `date -u '+%Y-%m-%d %H:%M:%S UTC'`" \
		"SHA256:   $(TYRIAN_SHA256)" \
		"MD5:      $(TYRIAN_MD5)" \
		"" \
		"What it is: the Tyrian 2.1 data files — levels, ships, sound and the" \
		"rest — that Epic MegaGames released as freeware in 2004. OpenTyrian's" \
		"own upstream README names this exact URL as the freeware data" \
		"source." \
		"" \
		"Licence: Epic MegaGames freeware release, 2004. The archive itself" \
		"still carries its original 1995 shrink-wrap license.doc and a 1995" \
		"file_id.diz reading 'all copying is prohibited' — neither was" \
		"updated for the later freeware release, which is documented" \
		"separately by OpenTyrian's own project rather than inside the zip." \
		"" \
		"Verification: both checksums above were computed from this" \
		"project's own download. No independently published checksum for" \
		"this archive was found, so — unlike Doom's shareware MD5 — neither" \
		"one is an authoritative reference; they only prove a later fetch" \
		"produced identical bytes to this one." \
		"" \
		"Tyrian is a trademark of its rights holders. This archive is not" \
		"ours, is not redistributed by this repository, and is downloaded" \
		"only by a person running 'make media' on their own machine." \
		> $(MEDIA_DIR)/provenance.txt
	@echo "  MEDIA provenance written to $(MEDIA_DIR)/provenance.txt"

# The Pi 5 netboot bundle: the image the Pi 5 firmware looks for, plus the
# boot configuration it must be served alongside. Copy the contents into the
# TFTP root the board boots from (the Raspberry Pi firmware files themselves
# come from that root's existing installation, not from here).
NETBOOT_DIR = build/netboot-rpi5
netboot: rpi5
	@mkdir -p $(NETBOOT_DIR)
	@cp host/build/rpi5/$(IMAGE_rpi5) $(NETBOOT_DIR)/
	@cp host/config.txt host/cmdline.txt $(NETBOOT_DIR)/
	@echo "  STAGED $(NETBOOT_DIR)/"
	@ls -l $(NETBOOT_DIR)/

# The card, staged into a directory to copy onto media formatted elsewhere:
# the three kernels, the boot configuration, and whatever media/ holds of the
# Tyrian data files. THIS TARGET NEVER DOWNLOADS ANYTHING — mkcard copies
# what `make media` left into games/tyrian/ and says so if it is empty.
CARD_DIR = build/sd-card
card: kernels
	@rm -rf $(CARD_DIR)
	@tools/mkcard --stage $(CARD_DIR)

clean-boards:
	@for b in $(BOARDS); do $(MAKE) -C host RAPI_BOARD=$$b clean-board; done
	rm -rf $(NETBOOT_DIR)
