package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
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

type AccountInfo struct {
	BuyingPower    float64 `json:"buying_power"`
	Cash           float64 `json:"cash"`
	PortfolioValue float64 `json:"portfolio_value"`
}

type Position struct {
	Symbol string  `json:"symbol"`
	Qty    float64 `json:"qty"`
	Side   string  `json:"side"`
}

func loadSignals() ([]Signal, error) {
	data, err := os.ReadFile("docs/signals.json")
	if err != nil {
		return nil, fmt.Errorf("reading signals.json: %w", err)
	}

	var signalsFile SignalsFile
	if err := json.Unmarshal(data, &signalsFile); err != nil {
		return nil, fmt.Errorf("parsing signals.json: %w", err)
	}

	return signalsFile.Signals, nil
}

func fetchAccountInfo(apiKey, apiSecret string) (*AccountInfo, error) {
	// In production, this would call:
	// GET https://paper-api.alpaca.markets/v2/account
	// Headers: APCA-API-KEY-ID, APCA-API-SECRET-KEY

	fmt.Println("Fetching account information from Alpaca...")
	fmt.Println("(Using paper trading account)")

	// Placeholder values
	return &AccountInfo{
		BuyingPower:    100000.0,
		Cash:           100000.0,
		PortfolioValue: 100000.0,
	}, nil
}

func fetchOpenPositions(apiKey, apiSecret string) ([]Position, error) {
	// In production, this would call:
	// GET https://paper-api.alpaca.markets/v2/positions
	// Headers: APCA-API-KEY-ID, APCA-API-SECRET-KEY

	fmt.Println("Fetching open positions from Alpaca...")

	// Placeholder - return empty positions for now
	return []Position{}, nil
}

func main() {
	fmt.Println("Low Frequency Trader v2 - Trade Executor\n")

	// Load credentials
	apiKey := os.Getenv("ALPACA_API_KEY")
	apiSecret := os.Getenv("ALPACA_API_SECRET")

	if apiKey == "" || apiSecret == "" {
		log.Fatal("Error: ALPACA_API_KEY and ALPACA_API_SECRET must be set")
	}

	fmt.Printf("Credentials loaded successfully\n")
	fmt.Printf("API Key: %s***\n", apiKey[:8])

	// Fetch account balance
	account, err := fetchAccountInfo(apiKey, apiSecret)
	if err != nil {
		log.Fatal("Error fetching account info:", err)
	}

	fmt.Printf("\nAccount Balance:\n")
	fmt.Printf("  Cash:            $%.2f\n", account.Cash)
	fmt.Printf("  Buying Power:    $%.2f\n", account.BuyingPower)
	fmt.Printf("  Portfolio Value: $%.2f\n", account.PortfolioValue)

	// Fetch open positions
	positions, err := fetchOpenPositions(apiKey, apiSecret)
	if err != nil {
		log.Fatal("Error fetching positions:", err)
	}

	// Build set of symbols we already have positions in
	existingPositions := make(map[string]bool)
	for _, pos := range positions {
		existingPositions[pos.Symbol] = true
	}

	fmt.Printf("Currently holding %d position(s)\n", len(positions))
	for _, pos := range positions {
		fmt.Printf("  %s: %.0f shares (%s)\n", pos.Symbol, pos.Qty, pos.Side)
	}

	// Calculate position size (2% of portfolio value)
	positionSize := account.PortfolioValue * 0.02
	fmt.Printf("\nPosition Size (2%% of portfolio): $%.2f\n", positionSize)

	// Load signals
	signals, err := loadSignals()
	if err != nil {
		fmt.Println("\nNo signals to execute:", err)
		return
	}

	if len(signals) == 0 {
		fmt.Println("\nNo signals to execute")
		return
	}

	fmt.Printf("\nFound %d signal(s) to execute:\n", len(signals))

	// Execute each signal
	for _, signal := range signals {
		fmt.Printf("\n📋 Signal: %s %s @ $%.2f\n", signal.Symbol, signal.Action, signal.Price)

		if signal.Action != "entry" {
			fmt.Println("   ⏭️  Skipping non-entry signal")
			continue
		}

		// Skip if we already have a position in this symbol
		if existingPositions[signal.Symbol] {
			fmt.Printf("   ⏭️  Already holding position in %s\n", signal.Symbol)
			continue
		}

		// Calculate shares to buy
		shares := int(positionSize / signal.Price)

		if shares < 1 {
			fmt.Printf("   ❌ Position size too small ($%.2f / $%.2f = %d shares)\n",
				positionSize, signal.Price, shares)
			continue
		}

		orderValue := float64(shares) * signal.Price

		fmt.Printf("   Strategy: %s\n", signal.Strategy)
		fmt.Printf("   Shares:   %d ($%.2f total)\n", shares, orderValue)

		if orderValue > account.Cash {
			fmt.Printf("   ❌ Insufficient cash (need $%.2f, have $%.2f)\n",
				orderValue, account.Cash)
			continue
		}

		// Set position parameters (matching backtest logic)
		takeProfit := signal.Price * 1.10 // +10%
		stopLoss := signal.Price * 0.95   // -5%

		fmt.Printf("   Take Profit: $%.2f (+10%%)\n", takeProfit)
		fmt.Printf("   Stop Loss:   $%.2f (-5%%)\n", stopLoss)

		// In production, this would place the order via Alpaca API:
		// POST /v2/orders
		// {
		//   "symbol": signal.Symbol,
		//   "qty": shares,
		//   "side": "buy",
		//   "type": "limit",
		//   "time_in_force": "day",
		//   "limit_price": signal.Price,
		//   "order_class": "bracket",
		//   "take_profit": { "limit_price": takeProfit },
		//   "stop_loss": { "stop_price": stopLoss }
		// }

		fmt.Println("   ✅ Order ready (not executed - dry run mode)")

		// Deduct from available cash for next signal
		account.Cash -= orderValue
	}

	fmt.Printf("\n✓ Execution complete (dry run - no actual orders placed)\n")
	fmt.Printf("Remaining cash: $%.2f\n", account.Cash)
}
