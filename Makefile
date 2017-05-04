TARGET = spectrogram
CPPFLAGS=-Wall -std=c++14
LDLIBS=-lsndfile -lpng
INCLUDES=$(wildcard src/*.hpp)
SRC=src/spectrogram.cpp

$(TARGET): $(SRC) $(INCLUDES)
	g++ $(CPPFLAGS) -o spectrogram $(SRC) $(LDLIBS)

.PHONY: clean

clean:
	rm -f src/*.o
	rm -f $(TARGET)