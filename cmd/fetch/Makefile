.PHONY: build run run-live run-backtest clean test

build:
	go build -o fetch .

# Live trading mode: fetch latest 25 bars for strategy candidates
run: run-live

run-live: build
	./fetch -live -strategies ../../docs/strategies.json -output ../../docs/bars

# Backtest mode: fetch 1000 bars for watchlist symbols
run-backtest: build
	./fetch -watchlist ../../watchlist.json -output ../../docs/bars

test:
	go test -v ./...

clean:
	rm -f fetch
	rm -rf ../../docs/bars/*.json ../../docs/bars/*.csv

fmt:
	go fmt ./...

lint:
	go vet ./...
