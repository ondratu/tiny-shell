# Requires GNU make (wildcard, shell, pattern rules).
#
# Override from the command line or environment as usual, e.g.:
#   make CXX=clang++ CXXFLAGS="-O2 -g"
# Everything is built into $(BUILD) (build/ by default).

-include local/config.mk

BUILD ?= build

PKGS = x11 xft xrender
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS := $(shell pkg-config --libs $(PKGS))

CXXFLAGS ?= -g
override CXXFLAGS += -MMD -std=c++17 -I./lib $(PKG_CFLAGS)
LDLIBS = $(PKG_LIBS)

LIB_SRC = $(wildcard lib/*.cc)
LIB_OBJ = $(LIB_SRC:%.cc=$(BUILD)/%.o)
LIB = $(BUILD)/lib/libtiny.a

EXAMPLES = $(patsubst examples/%.cc,$(BUILD)/examples/%,$(wildcard examples/*.cc))
TARGET = $(BUILD)/tiny-shell

PROGS = $(TARGET) $(EXAMPLES)
OBJ = $(LIB_OBJ) $(BUILD)/src/main.o $(addsuffix .o,$(EXAMPLES))
DEP = $(OBJ:.o=.d)

.PHONY: all examples clean check coverage
.DELETE_ON_ERROR:

all: $(TARGET)

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
	@printf '%s\n' -I./lib $(PKG_CFLAGS) > $@

compile_flags.txt: $(BUILD)/.pkg_check
	@printf '%s\n' -xc++ -Wall -std=c++17 -I./lib $(PKG_CFLAGS) > $@

# Tests (googletest), run with `make check`; they need an X server.
# Use XVFB_RUN= to run against the current $DISPLAY instead.
XVFB_RUN ?= xvfb-run -a

TEST_SRC = $(wildcard tests/test-*.cc)
TESTS = $(TEST_SRC:%.cc=$(BUILD)/%)
TEST_OBJ = $(TEST_SRC:%.cc=$(BUILD)/%.o) $(BUILD)/tests/main.o
DEP += $(TEST_OBJ:.o=.d)

-include $(TEST_OBJ:.o=.d)

$(BUILD)/tests/%.o: override CXXFLAGS += $(shell pkg-config --cflags gtest)

$(BUILD)/tests/test-%: $(BUILD)/tests/test-%.o $(BUILD)/tests/main.o $(LIB)
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
# Needs gcc with gcov and xvfb-run.
COV_DIR = build-cov

coverage:
	@rm -rf $(COV_DIR)
	@GTEST_BRIEF=1 $(MAKE) BUILD=$(COV_DIR) check -s \
	    CXXFLAGS="-g -O0 --coverage" LDFLAGS="--coverage"
	@gcov -n -o $(COV_DIR)/lib $(LIB_SRC) 2>/dev/null | \
	    awk -F"'" '/^File/{f=$$2} /^Lines executed/{ \
	        if (f ~ /^lib\/[^\/]*\.cc$$/ && !(f in seen)) { \
	            seen[f]=1; split($$0, a, /[:% ]+/); \
	            n=a[5]; m=n-int(n*a[3]/100+0.5); \
	            names[++c]=f; stmts[c]=n; miss[c]=m; ts+=n; tm+=m}} \
	        END { \
	            line="--------------------------------------------------"; \
	            printf "\n%-24s %6s %6s %7s\n%s\n", "Name", "Stmts", "Miss", "Cover", line; \
	            for (i=1; i<=c; i++) \
	                printf "%-24s %6d %6d %6.0f%%\n", names[i], stmts[i], miss[i], \
	                    stmts[i] ? 100*(stmts[i]-miss[i])/stmts[i] : 100; \
	            printf "%s\n%-24s %6d %6d %6.0f%%\n", line, "TOTAL", ts, tm, \
	                ts ? 100*(ts-tm)/ts : 100}'

clean:
	@$(RM) -r $(BUILD) $(COV_DIR)
	@$(RM) compile_flags.txt .syntastic_cpp_config
