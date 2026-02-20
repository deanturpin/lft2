package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"time"

	"github.com/deanturpin/lft2/internal/alpaca"
)

// Activity represents a trade or position change from Alpaca /v2/account/activities
type Activity struct {
	ActivityType string  `json:"activity_type"` // "FILL", "DIV", etc.
	TransactTime string  `json:"transaction_time"`
	Symbol       string  `json:"symbol"`
	Qty          string  `json:"qty"`
	Price        string  `json:"price"`
	Side         string  `json:"side"` // "buy" or "sell"
	NetAmount    string  `json:"net_amount"`
}

// DailySummary represents the JSON output for GitHub Pages
type DailySummary struct {
	Date       string         `json:"date"`
	Activities []Activity     `json:"activities"`
	Summary    TradingSummary `json:"summary"`
}

type TradingSummary struct {
	TotalTrades int    `json:"total_trades"`
	Buys        int    `json:"buys"`
	Sells       int    `json:"sells"`
	NetPnL      string `json:"net_pnl"` // Simple approximation
}

var client alpaca.Client

func main() {
	fmt.Println("Low Frequency Trader v2 - Daily Summary\n")

	// Load credentials
	apiKey := os.Getenv("ALPACA_API_KEY")
	apiSecret := os.Getenv("ALPACA_API_SECRET")
	if apiKey == "" || apiSecret == "" {
		log.Fatal("ALPACA_API_KEY and ALPACA_API_SECRET must be set")
	}
	client = alpaca.New(apiKey, apiSecret, os.Getenv("ALPACA_BASE_URL"), "")

	// Fetch yesterday's activities (for testing - change to "today" in production)
	now := time.Now()
	yesterday := now.AddDate(0, 0, -1).Format("2006-01-02")
	twoDaysAgo := now.AddDate(0, 0, -2).Format("2006-01-02")

	fmt.Printf("Fetching activities from %s onwards (filtering to %s)...\n", twoDaysAgo, yesterday)

	// Alpaca /v2/account/activities endpoint
	url := fmt.Sprintf("%s/v2/account/activities?activity_types=FILL&after=%sT00:00:00Z",
		client.BaseURL, twoDaysAgo)

	body, err := client.Get(url)
	if err != nil {
		log.Fatalf("fetching activities: %v", err)
	}

	var activities []Activity
	if err := json.Unmarshal(body, &activities); err != nil {
		log.Fatalf("parsing activities: %v", err)
	}

	// Filter to yesterday only (for testing)
	var targetActivities []Activity
	for _, act := range activities {
		if len(act.TransactTime) >= 10 && act.TransactTime[:10] == yesterday {
			targetActivities = append(targetActivities, act)
		}
	}

	fmt.Printf("Found %d trades on %s\n", len(targetActivities), yesterday)

	// Calculate summary stats
	buys := 0
	sells := 0
	for _, act := range targetActivities {
		if act.Side == "buy" {
			buys++
		} else if act.Side == "sell" {
			sells++
		}
	}

	summary := DailySummary{
		Date:       yesterday,
		Activities: targetActivities,
		Summary: TradingSummary{
			TotalTrades: len(targetActivities),
			Buys:        buys,
			Sells:       sells,
			NetPnL:      "calculated_by_dashboard", // Dashboard will compute from matched pairs
		},
	}

	// Write to docs/daily-summary.json
	outFile := "docs/daily-summary.json"
	f, err := os.Create(outFile)
	if err != nil {
		log.Fatalf("creating %s: %v", outFile, err)
	}
	defer f.Close()

	encoder := json.NewEncoder(f)
	encoder.SetIndent("", "  ")
	encoder.SetEscapeHTML(false)
	if err := encoder.Encode(summary); err != nil {
		log.Fatalf("writing JSON: %v", err)
	}

	fmt.Printf("\n✓ Wrote %s (%d activities)\n", outFile, len(targetActivities))
}
