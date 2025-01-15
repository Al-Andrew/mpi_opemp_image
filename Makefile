
CXX      := mpic++
SRCDIR   := src/
OUTDIR   := bin/
TARGET   := img_manip

CXXFLAGS := -std=c++20 -Wall -Wextra -Werror -pedantic -O3 -ggdb -fopenmp -Wno-cast-function-type
LDFLAGS  := 

SOURCES = src/main.cpp   \
		  src/blur.cpp   \
		  src/hflip.cpp  \
		  src/invert.cpp \


HEADERS = src/stb_image.h       \
		  src/stb_image_write.h \
		  src/util.hpp          \
		  src/operations.hpp    \


.PHONY: build
build: $(OUTDIR)$(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)$(TARGET): $(OUTDIR) $(SOURCES) $(FKSOURCES) $(HEADERS) $(FKHEADERS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(SOURCES) $(FKSOURCES) -o $(OUTDIR)$(TARGET)

.PHONY: clean
clean:
	rm -rf $(OUTDIR)