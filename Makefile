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
PROGS = $(TARGET) demo input_only menu test_memory

.PHONY: all clean coverage
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

# Coverage is built out of tree in COV_DIR so it does not mix with normal objects.
# Needs gcc with gcov and xvfb-run.
COV_DIR = build-cov
COV_TESTS = test_memory

coverage:
	@rm -rf $(COV_DIR) && mkdir -p $(COV_DIR)/lib
	@cp Makefile *.cc $(COV_DIR)/ && cp lib/*.cc lib/*.h $(COV_DIR)/lib/
	@$(MAKE) -C $(COV_DIR) $(COV_TESTS) -s \
	    CXXFLAGS="-g -O0 --coverage" LDFLAGS="--coverage"
	@cd $(COV_DIR) && for t in $(COV_TESTS); do \
	    xvfb-run -a ./$$t || exit 1; done
	@cd $(COV_DIR) && gcov -n -o lib $(notdir $(LIB_SRC)) 2>/dev/null | \
	    awk -F"'" '/^File/{f=$$2} /^Lines executed/{ \
	        if (f ~ /^lib\/[^\/]*\.cc$$/ && !(f in seen)) {seen[f]=1; print f ": " $$0}}'

clean:
	@$(RM) .pkg_check $(OBJ) $(DEP) $(PROGS) lib/libtiny.a $(LIB_OBJ) $(LIB_DEP)
	@$(RM) -r $(COV_DIR)
