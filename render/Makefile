CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra
TARGET = renderer
SOURCES = main.cpp
HEADERS = math.h geometry.h material.h light.h camera.h scene.h integrator.h texture.h

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET) output.ppm

test: $(TARGET)
	./$(TARGET)

.PHONY: all clean test
