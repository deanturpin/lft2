package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"

	"github.com/deanturpin/lft2/internal/alpaca"
)

// Account data from Alpaca
type Account struct {
	AccountNumber         string `json:"account_number"`
	Status                string `json:"status"`
	Currency              string `json:"currency"`
	Cash                  string `json:"cash"`
	PortfolioValue        string `json:"portfolio_value"`
	BuyingPower           string `json:"buying_power"`
	Equity                string `json:"equity"`
	LastEquity            string `json:"last_equity"`
	LongMarketValue       string `json:"long_market_value"`
	ShortMarketValue      string `json:"short_market_value"`
	InitialMargin         string `json:"initial_margin"`
	MaintenanceMargin     string `json:"maintenance_margin"`
	DaytradingBuyingPower string `json:"daytrading_buying_power"`
}

// Position data from Alpaca
type Position struct {
	Symbol         string `json:"symbol"`
	Qty            string `json:"qty"`
	AvgEntryPrice  string `json:"avg_entry_price"`
	CurrentPrice   string `json:"current_price"`
	MarketValue    string `json:"market_value"`
	CostBasis      string `json:"cost_basis"`
	UnrealizedPL   string `json:"unrealized_pl"`
	UnrealizedPLPC string `json:"unrealized_plpc"`
	ChangeToday    string `json:"change_today"`
	Side           string `json:"side"`
	AssetClass     string `json:"asset_class"`
}


var client alpaca.Client

func init() {
	apiKey := os.Getenv("ALPACA_API_KEY")
	apiSecret := os.Getenv("ALPACA_API_SECRET")

	if apiKey == "" || apiSecret == "" {
		log.Fatal("ALPACA_API_KEY and ALPACA_API_SECRET must be set")
	}

	client = alpaca.New(apiKey, apiSecret, os.Getenv("ALPACA_BASE_URL"), "")
}

func fetchAccount() (*Account, error) {
	body, err := client.Get(client.BaseURL + "/v2/account")
	if err != nil {
		return nil, err
	}

	var account Account
	if err := json.Unmarshal(body, &account); err != nil {
		return nil, err
	}

	return &account, nil
}

func fetchPositions() ([]Position, error) {
	body, err := client.Get(client.BaseURL + "/v2/positions")
	if err != nil {
		return nil, err
	}

	var positions []Position
	if err := json.Unmarshal(body, &positions); err != nil {
		return nil, err
	}

	return positions, nil
}


func main() {
	fmt.Println("Low Frequency Trader v2 - Account Module\n")

	// Fetch account info
	account, err := fetchAccount()
	if err != nil {
		log.Fatalf("Error fetching account: %v", err)
	}

	fmt.Printf("Account Status: %s\n", account.Status)
	fmt.Printf("  Cash:            $%s\n", account.Cash)
	fmt.Printf("  Buying Power:    $%s\n", account.BuyingPower)
	fmt.Printf("  Portfolio Value: $%s\n", account.PortfolioValue)
	fmt.Printf("  Equity:          $%s\n", account.Equity)

	// Ensure docs directory exists
	if err := os.MkdirAll("docs", 0755); err != nil {
		log.Fatalf("Error creating docs directory: %v", err)
	}

	// Write account.json for entries module
	accountFile, err := os.Create("docs/account.json")
	if err != nil {
		log.Fatalf("Error creating account.json: %v", err)
	}
	defer accountFile.Close()

	// Simplified account info for entries module
	accountData := map[string]interface{}{
		"cash":            account.Cash,
		"buying_power":    account.BuyingPower,
		"portfolio_value": account.PortfolioValue,
		"equity":          account.Equity,
	}

	encoder := json.NewEncoder(accountFile)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(accountData); err != nil {
		log.Fatalf("Error writing account.json: %v", err)
	}

	fmt.Println("\n✓ Wrote docs/account.json")

	// Fetch positions
	positions, err := fetchPositions()
	if err != nil {
		log.Fatalf("Error fetching positions: %v", err)
	}

	fmt.Printf("\nCurrently holding %d position(s):\n", len(positions))
	for _, pos := range positions {
		fmt.Printf("  %s: %s shares @ $%s (P/L: $%s / %s%%)\n",
			pos.Symbol, pos.Qty, pos.AvgEntryPrice, pos.UnrealizedPL, pos.UnrealizedPLPC)
	}

	// Write positions.json for exits module
	positionsFile, err := os.Create("docs/positions.json")
	if err != nil {
		log.Fatalf("Error creating positions.json: %v", err)
	}
	defer positionsFile.Close()

	// Simplified position data for exits module
	type SimplePosition struct {
		Symbol        string `json:"symbol"`
		Qty           string `json:"qty"`
		AvgEntryPrice string `json:"avg_entry_price"`
		Side          string `json:"side"`
	}

	simplePositions := make([]SimplePosition, len(positions))
	for i, pos := range positions {
		simplePositions[i] = SimplePosition{
			Symbol:        pos.Symbol,
			Qty:           pos.Qty,
			AvgEntryPrice: pos.AvgEntryPrice,
			Side:          pos.Side,
		}
	}

	encoder = json.NewEncoder(positionsFile)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(simplePositions); err != nil {
		log.Fatalf("Error writing positions.json: %v", err)
	}

	fmt.Println("\n✓ Wrote docs/positions.json")
}
