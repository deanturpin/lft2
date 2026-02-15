package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"time"
)

// Account data from Alpaca
type Account struct {
	AccountNumber    string    `json:"account_number"`
	Status           string    `json:"status"`
	Currency         string    `json:"currency"`
	Cash             string    `json:"cash"`
	PortfolioValue   string    `json:"portfolio_value"`
	BuyingPower      string    `json:"buying_power"`
	Equity           string    `json:"equity"`
	LastEquity       string    `json:"last_equity"`
	LongMarketValue  string    `json:"long_market_value"`
	ShortMarketValue string    `json:"short_market_value"`
	InitialMargin    string    `json:"initial_margin"`
	MaintenanceMargin string   `json:"maintenance_margin"`
	DaytradingBuyingPower string `json:"daytrading_buying_power"`
	LastFetched      time.Time `json:"last_fetched"`
}

// Position data from Alpaca
type Position struct {
	Symbol           string  `json:"symbol"`
	Qty              string  `json:"qty"`
	AvgEntryPrice    string  `json:"avg_entry_price"`
	CurrentPrice     string  `json:"current_price"`
	MarketValue      string  `json:"market_value"`
	CostBasis        string  `json:"cost_basis"`
	UnrealizedPL     string  `json:"unrealized_pl"`
	UnrealizedPLPC   string  `json:"unrealized_plpc"`
	ChangeToday      string  `json:"change_today"`
	Side             string  `json:"side"`
	AssetClass       string  `json:"asset_class"`
}

// Dashboard response combining account and positions
type Dashboard struct {
	Account   Account    `json:"account"`
	Positions []Position `json:"positions"`
}

var (
	apiKey    string
	apiSecret string
	baseURL   string
)

func init() {
	apiKey = os.Getenv("ALPACA_API_KEY")
	apiSecret = os.Getenv("ALPACA_API_SECRET")
	baseURL = os.Getenv("ALPACA_BASE_URL")

	if baseURL == "" {
		baseURL = "https://paper-api.alpaca.markets"
	}

	if apiKey == "" || apiSecret == "" {
		log.Fatal("ALPACA_API_KEY and ALPACA_API_SECRET must be set")
	}
}

func fetchAccount() (*Account, error) {
	req, err := http.NewRequest("GET", baseURL+"/v2/account", nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("APCA-API-KEY-ID", apiKey)
	req.Header.Set("APCA-API-SECRET-KEY", apiSecret)

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("alpaca API returned status %d", resp.StatusCode)
	}

	var account Account
	if err := json.NewDecoder(resp.Body).Decode(&account); err != nil {
		return nil, err
	}

	account.LastFetched = time.Now()
	return &account, nil
}

func fetchPositions() ([]Position, error) {
	req, err := http.NewRequest("GET", baseURL+"/v2/positions", nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("APCA-API-KEY-ID", apiKey)
	req.Header.Set("APCA-API-SECRET-KEY", apiSecret)

	client := &http.Client{Timeout: 10 * time.Second}
	resp, err := client.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("alpaca API returned status %d", resp.StatusCode)
	}

	var positions []Position
	if err := json.NewDecoder(resp.Body).Decode(&positions); err != nil {
		return nil, err
	}

	return positions, nil
}

func handleDashboard(w http.ResponseWriter, r *http.Request) {
	// Enable CORS for local development
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Content-Type", "application/json")

	account, err := fetchAccount()
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		log.Printf("Error fetching account: %v", err)
		return
	}

	positions, err := fetchPositions()
	if err != nil {
		http.Error(w, err.Error(), http.StatusInternalServerError)
		log.Printf("Error fetching positions: %v", err)
		return
	}

	dashboard := Dashboard{
		Account:   *account,
		Positions: positions,
	}

	if err := json.NewEncoder(w).Encode(dashboard); err != nil {
		log.Printf("Error encoding response: %v", err)
	}
}

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	http.HandleFunc("/api/dashboard", handleDashboard)

	log.Printf("Account service starting on port %s", port)
	log.Printf("Using Alpaca base URL: %s", baseURL)
	log.Fatal(http.ListenAndServe(":"+port, nil))
}
