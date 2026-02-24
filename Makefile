# ============================================================
# LFT2 Build System
#
# Two distinct roles:
#   cmake  - compiles C++ strategy modules (exits, entries, backtest, etc.)
#   make   - sequences Go and C++ modules in the correct order
#
# Live trading loop:  make run      (requires API credentials)
# Backtest pipeline:  make backtest (requires API credentials, run manually)
# ============================================================

BUILD_DIR := build
BACKTEST  := $(BUILD_DIR)/backtest
PROFILE   := $(BUILD_DIR)/profile
EXITS     := $(BUILD_DIR)/exits
ENTRIES   := $(BUILD_DIR)/entries

.PHONY: all build run profile clean \
        fetch-go filter-go backtest-cpp help

# Default: compile then run live trading loop
all: run

# ============================================================
# cmake: compile all C++ modules into build/
# ============================================================
build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CXX_COMPILER=$(GCXX)
	cmake --build $(BUILD_DIR) -j

# g++-15 required for C++26; override: GCXX=g++ make build
GCXX ?= g++-15

# ============================================================
# GNU make: full pipeline — runs every 5-minute bar
#   fetch    - get latest bars for watchlist → docs/bars/
#   filter   - score and rank candidates → docs/candidates.json
#   backtest - run C++ strategies → docs/strategies.json
#   account  - fetch cash balance and positions from Alpaca
#   entries  - evaluate entry signals → buy.fix (skips symbols already held)
#   exits    - check open positions for exit signals → sell.fix
#   execute  - submit buy.fix and sell.fix orders to Alpaca
# ============================================================
run: build
	@echo "=== LFT2 pipeline ==="
	@echo ""
	@echo "→ fetch"
	@cd cmd/fetch && go build -o ../../bin/fetch . && cd ../.. && ./bin/fetch
	@echo ""
	@echo "→ filter"
	@cd cmd/filter && go build -o ../../bin/filter . && cd ../.. && ./bin/filter
	@echo ""
	@echo "→ backtest"
	@./$(BACKTEST)
	@echo ""
	@echo "→ account"
	@cd cmd/account && go build -o ../../bin/account . && cd ../.. && ./bin/account
	@echo ""
	@echo "→ entries"
	@./$(ENTRIES)
	@echo ""
	@echo "→ exits"
	@./$(EXITS)
	@echo ""
	@echo "→ execute"
	@cd cmd/execute && go build -o ../../bin/execute . && cd ../.. && ./bin/execute
	@echo ""
	@echo "→ summary"
	@cd cmd/summary && go build -o ../../bin/summary . && cd ../.. && ./bin/summary
	@echo ""
	@echo "=== done ==="
	@echo "{\"timestamp\": \"$$(date -u +%Y-%m-%dT%H:%M:%SZ)\"}" > docs/pipeline-metadata.json
	@echo '{'                                                                        > docs/tech-stack.json
	@echo "  \"go\":     \"$$(go version | cut -d' ' -f3)\","                      >> docs/tech-stack.json
	@echo "  \"gcc\":    \"$$(g++ --version | head -1 | awk '{print $$NF}')\"," >> docs/tech-stack.json
	@echo "  \"cmake\":  \"$$(cmake --version | head -1 | awk '{print $$3}')\"," >> docs/tech-stack.json
	@echo "  \"make\":   \"$$(make --version | head -1 | awk '{print $$3}')\"," >> docs/tech-stack.json
	@echo "  \"os\":     \"$$(lsb_release -d 2>/dev/null | cut -f2 || uname -s)\"," >> docs/tech-stack.json
	@echo "  \"kernel\": \"$$(uname -r)\"" >> docs/tech-stack.json
	@echo '}'                                                                       >> docs/tech-stack.json
	@if [ "$$(uname -s)" = "Linux" ]; then \
	    lcov --capture --directory $(BUILD_DIR) --output-file docs/coverage.info \
	         --gcov-tool gcov-15 --ignore-errors mismatch \
	    && lcov --remove docs/coverage.info '/usr/include/*' --output-file docs/coverage.info \
	    && echo "→ captured coverage data" \
	    || echo "→ warning: lcov capture failed"; \
	    if [ -s docs/coverage.info ]; then \
	        genhtml docs/coverage.info --output-directory docs/coverage \
	                --title "LFT2 Coverage" --quiet \
	                --ignore-errors inconsistent,corrupt \
	        && echo "→ wrote docs/coverage/" \
	        || echo "→ warning: genhtml failed"; \
	    else \
	        echo "→ skipping coverage (no coverage data)"; \
	    fi; \
	    if [ -s gmon.out ]; then \
	        gprof $(PROFILE) gmon.out \
	        | gprof2dot -f prof -n 10 -e 10 \
	        | dot -Tsvg -o docs/callgraph.svg \
	        && echo "→ wrote docs/callgraph.svg" \
	        || echo "→ callgraph failed"; \
	    else \
	        echo "→ skipping callgraph (no gmon.out)"; \
	    fi; \
	else \
	    echo "→ skipping coverage and callgraph (Linux only)"; \
	fi

# ============================================================
# GNU make: backtest pipeline (module sequencing)
# Run manually to regenerate strategies.json and GitHub Pages data.
# Not triggered on push - use scheduled CI or run locally.
#   fetch    - fetch 1000 bars per watchlist symbol → docs/bars/
#   filter   - score and rank candidates → docs/strategies.json
#   backtest - run C++ strategies and write results → docs/
# ============================================================
backtest: fetch-go filter-go backtest-cpp
	@echo ""
	@echo "=== backtest complete ==="

fetch-go:
	@echo "→ fetch"
	@cd cmd/fetch && go build -o ../../bin/fetch . && cd ../.. && ./bin/fetch

filter-go:
	@echo "→ filter"
	@cd cmd/filter && go build -o ../../bin/filter . && cd ../.. && ./bin/filter

backtest-cpp: build
	@echo "→ backtest"
	@./$(BACKTEST)

# Run the profile binary (instrumented with -pg/--coverage) to generate:
#   docs/coverage/       - gcov HTML coverage report (lcov + genhtml)
#   docs/callgraph.svg   - gprof call graph as SVG (Linux only, via gprof2dot + dot)
profile: build
	@echo "→ profile"
	@./$(PROFILE) > /dev/null
	@if [ "$$(uname -s)" = "Linux" ]; then \
	    lcov --capture --directory $(BUILD_DIR) --output-file docs/coverage.info \
	         --gcov-tool gcov-15 --ignore-errors mismatch \
	    && lcov --remove docs/coverage.info '/usr/include/*' --output-file docs/coverage.info \
	    && echo "→ captured coverage data" \
	    || echo "→ warning: lcov capture failed"; \
	    if [ -s docs/coverage.info ]; then \
	        genhtml docs/coverage.info --output-directory docs/coverage \
	                --title "LFT2 Coverage" --quiet \
	                --ignore-errors inconsistent,corrupt \
	        && echo "→ wrote docs/coverage/" \
	        || echo "→ warning: genhtml failed"; \
	    else \
	        echo "→ skipping coverage (no coverage data)"; \
	    fi; \
	    if [ -s gmon.out ]; then \
	        gprof $(PROFILE) gmon.out \
	        | gprof2dot -f prof -n 10 -e 10 \
	        | dot -Tsvg -o docs/callgraph.svg \
	        && echo "→ wrote docs/callgraph.svg" \
	        || echo "→ callgraph failed"; \
	    else \
	        echo "→ skipping callgraph (no gmon.out)"; \
	    fi; \
	else \
	    echo "→ skipping coverage and callgraph (Linux only)"; \
	fi

# ============================================================
# Housekeeping
# ============================================================
clean:
	rm -rf $(BUILD_DIR) bin/
	rm -rf docs/bars/
	rm -f docs/candidates.json docs/strategies.json

help:
	@echo "LFT2 Build System"
	@echo ""
	@echo "  make        - compile and run full pipeline (fetch → filter → backtest → entries → execute)"
	@echo "  make build  - cmake: compile C++ modules only"
	@echo "  make clean  - remove all build artefacts and fetched data"
