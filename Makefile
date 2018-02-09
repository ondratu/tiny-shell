CXXFLAGS = `pkg-config --cflags x11`
LDFLAGS = `pkg-config --libs x11`

SRC = $(wildcard *.cc)
OBJ = $(SRC:%.cc=%.o)

tiny-shell: $(OBJ)
	$(CXX) $(LDFLAGS) $(OBJ) -o tiny-shell

help:
	@echo $(SRC_CC)
	@echo $(SRC)
	@echo $(OBJ)

clean:
	$(RM) -v $(OBJ)
	$(RM) -v tiny-shell
