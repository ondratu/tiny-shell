X11_CFLAGS = $(shell pkg-config --cflags x11)
X11_LIBS = $(shell pkg-config --libs x11)

XFT_CFLAGS = $(shell pkg-config --cflags xft)
XFT_LIBS = $(shell pkg-config --libs xft)

XRENDER_CFLAGS = $(shell pkg-config --cflags xrender)
XRENDER_LIBS = $(shell pkg-config --libs xrender)

CXXFLAGS = -MMD -g -std=c++17 -I./lib $(X11_CFLAGS) $(XFT_CFLAGS) $(XRENDER_CFLAGS)
LDFLAGS = -O2 $(X11_LIBS) $(XFT_LIBS) $(XRENDER_LIBS)

PKGS = x11 xft xrender
# TODO: create _CFLAGS and _LIBS from PKGS

LIB_SRC = $(wildcard lib/*.cc)
LIB_OBJ = $(LIB_SRC:%.cc=%.o)
LIB_DEP = $(LIB_OBJ:%.o=%.d)

SRC = $(wildcard *.cc)
OBJ = $(SRC:%.cc=%.o)
DEP = $(OBJ:%.o=%.d)

# CXX=clang++-9

TARGET = tiny-shell

all: $(TARGET) demo input_only menu test_memory

-include $(LIB_OBJ:.o=.d)

lib/libtiny.a: $(LIB_OBJ)
	$(AR) rcs $@ $(LIB_OBJ)


$(TARGET): lib/libtiny.a main.o
	$(CXX) main.o -L./lib -ltiny -o tiny-shell $(LDFLAGS)

demo: lib/libtiny.a demo.o
	$(CXX) demo.o -L./lib -ltiny -o demo $(LDFLAGS)

input_only: lib/libtiny.a input_only.o
	$(CXX) input_only.o -L./lib -ltiny -o input_only $(LDFLAGS)

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

compile_flags.txt: .pkg_check
	@echo "-xc++" > $@
	@echo "-Wall" >> $@
	@echo "-std=c++17" >> $@
	@echo "-I./lib" >> $@
	@$(foreach pkg, $(PKGS), for i in $$(pkg-config --cflags $(pkg)); do echo $$i >> $@; done;)

# Coverage is built out of tree in COV_DIR so it does not mix with normal objects.
COV_DIR = build-cov
COV_TESTS = test_memory

coverage:
	@rm -rf $(COV_DIR) && mkdir -p $(COV_DIR)/lib
	@cp Makefile *.cc $(COV_DIR)/ && cp lib/*.cc lib/*.h $(COV_DIR)/lib/
	@$(MAKE) -C $(COV_DIR) $(COV_TESTS) -s \
	    CXXFLAGS="-g -O0 --coverage -std=c++17 -I./lib $(X11_CFLAGS) $(XFT_CFLAGS) $(XRENDER_CFLAGS)" \
	    LDFLAGS="--coverage $(X11_LIBS) $(XFT_LIBS) $(XRENDER_LIBS)"
	@cd $(COV_DIR) && for t in $(COV_TESTS); do \
	    xvfb-run -a ./$$t || exit 1; done
	@cd $(COV_DIR) && gcov -n -o lib $(notdir $(LIB_SRC)) 2>/dev/null | \
	    awk -F"'" '/^File/{f=$$2} /^Lines executed/{ \
	        if (f ~ /^lib\/[^\/]*\.cc$$/ && !(f in seen)) {seen[f]=1; print f ": " $$0}}'

clean:
	@$(RM) -v .pkg_check
	@$(RM) -v $(OBJ)
	@$(RM) -v $(DEP)
	@$(RM) -v $(TARGET)
	@$(RM) -v lib/libtiny.a
	@$(RM) -v $(LIB_OBJ)
	@$(RM) -v $(LIB_DEP)
	@$(RM) -r $(COV_DIR)
