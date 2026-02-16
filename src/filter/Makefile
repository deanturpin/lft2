.PHONY: build run clean

CXX := g++-15
CXXFLAGS := -std=c++26 -Wall -Wextra -O3

build: filter

filter: main.cxx
	$(CXX) $(CXXFLAGS) -o filter main.cxx

run: build
	./filter

clean:
	rm -f filter
	rm -f ../../docs/candidates.json
