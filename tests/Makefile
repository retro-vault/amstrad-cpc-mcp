#
# Sanitized unit, protocol and cross-compiler integration tests.
#
# GPL 3.0 License (see: LICENSE)
# Copyright (C) 2026 tomaz stih
#

SRCDIR   := $(ROOT)/tests
OBJDIR   := $(OBJROOT)/tests
SOURCES  := $(wildcard $(SRCDIR)/test_*.cpp)
BINARIES := $(patsubst $(SRCDIR)/test_%.cpp,$(OBJDIR)/test_%,$(SOURCES))
DEPS     := $(BINARIES:=.d)
XCC_PROBE := $(OBJDIR)/xcc_probe.bin

.PHONY: all run clean
all: $(BINARIES)

$(XCC_PROBE): $(SRCDIR)/fixtures/xcc_probe.c
	@test -x "$(XCC)" || { echo "xcc not found at $(XCC)"; exit 1; }
	@mkdir -p $(dir $@)
	@$(XCC) -Os -Wall -Wextra --oformat=binary -Ttext=0x8000 $< -o $@
	@echo "  XCC     $(notdir $@)"

$(OBJDIR)/test_xcc: $(SRCDIR)/test_xcc.cpp $(XCC_PROBE) \
                     $(TOOLS_LIB) $(LINK_LIBS)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -I$(SRCDIR) \
	    -DXCC_PROBE_PATH='"$(XCC_PROBE)"' -MMD -MP $< \
	    $(TOOLS_LIB) $(LINK_LIBS) $(LDFLAGS) -o $@
	@echo "  TEST    $(notdir $@)"

$(OBJDIR)/test_%: $(SRCDIR)/test_%.cpp $(TOOLS_LIB) $(LINK_LIBS)
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -I$(SRCDIR) -MMD -MP $< \
	    $(TOOLS_LIB) $(LINK_LIBS) $(LDFLAGS) -o $@
	@echo "  TEST    $(notdir $@)"

run: $(BINARIES)
	@echo ""
	@failed=0; \
	for suite in $(BINARIES); do \
	    $$suite || failed=1; \
	done; \
	echo ""; \
	if [ $$failed -eq 0 ]; then \
	    echo "=== all test suites passed ==="; \
	else \
	    echo "=== TEST FAILURES ==="; \
	fi; \
	exit $$failed

clean:
	@rm -rf $(OBJDIR)

-include $(DEPS)
