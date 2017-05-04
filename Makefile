TARGET = spectrogram
CPPFLAGS=-Wall
LDLIBS=-lsndfile -lpng
INCLUDES=$(wildcard src/*.hpp)
SRC=src/spectrogram.cpp

$(TARGET): $(SRC) $(INCLUDES)
	g++ -o spectrogram $(SRC) $(LDLIBS)

.PHONY: clean

clean:
	rm -f src/*.o
	rm -f $(TARGET)