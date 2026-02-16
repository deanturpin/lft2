.PHONY: build run clean

CXX := g++-15
CXXFLAGS := -std=c++26 -Wall -Wextra -O3

build: backtest

backtest: main.cxx
	$(CXX) $(CXXFLAGS) -o backtest main.cxx

run: build
	./backtest

clean:
	rm -f backtest
	rm -f ../../docs/strategies.json
