X11_CFLAGS = $(shell pkg-config --cflags x11)
X11_LIBS = $(shell pkg-config --libs x11)

XFT_CFLAGS = $(shell pkg-config --cflags xft)
XFT_LIBS = $(shell pkg-config --libs xft)

CXXFLAGS = -MMD -g -std=c++11 -I./lib $(X11_CFLAGS) $(XFT_CFLAGS)
LDFLAGS = -O2 $(X11_LIBS) $(XFT_LIBS)

PKGS = x11 xft
# TODO: create _CFLAGS and _LIBS from PKGS

LIB_SRC = $(wildcard lib/*.cc)
LIB_OBJ = $(LIB_SRC:%.cc=%.o)
LIB_DEP = $(LIB_OBJ:%.o=%.d)

SRC = $(wildcard *.cc)
OBJ = $(SRC:%.cc=%.o)
DEP = $(OBJ:%.o=%.d)

# CXX=clang++-9

TARGET = tiny-shell

all: $(TARGET)

-include $(LIB_OBJ:.o=.d)

lib/libtiny.a: $(LIB_OBJ)
	$(AR) rcs $@ $(LIB_OBJ)


$(TARGET): lib/libtiny.a main.o
	$(CXX) main.o -L./lib -ltiny -o tiny-shell $(LDFLAGS)

demo: lib/libtiny.a demo.o
	$(CXX) demo.o -L./lib -ltiny -o demo $(LDFLAGS)

input_only: lib/libtiny.a input_only.o
	$(CXX) input_only.o -L./lib -ltiny -o input_only $(LDFLAGS)

wm_test: lib/libtiny.a wm_test.o
	$(CXX) wm_test.o -L./lib -ltiny -o wm_test $(LDFLAGS)

menu: lib/libtiny.a menu.o
	$(CXX) menu.o -L./lib -ltiny -o menu $(LDFLAGS)

test_memory: lib/libtiny.a test_memory.o
	$(CXX) test_memory.o -L./lib -ltiny -o test_memory $(LDFLAGS)


$(OBJ): .pkg_check
$(LIB_OBJ): .pkg_check

.pkg_check: Makefile
	@$(foreach pkg, $(PKGS), printf "Checking packages $(pkg)\t" && \
	    pkg-config --print-errors --exists $(pkg) && printf "Ok\n" || exit 1;)
	@touch $@

.syntastic_cpp_config: .pkg_check
	@echo "-I./lib" > $@
	@$(foreach pkg, $(PKGS), for i in $$(pkg-config --cflags $(pkg)); do echo $$i >> $@; done;)

clean:
	@$(RM) -v .pkg_check
	@$(RM) -v $(OBJ)
	@$(RM) -v $(DEP)
	@$(RM) -v $(TARGET)
	@$(RM) -v lib/libtiny.a
	@$(RM) -v $(LIB_OBJ)
	@$(RM) -v $(LIB_DEP)
