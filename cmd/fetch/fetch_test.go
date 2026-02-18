package main

import (
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

// --- loadWatchlist ---

func TestLoadWatchlist_Valid(t *testing.T) {
	f := writeTemp(t, `{"symbols":["AAPL","MSFT","NVDA"]}`)
	wl, err := loadWatchlist(f)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(wl.Symbols) != 3 {
		t.Errorf("got %d symbols, want 3", len(wl.Symbols))
	}
	if wl.Symbols[0] != "AAPL" {
		t.Errorf("got %q, want AAPL", wl.Symbols[0])
	}
}

func TestLoadWatchlist_Missing(t *testing.T) {
	_, err := loadWatchlist("/nonexistent/path/watchlist.json")
	if err == nil {
		t.Error("expected error for missing file, got nil")
	}
}

func TestLoadWatchlist_InvalidJSON(t *testing.T) {
	f := writeTemp(t, `not json`)
	_, err := loadWatchlist(f)
	if err == nil {
		t.Error("expected error for invalid JSON, got nil")
	}
}

func TestLoadWatchlist_Empty(t *testing.T) {
	f := writeTemp(t, `{"symbols":[]}`)
	wl, err := loadWatchlist(f)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(wl.Symbols) != 0 {
		t.Errorf("got %d symbols, want 0", len(wl.Symbols))
	}
}

// --- bar reversal (desc → asc) ---

func TestBarsReversed(t *testing.T) {
	// Simulate the reversal applied inside fetchBars after receiving desc-sorted bars.
	bars := []AlpacaBar{
		{Timestamp: "2024-01-03"},
		{Timestamp: "2024-01-02"},
		{Timestamp: "2024-01-01"},
	}
	for i, j := 0, len(bars)-1; i < j; i, j = i+1, j-1 {
		bars[i], bars[j] = bars[j], bars[i]
	}
	if bars[0].Timestamp != "2024-01-01" {
		t.Errorf("after reversal bars[0]=%q, want 2024-01-01", bars[0].Timestamp)
	}
	if bars[2].Timestamp != "2024-01-03" {
		t.Errorf("after reversal bars[2]=%q, want 2024-01-03", bars[2].Timestamp)
	}
}

func TestBarsReversed_Single(t *testing.T) {
	bars := []AlpacaBar{{Timestamp: "2024-01-01"}}
	for i, j := 0, len(bars)-1; i < j; i, j = i+1, j-1 {
		bars[i], bars[j] = bars[j], bars[i]
	}
	if bars[0].Timestamp != "2024-01-01" {
		t.Errorf("single-bar reversal changed value: %q", bars[0].Timestamp)
	}
}

// --- saveJSON ---

func TestSaveJSON(t *testing.T) {
	dir := t.TempDir()
	data := &SymbolData{
		Symbol:    "AAPL",
		Bars:      []AlpacaBar{{Timestamp: "2024-01-01", Close: 180.0}},
		Count:     1,
		FetchedAt: "2024-01-01T00:00:00Z",
	}
	if err := saveJSON(data, dir); err != nil {
		t.Fatalf("saveJSON error: %v", err)
	}

	raw, err := os.ReadFile(filepath.Join(dir, "AAPL.json"))
	if err != nil {
		t.Fatalf("reading saved file: %v", err)
	}

	var got SymbolData
	if err := json.Unmarshal(raw, &got); err != nil {
		t.Fatalf("parsing saved JSON: %v", err)
	}
	if got.Symbol != "AAPL" {
		t.Errorf("symbol: got %q, want AAPL", got.Symbol)
	}
	if got.Count != 1 {
		t.Errorf("count: got %d, want 1", got.Count)
	}
	if got.Bars[0].Close != 180.0 {
		t.Errorf("close: got %.2f, want 180.00", got.Bars[0].Close)
	}
}

// writeTemp writes content to a temporary file and returns its path.
func writeTemp(t *testing.T, content string) string {
	t.Helper()
	f, err := os.CreateTemp(t.TempDir(), "*.json")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.WriteString(content); err != nil {
		t.Fatal(err)
	}
	f.Close()
	return f.Name()
}
