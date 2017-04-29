TARGET = spectrogram
CPPFLAGS=-Wall
LDLIBS=-lsndfile -lpng
# SRCS=$(wildcard src/*.cpp)
SRCS=src/fft.cpp src/window_functions.cpp src/image_output.cpp src/spectrogram.cpp
OBJS=$(subst .cpp,.o,$(SRCS))

$(TARGET): $(OBJS)
	g++ -o spectrogram $(OBJS) $(LDLIBS)

%.o: %.cpp
	g++ $(CPPFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	rm -f src/*.o
	rm -f $(TARGET)