package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"strconv"
)

type Signal struct {
	Symbol    string  `json:"symbol"`
	Strategy  string  `json:"strategy"`
	Action    string  `json:"action"`
	Price     float64 `json:"price"`
	Timestamp string  `json:"timestamp"`
}

type SignalsFile struct {
	Signals []Signal `json:"signals"`
}

// Matches the schema written by the account module to docs/account.json
type AccountInfo struct {
	BuyingPower    string `json:"buying_power"`
	Cash           string `json:"cash"`
	PortfolioValue string `json:"portfolio_value"`
}

// Matches the schema written by the account module to docs/positions.json
type Position struct {
	Symbol        string `json:"symbol"`
	Qty           string `json:"qty"`
	AvgEntryPrice string `json:"avg_entry_price"`
	Side          string `json:"side"`
}

func mustParseFloat(s string) float64 {
	v, err := strconv.ParseFloat(s, 64)
	if err != nil {
		log.Fatalf("parsing float %q: %v", s, err)
	}
	return v
}

func loadJSON(path string, v any) error {
	data, err := os.ReadFile(path)
	if err != nil {
		return fmt.Errorf("reading %s: %w", path, err)
	}
	if err := json.Unmarshal(data, v); err != nil {
		return fmt.Errorf("parsing %s: %w", path, err)
	}
	return nil
}

func main() {
	fmt.Println("Low Frequency Trader v2 - Trade Executor\n")

	apiKey := os.Getenv("ALPACA_API_KEY")
	apiSecret := os.Getenv("ALPACA_API_SECRET")
	if apiKey == "" || apiSecret == "" {
		log.Fatal("ALPACA_API_KEY and ALPACA_API_SECRET must be set")
	}
	fmt.Printf("API Key: %s***\n", apiKey[:8])

	// Read live account data written by the account module
	var account AccountInfo
	if err := loadJSON("docs/account.json", &account); err != nil {
		log.Fatal("Run the account module first: make account\n", err)
	}

	cash := mustParseFloat(account.Cash)
	portfolioValue := mustParseFloat(account.PortfolioValue)

	fmt.Printf("\nAccount Balance:\n")
	fmt.Printf("  Cash:            $%.2f\n", cash)
	fmt.Printf("  Buying Power:    $%s\n", account.BuyingPower)
	fmt.Printf("  Portfolio Value: $%.2f\n", portfolioValue)

	// Read live positions written by the account module
	var positions []Position
	if err := loadJSON("docs/positions.json", &positions); err != nil {
		log.Fatal("Run the account module first: make account\n", err)
	}

	existingPositions := make(map[string]bool)
	for _, pos := range positions {
		existingPositions[pos.Symbol] = true
	}

	fmt.Printf("\nCurrently holding %d position(s)\n", len(positions))
	for _, pos := range positions {
		fmt.Printf("  %s: %s shares (%s)\n", pos.Symbol, pos.Qty, pos.Side)
	}

	// Calculate position size (2% of portfolio value)
	positionSize := portfolioValue * 0.02
	fmt.Printf("\nPosition Size (2%% of portfolio): $%.2f\n", positionSize)

	// Load signals
	var signalsFile SignalsFile
	if err := loadJSON("docs/signals.json", &signalsFile); err != nil {
		fmt.Println("\nNo signals to execute:", err)
		return
	}

	signals := signalsFile.Signals
	if len(signals) == 0 {
		fmt.Println("\nNo signals to execute")
		return
	}

	fmt.Printf("\nFound %d signal(s) to execute:\n", len(signals))

	for _, signal := range signals {
		fmt.Printf("\n📋 Signal: %s %s @ $%.2f\n", signal.Symbol, signal.Action, signal.Price)

		if signal.Action != "entry" {
			fmt.Println("   ⏭️  Skipping non-entry signal")
			continue
		}

		if existingPositions[signal.Symbol] {
			fmt.Printf("   ⏭️  Already holding position in %s\n", signal.Symbol)
			continue
		}

		shares := int(positionSize / signal.Price)
		if shares < 1 {
			fmt.Printf("   ❌ Position size too small ($%.2f / $%.2f = %d shares)\n",
				positionSize, signal.Price, shares)
			continue
		}

		orderValue := float64(shares) * signal.Price
		if orderValue > cash {
			fmt.Printf("   ❌ Insufficient cash (need $%.2f, have $%.2f)\n", orderValue, cash)
			continue
		}

		takeProfit := signal.Price * 1.10
		stopLoss := signal.Price * 0.95

		fmt.Printf("   Strategy:    %s\n", signal.Strategy)
		fmt.Printf("   Shares:      %d ($%.2f total)\n", shares, orderValue)
		fmt.Printf("   Take Profit: $%.2f (+10%%)\n", takeProfit)
		fmt.Printf("   Stop Loss:   $%.2f (-5%%)\n", stopLoss)
		fmt.Println("   ✅ Order ready (dry run — no actual orders placed)")

		cash -= orderValue
	}

	fmt.Printf("\n✓ Execution complete\n")
	fmt.Printf("Remaining cash: $%.2f\n", cash)
}
