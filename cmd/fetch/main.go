package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type Config struct {
	APIKey        string
	APISecret     string
	DataURL       string
	WatchlistFile string
	OutputDir     string
	BarsPerSymbol int
	TimeframeMin  int
}

type Watchlist struct {
	Symbols []string `json:"symbols"`
}

type AlpacaBar struct {
	Timestamp string  `json:"t"`
	Open      float64 `json:"o"`
	High      float64 `json:"h"`
	Low       float64 `json:"l"`
	Close     float64 `json:"c"`
	Volume    int64   `json:"v"`
}

type AlpacaBarsResponse struct {
	Bars          []AlpacaBar `json:"bars"`
	Symbol        string      `json:"symbol"`
	NextPageToken string      `json:"next_page_token,omitempty"`
}

type SymbolData struct {
	Symbol    string      `json:"symbol"`
	Bars      []AlpacaBar `json:"bars"`
	Count     int         `json:"count"`
	FetchedAt string      `json:"fetched_at"`
}

type FetchResult struct {
	Symbol string
	Count  int
	Error  error
}

func loadConfig() Config {
	cfg := Config{}
	flag.StringVar(&cfg.WatchlistFile, "watchlist", "watchlist.json", "Path to watchlist JSON file")
	flag.StringVar(&cfg.OutputDir, "output", "docs/bars", "Output directory for bar data")
	flag.IntVar(&cfg.BarsPerSymbol, "bars", 25, "Number of bars to fetch per symbol")
	flag.IntVar(&cfg.TimeframeMin, "timeframe", 5, "Timeframe in minutes")
	flag.Parse()

	cfg.APIKey = os.Getenv("ALPACA_API_KEY")
	cfg.APISecret = os.Getenv("ALPACA_API_SECRET")
	cfg.DataURL = os.Getenv("ALPACA_DATA_URL")

	if cfg.DataURL == "" {
		cfg.DataURL = "https://data.alpaca.markets"
	}

	if cfg.APIKey == "" || cfg.APISecret == "" {
		log.Fatal("ALPACA_API_KEY and ALPACA_API_SECRET environment variables required")
	}

	return cfg
}

func loadWatchlist(path string) (*Watchlist, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("reading watchlist: %w", err)
	}

	var watchlist Watchlist
	if err := json.Unmarshal(data, &watchlist); err != nil {
		return nil, fmt.Errorf("parsing watchlist: %w", err)
	}

	return &watchlist, nil
}

func fetchBars(cfg Config, symbol string) (*SymbolData, error) {
	// sort=desc + start 6 weeks back gives up to limit bars of recent history.
	// feed=iex is intentionally omitted — IEX only retains today's bars;
	// the default SIP feed provides weeks of history needed for backtesting.
	// Bars are reversed to ascending (oldest first) order before saving.
	start := time.Now().UTC().AddDate(0, 0, -42).Format(time.RFC3339)
	url := fmt.Sprintf("%s/v2/stocks/%s/bars?timeframe=%dMin&limit=%d&sort=desc&start=%s",
		cfg.DataURL,
		symbol,
		cfg.TimeframeMin,
		cfg.BarsPerSymbol,
		start,
	)

	req, err := NewAlpacaRequest("GET", url, cfg.APIKey, cfg.APISecret)
	if err != nil {
		return nil, fmt.Errorf("creating request: %w", err)
	}

	body, err := ExecuteRequest(req)
	if err != nil {
		return nil, fmt.Errorf("executing request: %w", err)
	}

	var response AlpacaBarsResponse
	if err := json.Unmarshal(body, &response); err != nil {
		return nil, fmt.Errorf("parsing response: %w", err)
	}

	if len(response.Bars) == 0 {
		return nil, fmt.Errorf("no bars returned")
	}

	bars := response.Bars
	if len(bars) > cfg.BarsPerSymbol {
		bars = bars[:cfg.BarsPerSymbol]
	}
	// Reverse from desc back to chronological (oldest first)
	for i, j := 0, len(bars)-1; i < j; i, j = i+1, j-1 {
		bars[i], bars[j] = bars[j], bars[i]
	}

	return &SymbolData{
		Symbol:    symbol,
		Bars:      bars,
		Count:     len(bars),
		FetchedAt: time.Now().UTC().Format(time.RFC3339),
	}, nil
}

func saveJSON(data *SymbolData, outputDir string) error {
	filename := filepath.Join(outputDir, fmt.Sprintf("%s.json", data.Symbol))

	file, err := os.Create(filename)
	if err != nil {
		return fmt.Errorf("creating file: %w", err)
	}
	defer file.Close()

	encoder := json.NewEncoder(file)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(data); err != nil {
		return fmt.Errorf("encoding JSON: %w", err)
	}

	return nil
}

func processSymbol(cfg Config, symbol string, resultChan chan<- FetchResult, wg *sync.WaitGroup) {
	defer wg.Done()

	data, err := fetchBars(cfg, symbol)
	if err != nil {
		resultChan <- FetchResult{Symbol: symbol, Error: err}
		return
	}

	if err := saveJSON(data, cfg.OutputDir); err != nil {
		resultChan <- FetchResult{Symbol: symbol, Error: fmt.Errorf("saving JSON: %w", err)}
		return
	}

	resultChan <- FetchResult{Symbol: symbol, Count: data.Count}
}

func main() {
	cfg := loadConfig()

	log.Printf("Loading watchlist from %s", cfg.WatchlistFile)
	watchlist, err := loadWatchlist(cfg.WatchlistFile)
	if err != nil {
		log.Fatalf("Failed to load watchlist: %v", err)
	}

	if len(watchlist.Symbols) == 0 {
		log.Fatal("No symbols in watchlist")
	}

	log.Printf("Creating output directory: %s", cfg.OutputDir)
	if err := os.MkdirAll(cfg.OutputDir, 0755); err != nil {
		log.Fatalf("Failed to create output directory: %v", err)
	}

	log.Printf("Fetching %d bars for %d symbols (timeframe: %dMin)",
		cfg.BarsPerSymbol, len(watchlist.Symbols), cfg.TimeframeMin)
	log.Println()

	var wg sync.WaitGroup
	resultChan := make(chan FetchResult, len(watchlist.Symbols))

	for _, symbol := range watchlist.Symbols {
		wg.Add(1)
		go processSymbol(cfg, symbol, resultChan, &wg)
	}

	go func() {
		wg.Wait()
		close(resultChan)
	}()

	successCount := 0
	failCount := 0

	for result := range resultChan {
		if result.Error != nil {
			log.Printf("✗ %s: %v", result.Symbol, result.Error)
			failCount++
		} else {
			log.Printf("✓ %s: %d bars", result.Symbol, result.Count)
			successCount++
		}
	}

	log.Println()
	log.Printf("Done! Success: %d, Failed: %d", successCount, failCount)
	log.Printf("Files saved to %s/", cfg.OutputDir)
}
