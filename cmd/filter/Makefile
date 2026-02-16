.PHONY: build run clean test

build:
	go build -o filter .

run: build
	./filter

test:
	go test -v ./...

clean:
	rm -f filter
	rm -f ../../docs/candidates.json

fmt:
	go fmt ./...

lint:
	go vet ./...
