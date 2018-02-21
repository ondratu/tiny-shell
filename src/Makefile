X11_CFLAGS = $(shell pkg-config --cflags x11)
X11_LIBS = $(shell pkg-config --libs x11)

CXXFLAGS = -MMD $(X11_CFLAGS)
LDFLAGS = -O2 $(X11_LIBS)

PKGS = x11

SRC = $(wildcard *.cc)
OBJ = $(SRC:%.cc=%.o)

TARGET = tiny-shell

$(TARGET): $(OBJ)
	$(CXX) $(LDFLAGS) $(OBJ) -o tiny-shell

-include $(OBJ:.o=.d)

$(OBJ): .pkg_check

.pkg_check: Makefile
	@$(foreach pkg, $(PKGS), printf "Checking packages $(pkg)\t" && \
	    pkg-config --print-errors --exists $(pkg) && printf "Ok\n" || exit 1;)
	@touch $@

clean:
	@$(RM) -v .pkg_check
	@$(RM) -v $(OBJ)
	@$(RM) -v $(TARGET)
