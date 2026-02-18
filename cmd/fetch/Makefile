.PHONY: build run run-live run-backtest clean test

build:
	go build -o fetch .

# Live trading mode: download latest strategies from GitHub Pages, then fetch 25 bars
run: run-live

run-live: build
	curl -sfL https://deanturpin.github.io/lft2/strategies.json \
	    | jq '{symbols: [.recommendations[].symbol]}' > /tmp/live-watchlist.json
	./fetch -watchlist /tmp/live-watchlist.json -bars 25 -output ../../docs/bars

# Backtest mode: fetch 1000 bars for full watchlist
run-backtest: build
	./fetch -watchlist ../../watchlist.json -bars 1000 -output ../../docs/bars

test:
	go test -v ./...

clean:
	rm -f fetch
	rm -rf ../../docs/bars/*.json ../../docs/bars/*.csv

fmt:
	go fmt ./...

lint:
	go vet ./...
