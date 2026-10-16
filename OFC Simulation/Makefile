# Makefile — build the 10G EPON + Server A discrete-event simulator.
# Usage: make | make run | make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc
SRC      := src/main.cpp
TARGET   := epon_sim

.PHONY: all clean run

all: $(TARGET)

# Header-only sources are pulled in via main.cpp; rebuild when any header changes.
$(TARGET): $(SRC) src/*.h
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) --N 8 --Nserver 5 --x 20 --y 100 --lambda 1365 --sim-time 2800

clean:
	rm -f $(TARGET) metrics_per_onu.csv metrics_network.csv
