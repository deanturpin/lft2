.PHONY: build run clean test

build:
	go build -o fetch .

run: build
	./fetch -watchlist ../watchlist.json -output ../docs/bars

test:
	go test -v ./...

clean:
	rm -f fetch
	rm -rf ../docs/bars/*.json ../docs/bars/*.csv

fmt:
	go fmt ./...

lint:
	go vet ./...
