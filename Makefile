# Requires GNU make (wildcard, shell, pattern rules).
#
# Override from the command line or environment as usual, e.g.:
#   make CXX=clang++ CXXFLAGS="-O2 -g"

PKGS = x11 xft xrender
PKG_CFLAGS := $(shell pkg-config --cflags $(PKGS))
PKG_LIBS := $(shell pkg-config --libs $(PKGS))

CXXFLAGS ?= -g
override CXXFLAGS += -MMD -std=c++17 -I./lib $(PKG_CFLAGS)
LDLIBS = $(PKG_LIBS)

LIB_SRC = $(wildcard lib/*.cc)
LIB_OBJ = $(LIB_SRC:.cc=.o)
LIB_DEP = $(LIB_OBJ:.o=.d)

SRC = $(wildcard *.cc)
OBJ = $(SRC:.cc=.o)
DEP = $(OBJ:.o=.d)

TARGET = tiny-shell
PROGS = $(TARGET) demo input_only menu

.PHONY: all clean check coverage
.DELETE_ON_ERROR:

all: $(PROGS)

-include $(LIB_DEP) $(DEP)

lib/libtiny.a: $(LIB_OBJ)
	$(AR) rcs $@ $^

# tiny-shell is built from main.cc, the other programs from <name>.cc
$(TARGET): main.o lib/libtiny.a
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(filter-out $(TARGET),$(PROGS)): %: %.o lib/libtiny.a
	$(CXX) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(OBJ) $(LIB_OBJ): .pkg_check

.pkg_check: Makefile
	@printf "Checking packages $(PKGS)\t"
	@pkg-config --print-errors --exists $(PKGS) && printf "Ok\n"
	@touch $@

# Editor / language server configuration
.syntastic_cpp_config: .pkg_check
	@printf '%s\n' -I./lib $(PKG_CFLAGS) > $@

compile_flags.txt: .pkg_check
	@printf '%s\n' -xc++ -Wall -std=c++17 -I./lib $(PKG_CFLAGS) > $@

# Tests (googletest), run with `make check`; they need an X server.
# Use XVFB_RUN= to run against the current $DISPLAY instead.
XVFB_RUN ?= xvfb-run -a

TEST_SRC = $(wildcard tests/test-*.cc)
TESTS = $(TEST_SRC:.cc=)
TEST_OBJ = $(TEST_SRC:.cc=.o) tests/main.o
TEST_DEP = $(TEST_OBJ:.o=.d)

-include $(TEST_DEP)

tests/%.o: override CXXFLAGS += $(shell pkg-config --cflags gtest)

tests/test-%: tests/test-%.o tests/main.o lib/libtiny.a
	$(CXX) $^ $(LDFLAGS) $(shell pkg-config --libs gtest) $(LDLIBS) -o $@

check: .gtest_check $(TESTS)
	@rc=0; for t in $(TESTS); do \
	    echo "running $$t"; $(XVFB_RUN) ./$$t || rc=1; done; exit $$rc

.gtest_check: Makefile
	@printf "Checking packages gtest\t"
	@pkg-config --print-errors --exists gtest && printf "Ok\n"
	@touch $@

$(TESTS) $(TEST_OBJ): | .gtest_check

# Coverage is built out of tree in COV_DIR so it does not mix with normal objects.
# Needs gcc with gcov and xvfb-run.
COV_DIR = build-cov

coverage:
	@rm -rf $(COV_DIR) && mkdir -p $(COV_DIR)/lib $(COV_DIR)/tests
	@cp Makefile *.cc $(COV_DIR)/ && cp lib/*.cc lib/*.h $(COV_DIR)/lib/
	@cp tests/*.cc tests/*.h $(COV_DIR)/tests/
	@GTEST_BRIEF=1 $(MAKE) -C $(COV_DIR) check -s \
	    CXXFLAGS="-g -O0 --coverage" LDFLAGS="--coverage"
	@cd $(COV_DIR) && gcov -n -o lib $(notdir $(LIB_SRC)) 2>/dev/null | \
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
	@$(RM) .pkg_check $(OBJ) $(DEP) $(PROGS) lib/libtiny.a $(LIB_OBJ) $(LIB_DEP)
	@$(RM) .gtest_check $(TESTS) $(TEST_OBJ) $(TEST_DEP)
	@$(RM) -r $(COV_DIR)
