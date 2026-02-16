.PHONY: build run clean test

build:
	go build -o fetch .

run: build
	./fetch -watchlist ../watchlist.json -output ../public/bars

test:
	go test -v ./...

clean:
	rm -f fetch
	rm -rf ../public/bars/*.json ../public/bars/*.csv

fmt:
	go fmt ./...

lint:
	go vet ./...
