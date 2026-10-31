# Requires GNU make (wildcard, shell, pattern rules).
#
# Override from the command line or environment as usual, e.g.:
#   make CXX=clang++ CXXFLAGS="-O2 -g"
# Everything is built into $(BUILD) (build/ by default).

-include local/config.mk

BUILD ?= build

# Clang is stricter, so it is the default compiler when it is installed (the
# newest versioned one if there is no plain clang++); g++ otherwise.
# Set CXX to choose another one.
ifeq ($(origin CXX),default)
CLANGXX := $(firstword $(shell command -v clang++ 2>/dev/null) \
    $(shell ls /usr/bin/clang++-[0-9]* 2>/dev/null | sort -V | tail -n 1))
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

PKGS = x11 xft xrender
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS := $(shell pkg-config --libs $(PKGS))

# X headers are system headers: their warnings are not ours to fix
SYS_CFLAGS := $(subst -I,-isystem ,$(PKG_CFLAGS))

CXXFLAGS ?= -g
override CXXFLAGS += -MMD -std=c++17 -I./lib -isystem ./lib/third_party \
    $(SYS_CFLAGS) $(WARNINGS) \
    $(HARDENING)
LDLIBS = $(PKG_LIBS)

# the library has a directory for each part: lib/wm/, lib/xdg/, lib/image/
LIB_SRC = $(filter-out lib/third_party/%,$(wildcard lib/*.cc lib/*/*.cc))
# the vendored libraries are built into the library, but not analyzed
VENDOR_SRC = $(wildcard lib/third_party/*.cc)
LIB_OBJ = $(LIB_SRC:%.cc=$(BUILD)/%.o) $(VENDOR_SRC:%.cc=$(BUILD)/%.o)
LIB = $(BUILD)/lib/libtiny.a

EXAMPLES = $(patsubst examples/%.cc,$(BUILD)/examples/%,$(wildcard examples/*.cc))
TARGET = $(BUILD)/tiny-shell

PROGS = $(TARGET) $(EXAMPLES)
OBJ = $(LIB_OBJ) $(BUILD)/src/main.o $(addsuffix .o,$(EXAMPLES))
DEP = $(OBJ:.o=.d)

.PHONY: all examples clean check coverage format lint asan cppcheck help
.DELETE_ON_ERROR:

all: $(TARGET)

help:
	@echo "make            build build/tiny-shell"
	@echo "make examples   build the toolkit examples"
	@echo "make check      run the unit tests (against Xvfb)"
	@echo "make coverage   run the tests and print the coverage table"
	@echo "make asan       run the tests with AddressSanitizer and UBSan"
	@echo "make lint       static analysis with clang-tidy"
	@echo "make cppcheck  second opinion on the sources (only reports, not in CI)"
	@echo "make format     format the sources with clang-format"
	@echo "make clean      remove the build directories"
	@echo "Variables: CXX, CXXFLAGS, BUILD, STRICT=1 (warnings are errors),"
	@echo "           HARDEN=1 (hardening flags for a release build)"

examples: $(EXAMPLES)

-include $(DEP)

$(BUILD)/%.o: %.cc | $(BUILD)/.pkg_check
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

# tiny-shell is built from src/main.cc, the examples from examples/<name>.cc
$(TARGET): $(BUILD)/src/main.o $(LIB)
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
TEST_SRC = $(wildcard tests/test_*.cc tests/*/test_*.cc)
TESTS = $(TEST_SRC:%.cc=$(BUILD)/%)
TEST_OBJ = $(TEST_SRC:%.cc=$(BUILD)/%.o) $(BUILD)/tests/main.o
DEP += $(TEST_OBJ:.o=.d)

-include $(TEST_OBJ:.o=.d)

$(BUILD)/tests/%.o: override CXXFLAGS += $(GTEST_CFLAGS) -I./tests

$(TESTS): %: %.o $(BUILD)/tests/main.o $(LIB)
	$(CXX) $^ $(LDFLAGS) $(shell pkg-config --libs gtest) $(LDLIBS) -o $@

check: $(BUILD)/.gtest_check $(TESTS)
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
	@$(RM) -r $(BUILD) $(COV_DIR) $(ASAN_DIR)
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

lint: $(BUILD)/.pkg_check $(BUILD)/.gtest_check
	@printf '%s\n' $(LINT_SRC) | xargs -P $$(nproc) -I{} \
	    $(CLANG_TIDY) --quiet {} -- $(LINT_FLAGS)
	@printf '%s\n' $(TEST_SRC) | xargs -P $$(nproc) -I{} \
	    $(CLANG_TIDY) --quiet {} -- $(LINT_FLAGS) $(GTEST_CFLAGS) -I./tests

# A second opinion next to clang-tidy. It only reports: the findings are not
# triaged yet, so it is not a part of CI or the git hooks. The tests are left
# out (cppcheck does not know the googletest macros).
CPPCHECK ?= cppcheck
CPPCHECK_FLAGS = --enable=warning,style,performance,portability --std=c++17 \
    -I lib --inline-suppr --quiet

cppcheck:
	$(CPPCHECK) $(CPPCHECK_FLAGS) lib src examples

# Run all the tests with AddressSanitizer and UndefinedBehaviorSanitizer, built
# into build-asan. Leaks that are accepted for now are in tools/lsan.supp.
ASAN_DIR = build-asan
SANITIZE = -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer

SYMBOLIZER := $(firstword $(shell command -v llvm-symbolizer 2>/dev/null) \
    $(shell ls /usr/bin/llvm-symbolizer-[0-9]* 2>/dev/null | sort -V | tail -n 1))

asan:
	@ASAN_SYMBOLIZER_PATH=$(SYMBOLIZER) UBSAN_OPTIONS=print_stacktrace=1 \
	    ASAN_OPTIONS=fast_unwind_on_malloc=0 \
	    LSAN_OPTIONS=suppressions=$(CURDIR)/tools/lsan.supp:print_suppressions=0 \
	    $(MAKE) BUILD=$(ASAN_DIR) check \
	    CXXFLAGS="-g -O1 $(SANITIZE)" LDFLAGS="$(SANITIZE)"
