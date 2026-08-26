#
# Top-level build for amstrad-cpc-mcp.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

PROJECT   := amstrad-cpc-mcp
ROOT      := $(CURDIR)
BUILD     := $(ROOT)/build
BIN       := $(ROOT)/bin
BINDIR    := $(BIN)/bin
SHAREDIR  := $(BIN)/share/$(PROJECT)
SHAREDOCDIR := $(BIN)/share/doc/$(PROJECT)
CXX       ?= g++
CC        ?= gcc
AR        ?= ar
MODE      ?= release
WARN      := -Wall -Wextra -pedantic
INCLUDES  := -I$(ROOT)/include \
             -I$(ROOT)/lib/json/include \
             -I$(ROOT)/lib/mcp/include \
             -I$(ROOT)/lib/png/include \
             -I$(ROOT)/lib/cpc/include \
             -I$(ROOT)/lib/chips/include \
             -I$(ROOT)/lib/z80/include \
             -I$(ROOT)/lib/miniz/include

ifeq ($(MODE),debug)
  OPT      := -O1 -g -fno-omit-frame-pointer
  SAN      := -fsanitize=address,undefined
  LINKMODE := -static-libasan -static-libstdc++ -static-libgcc
  SUFFIX   := -debug
else
  OPT      := -O2 -DNDEBUG
  SAN      :=
  LINKMODE := -static
  SUFFIX   :=
endif

MINIZ_DEFS := -DMINIZ_NO_ARCHIVE_APIS -DMINIZ_NO_STDIO \
              -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
MINIZ_INC  := -I$(ROOT)/lib/miniz/include/miniz
CXXFLAGS  := -std=c++20 $(WARN) $(OPT) $(SAN) $(INCLUDES)
CFLAGS    := -std=c11 -O2 $(INCLUDES)
LDFLAGS   := $(SAN) $(LINKMODE)
OBJROOT   := $(BUILD)/$(MODE)
TARGET    := $(BINDIR)/$(PROJECT)$(SUFFIX)
LIB_NAMES := cpc mcp png json miniz
LINK_LIBS := $(foreach n,$(LIB_NAMES),$(OBJROOT)/lib$(n).a)
TOOLS_LIB := $(OBJROOT)/libtools.a
XCC       ?= $(ROOT)/../xyz/bin/x/bin/xcc
PREFIX    ?= /usr/local
DESTDIR   ?=
DATADIR   := $(PREFIX)/share/$(PROJECT)
DOCDIR    := $(PREFIX)/share/doc/$(PROJECT)

export ROOT BUILD BIN OBJROOT CXX CC AR CXXFLAGS CFLAGS LDFLAGS
export MINIZ_DEFS MINIZ_INC SUFFIX TARGET LINK_LIBS TOOLS_LIB PROJECT MODE XCC

.PHONY: all layout libs app test run-tests debug release clean distclean run \
        list-tools roms references external-tests firmware-test \
        conformance-smoke cpm-test showcase-games showcase-captures help \
        install install-release package \
        package-release $(LIB_NAMES)

all: layout

help:
	@echo "targets:"
	@echo "  all             build the release and stage the Linux-style tree"
	@echo "  debug           build the sanitizer server"
	@echo "  test            run sanitizer and xcc integration tests"
	@echo "  references      fetch verified component and CPC manuals"
	@echo "  external-tests  fetch pinned interactive CPC test media"
	@echo "  firmware-test   boot and pixel-check all three stock ROM sets"
	@echo "  conformance-smoke launch and pixel-check SHAKER module A"
	@echo "  cpm-test        boot bundled CP/M 2.2 to its pinned A> prompt"
	@echo "  showcase-games  fetch five hash-pinned free-to-play CPC games"
	@echo "  showcase-captures boot those games over MCP and refresh screenshots"
	@echo "  roms            fetch verified freely distributable firmware"
	@echo "  run              start the MCP server on stdio"
	@echo "  list-tools       print the MCP tool schemas"
	@echo "  package          refresh a release-only copyable tree under bin/"
	@echo "  install          install under PREFIX (default /usr/local)"
	@echo "  clean            remove build/ and bin/"
	@echo ""
	@echo "XCC=$(XCC) is used by the cross-compiler integration test"

libs: $(LIB_NAMES)

$(LIB_NAMES):
	@$(MAKE) --no-print-directory -C lib/$@

app: libs
	@$(MAKE) --no-print-directory -C src

layout: app
	@install -d "$(SHAREDIR)/roms" "$(SHAREDIR)/disks" \
	    "$(SHAREDOCDIR)/docs"
	@install -m 0644 "$(ROOT)/data/roms/README.md" \
	    "$(SHAREDIR)/roms/README.md"
	@install -m 0755 "$(ROOT)/data/roms/fetch-roms.sh" \
	    "$(SHAREDIR)/roms/fetch-roms.sh"
	@for rom in "$(ROOT)"/data/roms/*.rom; do \
	    test -f "$$rom" || continue; \
	    install -m 0644 "$$rom" "$(SHAREDIR)/roms/"; \
	done
	@install -m 0644 "$(ROOT)/data/disks/cpm-2.2-en.dsk" \
	    "$(ROOT)/data/disks/README.md" "$(SHAREDIR)/disks/"
	@install -m 0755 "$(ROOT)/data/disks/prepare-cpm-disk.sh" \
	    "$(SHAREDIR)/disks/prepare-cpm-disk.sh"
	@install -m 0644 "$(ROOT)/README.md" "$(ROOT)/LICENSE" \
	    "$(SHAREDOCDIR)/"
	@cp -a "$(ROOT)/docs/manuals" "$(ROOT)/docs/notes" \
	    "$(ROOT)/docs/research" "$(ROOT)/docs/screenshots" \
	    "$(SHAREDOCDIR)/docs/"

debug:
	@$(MAKE) --no-print-directory MODE=debug all

release:
	@$(MAKE) --no-print-directory MODE=release all

test:
	@$(MAKE) --no-print-directory MODE=debug run-tests

run-tests: libs
	@$(MAKE) --no-print-directory -C src toolslib
	@$(MAKE) --no-print-directory -C tests run

run: app
	@$(TARGET)

list-tools: app
	@$(TARGET) --list-tools

roms:
	@sh data/roms/fetch-roms.sh

references:
	@sh docs/research/fetch-references.sh

external-tests:
	@sh tests/external/fetch-tests.sh

firmware-test: app
	@sh tests/external/firmware-smoke.sh

conformance-smoke: app
	@sh tests/external/shaker-smoke.sh

cpm-test: app
	@sh tests/external/cpm-smoke.sh

showcase-games:
	@sh tests/external/fetch-showcase-games.sh

showcase-captures: app showcase-games
	@sh tests/external/capture-showcase.sh

install:
	@$(MAKE) --no-print-directory MODE=release install-release \
	    PREFIX="$(PREFIX)" DESTDIR="$(DESTDIR)"

install-release: app
	@install -d "$(DESTDIR)$(PREFIX)/bin" \
	    "$(DESTDIR)$(DATADIR)/roms" \
	    "$(DESTDIR)$(DATADIR)/disks" "$(DESTDIR)$(DOCDIR)/docs"
	@install -m 0755 "$(TARGET)" \
	    "$(DESTDIR)$(PREFIX)/bin/$(PROJECT)"
	@install -m 0644 "$(ROOT)/data/roms/README.md" \
	    "$(DESTDIR)$(DATADIR)/roms/README.md"
	@install -m 0755 "$(ROOT)/data/roms/fetch-roms.sh" \
	    "$(DESTDIR)$(DATADIR)/roms/fetch-roms.sh"
	@for rom in "$(ROOT)"/data/roms/*.rom; do \
	    test -f "$$rom" || continue; \
	    install -m 0644 "$$rom" "$(DESTDIR)$(DATADIR)/roms/"; \
	done
	@install -m 0644 "$(ROOT)/data/disks/cpm-2.2-en.dsk" \
	    "$(ROOT)/data/disks/README.md" "$(DESTDIR)$(DATADIR)/disks/"
	@install -m 0755 "$(ROOT)/data/disks/prepare-cpm-disk.sh" \
	    "$(DESTDIR)$(DATADIR)/disks/prepare-cpm-disk.sh"
	@install -m 0644 "$(ROOT)/README.md" "$(ROOT)/LICENSE" \
	    "$(DESTDIR)$(DOCDIR)/"
	@cp -a "$(ROOT)/docs/manuals" "$(ROOT)/docs/notes" \
	    "$(ROOT)/docs/research" "$(ROOT)/docs/screenshots" \
	    "$(DESTDIR)$(DOCDIR)/docs/"
	@echo "installed $(PROJECT) under $(DESTDIR)$(PREFIX)"

package:
	@$(MAKE) --no-print-directory MODE=release package-release

package-release: app
	@rm -rf "$(BIN)/share" "$(BIN)/package"
	@rm -f "$(BIN)/$(PROJECT)" "$(BIN)/$(PROJECT)-debug"
	@rm -f "$(BINDIR)/$(PROJECT)-debug"
	@chmod 0755 "$(TARGET)"
	@install -d "$(SHAREDIR)/roms" "$(SHAREDIR)/disks" \
	    "$(SHAREDOCDIR)/docs"
	@install -m 0644 "$(ROOT)/data/roms/README.md" \
	    "$(SHAREDIR)/roms/README.md"
	@install -m 0755 "$(ROOT)/data/roms/fetch-roms.sh" \
	    "$(SHAREDIR)/roms/fetch-roms.sh"
	@for rom in "$(ROOT)"/data/roms/*.rom; do \
	    test -f "$$rom" || continue; \
	    install -m 0644 "$$rom" "$(SHAREDIR)/roms/"; \
	done
	@install -m 0644 "$(ROOT)/data/disks/cpm-2.2-en.dsk" \
	    "$(ROOT)/data/disks/README.md" \
	    "$(SHAREDIR)/disks/"
	@install -m 0755 "$(ROOT)/data/disks/prepare-cpm-disk.sh" \
	    "$(SHAREDIR)/disks/prepare-cpm-disk.sh"
	@install -m 0644 "$(ROOT)/README.md" "$(ROOT)/LICENSE" \
	    "$(SHAREDOCDIR)/"
	@cp -a "$(ROOT)/docs/manuals" "$(ROOT)/docs/notes" \
	    "$(ROOT)/docs/research" "$(ROOT)/docs/screenshots" \
	    "$(SHAREDOCDIR)/docs/"
	@echo "package tree: $(BIN)/{bin,share}"

clean:
	@rm -rf $(BUILD) $(BIN)
	@echo "removed build/ and bin/"

distclean: clean
