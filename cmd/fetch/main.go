package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
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
	StrategyFile  string
	OutputDir     string
	BarsPerSymbol int
	TimeframeMin  int
	LiveMode      bool
}

type Watchlist struct {
	Symbols []string `json:"symbols"`
}

type StrategyRecommendation struct {
	Symbol   string  `json:"symbol"`
	Strategy string  `json:"strategy"`
	WinRate  float64 `json:"win_rate"`
}

type StrategyFile struct {
	Recommendations []StrategyRecommendation `json:"recommendations"`
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
	Bars      []AlpacaBar `json:"bars"`
	Symbol    string      `json:"symbol"`
	NextPageToken string  `json:"next_page_token,omitempty"`
}

type SymbolData struct {
	Symbol string       `json:"symbol"`
	Bars   []AlpacaBar  `json:"bars"`
	Count  int          `json:"count"`
	FetchedAt string    `json:"fetched_at"`
}

type FetchResult struct {
	Symbol string
	Count  int
	Error  error
}

func loadConfig() Config {
	cfg := Config{}
	flag.StringVar(&cfg.WatchlistFile, "watchlist", "watchlist.json", "Path to watchlist JSON file")
	flag.StringVar(&cfg.StrategyFile, "strategies", "https://deanturpin.github.io/lft2/strategies.json", "URL of strategies JSON")
	flag.StringVar(&cfg.OutputDir, "output", "docs/bars", "Output directory for bar data")
	flag.IntVar(&cfg.BarsPerSymbol, "bars", 1000, "Number of bars to fetch per symbol")
	flag.IntVar(&cfg.TimeframeMin, "timeframe", 5, "Timeframe in minutes")
	flag.BoolVar(&cfg.LiveMode, "live", false, "Live trading mode (fetch latest bars for strategy candidates)")
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

	// In live mode, fetch fewer bars and use strategies file
	if cfg.LiveMode {
		cfg.BarsPerSymbol = 25  // Latest 25 bars for live trading
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

const pagesBase = "https://deanturpin.github.io/lft2"

// downloadFile fetches a URL and writes it to a local path.
func downloadFile(url, dest string) error {
	resp, err := http.Get(url)
	if err != nil {
		return fmt.Errorf("fetching %s: %w", url, err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return fmt.Errorf("fetching %s: HTTP %d", url, resp.StatusCode)
	}

	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return fmt.Errorf("reading %s: %w", url, err)
	}

	if err := os.WriteFile(dest, data, 0644); err != nil {
		return fmt.Errorf("writing %s: %w", dest, err)
	}

	return nil
}

func loadStrategies(url string) ([]string, error) {
	resp, err := http.Get(url)
	if err != nil {
		return nil, fmt.Errorf("fetching strategies: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("fetching strategies: HTTP %d", resp.StatusCode)
	}

	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("reading strategies response: %w", err)
	}

	var stratFile StrategyFile
	if err := json.Unmarshal(data, &stratFile); err != nil {
		return nil, fmt.Errorf("parsing strategies: %w", err)
	}

	symbols := make([]string, 0, len(stratFile.Recommendations))
	for _, rec := range stratFile.Recommendations {
		symbols = append(symbols, rec.Symbol)
	}

	return symbols, nil
}

func fetchBars(cfg Config, symbol string) (*SymbolData, error) {
	// Calculate date range to fetch approximately the requested number of bars
	// 5-minute bars: ~78 per day (market hours only)
	// Request extra days to account for weekends and holidays
	daysNeeded := (cfg.BarsPerSymbol / 78) + 10
	endDate := time.Now().UTC()
	startDate := endDate.AddDate(0, 0, -daysNeeded)

	url := fmt.Sprintf("%s/v2/stocks/%s/bars?timeframe=%dMin&start=%s&end=%s&limit=%d&feed=iex",
		cfg.DataURL,
		symbol,
		cfg.TimeframeMin,
		startDate.Format("2006-01-02"),
		endDate.Format("2006-01-02"),
		cfg.BarsPerSymbol,
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

	// Keep only the most recent N bars
	bars := response.Bars
	if len(bars) > cfg.BarsPerSymbol {
		bars = bars[len(bars)-cfg.BarsPerSymbol:]
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

	// Save JSON
	if err := saveJSON(data, cfg.OutputDir); err != nil {
		resultChan <- FetchResult{Symbol: symbol, Error: fmt.Errorf("saving JSON: %w", err)}
		return
	}

	resultChan <- FetchResult{Symbol: symbol, Count: data.Count}
}

func main() {
	cfg := loadConfig()

	var symbols []string
	var err error

	if cfg.LiveMode {
		// Download latest pipeline outputs from GitHub Pages
		for _, f := range []struct{ url, dest string }{
			{pagesBase + "/strategies.json", "../../docs/strategies.json"},
			{pagesBase + "/candidates.json", "../../docs/candidates.json"},
		} {
			log.Printf("Downloading %s → %s", f.url, f.dest)
			if err := downloadFile(f.url, f.dest); err != nil {
				log.Fatalf("Failed to download %s: %v", f.url, err)
			}
		}

		log.Printf("Live mode: Fetching candidates from %s", cfg.StrategyFile)
		symbols, err = loadStrategies(cfg.StrategyFile)
		if err != nil {
			log.Fatalf("Failed to load strategies: %v", err)
		}
	} else {
		log.Printf("Backtest mode: Loading watchlist from %s", cfg.WatchlistFile)
		watchlist, err := loadWatchlist(cfg.WatchlistFile)
		if err != nil {
			log.Fatalf("Failed to load watchlist: %v", err)
		}
		symbols = watchlist.Symbols
	}

	if len(symbols) == 0 {
		log.Fatal("No symbols to fetch")
	}

	log.Printf("Creating output directory: %s", cfg.OutputDir)
	if err := os.MkdirAll(cfg.OutputDir, 0755); err != nil {
		log.Fatalf("Failed to create output directory: %v", err)
	}

	mode := "backtest"
	if cfg.LiveMode {
		mode = "live"
	}
	log.Printf("Mode: %s | Fetching %d bars for %d symbols (timeframe: %dMin)",
		mode, cfg.BarsPerSymbol, len(symbols), cfg.TimeframeMin)
	log.Println()

	var wg sync.WaitGroup
	resultChan := make(chan FetchResult, len(symbols))

	// Process symbols concurrently
	for _, symbol := range symbols {
		wg.Add(1)
		go processSymbol(cfg, symbol, resultChan, &wg)
	}

	// Wait for all goroutines to complete
	go func() {
		wg.Wait()
		close(resultChan)
	}()

	// Collect results
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
