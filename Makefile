# Requires GNU make (wildcard, shell, pattern rules).
#
# Override from the command line or environment as usual, e.g.:
#   make CXX=clang++ CXXFLAGS="-O2"   (or DEBUG=1 for a build to debug)
# Everything is built into $(BUILD) (build/ by default).

-include local/config.mk

BUILD ?= build

# Clang is stricter, so it is the default compiler when it is installed (the
# newest versioned one if there is no plain clang++); g++ otherwise.
# Set CXX to choose another one.
ifeq ($(origin CXX),default)
CLANGXX := $(firstword $(shell command -v clang++ 2>/dev/null) \
    $(shell tools/newest.sh /usr/bin/clang++-[0-9]*))
ifneq (,$(CLANGXX))
CXX := $(CLANGXX)
endif
endif

# Strict warnings. STRICT=1 turns them into errors (used by CI); it is not the
# default, so that a newer compiler does not break the build of a user.
WARNINGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion \
    -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wcast-align \
    -Wnull-dereference -Wdouble-promotion -Wformat=2 -Wimplicit-fallthrough
CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
ifneq (,$(findstring clang,$(CXX_VERSION)))
WARNINGS += -Wextra-semi -Wunreachable-code
else
WARNINGS += -Wuseless-cast -Wduplicated-cond -Wlogical-op
endif
ifdef STRICT
WARNINGS += -Werror
endif

# HARDEN=1 adds the hardening flags for a release build (needs optimization,
# e.g. make HARDEN=1 CXXFLAGS="-O2"). Not the default: _FORTIFY_SOURCE needs
# glibc and _GLIBCXX_ASSERTIONS needs libstdc++.
ifdef HARDEN
HARDENING = -D_FORTIFY_SOURCE=3 -D_GLIBCXX_ASSERTIONS -fstack-protector-strong
endif

# fontconfig is a dependency of Xft; it is named because the list of fonts uses it
PKGS = x11 xft xrender fontconfig
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS := $(shell pkg-config --libs $(PKGS))

# The version for --version: the tag, or the commit when there is no tag
TINY_VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null)

# X headers are system headers: their warnings are not ours to fix
SYS_CFLAGS := $(subst -I,-isystem ,$(PKG_CFLAGS))

# -Os: small is the first goal. -fno-rtti: nothing uses dynamic_cast or typeid
# (the typeinfo of the classes was 15 kB of the binary).
# -fno-asynchronous-unwind-tables: exceptions work, only a debugger or a signal
# handler can't unwind the stack without them (it was 9.5 kB of the binary).
# The debugging information (-g) is for development: make DEBUG=1 (it makes
# the libraries and the programs several times bigger).
ifdef DEBUG
CXXFLAGS ?= -Og -g
else
CXXFLAGS ?= -Os -fno-asynchronous-unwind-tables
endif
override CXXFLAGS += -MMD -std=c++17 -fno-rtti -I./lib -isystem ./lib/third_party \
    $(SYS_CFLAGS) $(WARNINGS) \
    $(HARDENING)
LDLIBS = $(PKG_LIBS)

# Two libraries. libtiny-widgets.a is the widget toolkit (lib/*.cc, the
# pictures in lib/image/ and the vendored libraries): it knows nothing about
# windows managers, and the shell, the settings and the font viewer all use it.
# libtiny-shell.a has the window manager and the shell (lib/wm/), the
# freedesktop.org files (lib/xdg/) and the logic of the programs (lib/app/).
WIDGET_SRC = $(filter-out lib/third_party/%,$(wildcard lib/*.cc lib/image/*.cc))
SHELL_SRC = $(wildcard lib/wm/*.cc lib/xdg/*.cc lib/app/*.cc)
LIB_SRC = $(WIDGET_SRC) $(SHELL_SRC)
# the vendored libraries are built into the widgets, but not analyzed
VENDOR_SRC = $(wildcard lib/third_party/*.cc)
WIDGET_OBJ = $(WIDGET_SRC:%.cc=$(BUILD)/%.o) $(VENDOR_SRC:%.cc=$(BUILD)/%.o)
SHELL_OBJ = $(SHELL_SRC:%.cc=$(BUILD)/%.o)
LIB_OBJ = $(SHELL_OBJ) $(WIDGET_OBJ)
WIDGET_LIB = $(BUILD)/lib/libtiny-widgets.a
SHELL_LIB = $(BUILD)/lib/libtiny-shell.a
# the order matters: the shell uses the widgets
LIB = $(SHELL_LIB) $(WIDGET_LIB)

EXAMPLES = $(patsubst examples/%.cc,$(BUILD)/examples/%,$(wildcard examples/*.cc))
TARGET = $(BUILD)/tiny-shell
FONTS = $(BUILD)/tiny-fonts
SETTINGS = $(BUILD)/tiny-settings

PROGS = $(TARGET) $(FONTS) $(SETTINGS) $(EXAMPLES)
OBJ = $(LIB_OBJ) $(BUILD)/src/main.o $(BUILD)/src/fonts.o $(BUILD)/src/settings.o \
    $(addsuffix .o,$(EXAMPLES))
DEP = $(OBJ:.o=.d)

.PHONY: FORCE all release examples clean check coverage format lint lint-fast \
    lint-analyze asan cppcheck help layers
.DELETE_ON_ERROR:

all: $(TARGET) $(FONTS) $(SETTINGS)

# A small binary without the debugging information, into build-release.
# CI measures the size with -flto on top of this (see ci.yml).
RELEASE_DIR = build-release
release:
	$(MAKE) BUILD=$(RELEASE_DIR) CXXFLAGS="-Os -fno-asynchronous-unwind-tables -DNDEBUG -ffunction-sections \
	    -fdata-sections" \
	    LDFLAGS="-s -Wl,--gc-sections" all

help:
	@echo "make            build tiny-shell, tiny-fonts and tiny-settings"
	@echo "make layers     check that the widgets do not include the shell"
	@echo "make release    build small stripped programs into build-release"
	@echo "make examples   build the toolkit examples"
	@echo "make check      run the unit tests (against Xvfb)"
	@echo "make coverage   run the tests and print the coverage table"
	@echo "make asan       run the tests with AddressSanitizer and UBSan"
	@echo "make lint       static analysis with clang-tidy (lint-fast + lint-analyze)"
	@echo "make lint-fast  clang-tidy checks without the slow analyzer, all sources"
	@echo "make lint-analyze  clang-tidy analyzer (clang-analyzer-*), lib/ and src/"
	@echo "make cppcheck  second opinion on the sources (cppcheck)"
	@echo "make format     format the sources with clang-format"
	@echo "make clean      remove the build directories"
	@echo "Variables: CXX, CXXFLAGS, BUILD, DEBUG=1 (-Og -g), STRICT=1 (warnings are errors),"
	@echo "           HARDEN=1 (hardening flags for a release build)"

examples: $(EXAMPLES)

-include $(DEP)

# options.o is rebuilt when the version changes (the stamp changes only then)
ifneq (,$(TINY_VERSION))
$(BUILD)/lib/wm/options.o: override CXXFLAGS += -DTINY_SHELL_VERSION='"$(TINY_VERSION)"'
$(BUILD)/lib/wm/options.o: $(BUILD)/.version
endif

$(BUILD)/.version: FORCE
	@mkdir -p $(BUILD)
	@echo '$(TINY_VERSION)' | cmp -s - $@ || echo '$(TINY_VERSION)' > $@

FORCE:

$(BUILD)/%.o: %.cc | $(BUILD)/.pkg_check
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(WIDGET_LIB): $(WIDGET_OBJ)
	$(AR) rcs $@ $^

$(SHELL_LIB): $(SHELL_OBJ)
	$(AR) rcs $@ $^

# tiny-shell is built from src/main.cc, tiny-fonts from src/fonts.cc,
# tiny-settings from src/settings.cc, the examples from examples/<name>.cc
$(TARGET): $(BUILD)/src/main.o $(LIB)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(FONTS): $(BUILD)/src/fonts.o $(LIB)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(SETTINGS): $(BUILD)/src/settings.o $(LIB)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(EXAMPLES): %: %.o $(LIB)
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD)/.pkg_check: Makefile
	@mkdir -p $(BUILD)
	@printf "Checking packages $(PKGS)\t"
	@pkg-config --print-errors --exists $(PKGS) && printf "Ok\n"
	@touch $@

# Editor / language server configuration (generated, not in git)
.syntastic_cpp_config: $(BUILD)/.pkg_check
	@printf '%s\n' -I./lib -isystem ./lib/third_party $(PKG_CFLAGS) > $@

compile_flags.txt: $(BUILD)/.pkg_check
	@printf '%s\n' -xc++ -Wall -std=c++17 -I./lib -I./tests -isystem ./lib/third_party \
	    $(PKG_CFLAGS) > $@

# Tests (googletest), run with `make check`; they need an X server.
# Use XVFB_RUN= to run against the current $DISPLAY instead.
XVFB_RUN ?= xvfb-run -a

# googletest is a system header like Xlib: its warnings are not ours (it can be
# in /usr/pkg/include and the like, where the compiler does not know that)
GTEST_CFLAGS := $(subst -I,-isystem ,$(shell pkg-config --cflags gtest))

# tests/ mirrors lib/: tests/wm/test_dock.cc covers lib/wm/dock.cc
TEST_WIDGET_SRC = $(wildcard tests/test_*.cc tests/image/test_*.cc)
TEST_SHELL_SRC = $(wildcard tests/wm/test_*.cc tests/xdg/test_*.cc \
    tests/app/test_*.cc)
TEST_SRC = $(TEST_WIDGET_SRC) $(TEST_SHELL_SRC)
TESTS = $(TEST_SRC:%.cc=$(BUILD)/%)
TESTS_WIDGET = $(TEST_WIDGET_SRC:%.cc=$(BUILD)/%)
TESTS_SHELL = $(TEST_SHELL_SRC:%.cc=$(BUILD)/%)
TEST_OBJ = $(TEST_SRC:%.cc=$(BUILD)/%.o) $(BUILD)/tests/main.o
DEP += $(TEST_OBJ:.o=.d)

-include $(TEST_OBJ:.o=.d)

$(BUILD)/tests/%.o: override CXXFLAGS += $(GTEST_CFLAGS) -I./tests

# the tests of the widgets link the widgets only: they cannot need the shell
$(TESTS_WIDGET): %: %.o $(BUILD)/tests/main.o $(WIDGET_LIB)
	$(CXX) $^ $(LDFLAGS) $(shell pkg-config --libs gtest) $(LDLIBS) -o $@

$(TESTS_SHELL): %: %.o $(BUILD)/tests/main.o $(LIB)
	$(CXX) $^ $(LDFLAGS) $(shell pkg-config --libs gtest) $(LDLIBS) -o $@

# The widget toolkit must not include the shell (the layers of CONTRIBUTING.md)
layers:
	@! grep -nE '#include "(wm|xdg|app)/' lib/*.h lib/*.cc lib/image/*.h \
	    lib/image/*.cc || { echo "layers: the widgets include the shell" >&2; \
	    exit 1; }

check: layers $(BUILD)/.gtest_check $(TESTS)
	@rc=0; for t in $(TESTS); do \
	    echo "running $$t"; $(XVFB_RUN) $$t || rc=1; done; exit $$rc

$(BUILD)/.gtest_check: Makefile
	@mkdir -p $(BUILD)
	@printf "Checking packages gtest\t"
	@pkg-config --print-errors --exists gtest && printf "Ok\n"
	@touch $@

$(TESTS) $(TEST_OBJ): | $(BUILD)/.gtest_check

# Coverage is built into COV_DIR so it does not mix with normal objects.
# Needs xvfb-run and the gcov that matches the compiler: clang writes its own
# format, so llvm-cov is used for it (override with make coverage GCOV=...).
COV_DIR = build-cov
ifneq (,$(findstring clang,$(CXX_VERSION)))
GCOV ?= tools/run-tool.sh llvm-cov gcov
else
GCOV ?= gcov
endif

coverage:
	@rm -rf $(COV_DIR)
	@GTEST_BRIEF=1 $(MAKE) BUILD=$(COV_DIR) check -s \
	    CXXFLAGS="-g -O0 --coverage" LDFLAGS="--coverage"
	@for f in $(LIB_SRC); do $(GCOV) -n -o $(COV_DIR)/$$(dirname $$f) $$f; done 2>/dev/null | \
	    awk -F"'" '/^File/{f=$$2} /^Lines executed/{ \
	        if (f ~ /^lib\/([^\/]+\/)?[^\/]+\.cc$$/ && !(f in seen)) { \
	            seen[f]=1; split($$0, a, /[:% ]+/); \
	            n=a[5]; m=n-int(n*a[3]/100+0.5); \
	            names[++c]=f; stmts[c]=n; miss[c]=m; ts+=n; tm+=m}} \
	        END { \
	            line="--------------------------------------------------"; \
	            printf "\n%-32s %6s %6s %7s\n%s\n", "Name", "Stmts", "Miss", "Cover", line; \
	            for (i=1; i<=c; i++) \
	                printf "%-32s %6d %6d %6.0f%%\n", names[i], stmts[i], miss[i], \
	                    stmts[i] ? 100*(stmts[i]-miss[i])/stmts[i] : 100; \
	            printf "%s\n%-32s %6d %6d %6.0f%%\n", line, "TOTAL", ts, tm, \
	                ts ? 100*(ts-tm)/ts : 100}'

clean:
	@$(RM) -r $(BUILD) $(COV_DIR) $(ASAN_DIR) $(RELEASE_DIR)
	@$(RM) compile_flags.txt .syntastic_cpp_config

# Format the sources with clang-format (style in .clang-format).
# CLANG_FORMAT picks the tool, e.g. make format CLANG_FORMAT=clang-format-21.
CLANG_FORMAT ?= tools/run-tool.sh clang-format
FORMAT_FILES = $(filter-out lib/third_party/%,$(wildcard lib/*.cc lib/*.h \
    lib/*/*.cc lib/*/*.h)) $(wildcard src/*.cc examples/*.cc tests/*.cc \
    tests/*.h tests/*/*.cc)

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

# Static analysis with clang-tidy (checks in .clang-tidy); runs in parallel.
# CLANG_TIDY picks the tool, e.g. make lint CLANG_TIDY=clang-tidy-22.
CLANG_TIDY ?= tools/run-tool.sh clang-tidy
LINT_FLAGS = -std=c++17 -I./lib -isystem ./lib/third_party $(SYS_CFLAGS)
LINT_SRC = $(LIB_SRC) $(wildcard src/*.cc examples/*.cc)
# The path-sensitive analyzer (clang-analyzer-*) takes over 90 % of the time,
# and by far the most on the googletest macros, so it runs only on the library
# and the programs; every other check runs on all the sources.
ANALYZE_SRC = $(LIB_SRC) $(wildcard src/*.cc)

lint: lint-fast lint-analyze

lint-fast: $(BUILD)/.pkg_check $(BUILD)/.gtest_check
	@printf '%s\n' $(LINT_SRC) | xargs -P $$(nproc) -I{} \
	    $(CLANG_TIDY) --quiet {} -- $(LINT_FLAGS)
	@printf '%s\n' $(TEST_SRC) | xargs -P $$(nproc) -I{} \
	    $(CLANG_TIDY) --quiet {} -- $(LINT_FLAGS) \
	    $(GTEST_CFLAGS) -I./tests

lint-analyze: $(BUILD)/.pkg_check
	@printf '%s\n' $(ANALYZE_SRC) | xargs -P $$(nproc) -I{} \
	    $(CLANG_TIDY) --quiet '--checks=-*,clang-analyzer-*' {} -- $(LINT_FLAGS)

# A second opinion next to clang-tidy, part of CI and of the pre-push hook.
# The tests are left out (cppcheck does not know the googletest macros) and so
# are the examples (each one is a program of its own, the same class names
# look like a violation of the one definition rule to it). Suppressed on
# purpose, project wide:
#   useStlAlgorithm       raw loops are clearer for the readers learning Xlib
#   shadowFunction        names like `frame` and `error` are the natural ones
#   functionStatic        members stay members to keep the interface uniform
#   constParameterCallback  Xlib callbacks have a fixed signature
CPPCHECK ?= cppcheck
CPPCHECK_FLAGS = --enable=warning,style,performance,portability --std=c++17 \
    -I lib --inline-suppr --quiet --error-exitcode=1 \
    --suppress=useStlAlgorithm --suppress=shadowFunction \
    --suppress=functionStatic --suppress=constParameterCallback

cppcheck:
	$(CPPCHECK) $(CPPCHECK_FLAGS) lib src

# Run all the tests with AddressSanitizer and UndefinedBehaviorSanitizer, built
# into build-asan. Leaks that are accepted for now are in tools/lsan.supp.
ASAN_DIR = build-asan
SANITIZE = -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

SYMBOLIZER := $(firstword $(shell command -v llvm-symbolizer 2>/dev/null) \
    $(shell tools/newest.sh /usr/bin/llvm-symbolizer-[0-9]*))

asan:
	@ASAN_SYMBOLIZER_PATH=$(SYMBOLIZER) UBSAN_OPTIONS=print_stacktrace=1 \
	    ASAN_OPTIONS=fast_unwind_on_malloc=0 \
	    LSAN_OPTIONS=suppressions=$(CURDIR)/tools/lsan.supp:print_suppressions=0 \
	    $(MAKE) -j$$(nproc) BUILD=$(ASAN_DIR) check \
	    CXXFLAGS="-g -O1 $(SANITIZE)" LDFLAGS="$(SANITIZE)"
