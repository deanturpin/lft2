package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"strings"

	"github.com/deanturpin/lft2/internal/alpaca"
)

// Account data from Alpaca /v2/account
type Account struct {
	Cash           string `json:"cash"`
	BuyingPower    string `json:"buying_power"`
	PortfolioValue string `json:"portfolio_value"`
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

// parseFIX parses a single FIX message line into a tag→value map.
// Format: 8=FIX.5.0SP2|9=...|35=D|...|
func parseFIX(line string) map[string]string {
	fields := make(map[string]string)
	for _, pair := range strings.Split(line, "|") {
		parts := strings.SplitN(pair, "=", 2)
		if len(parts) == 2 {
			fields[parts[0]] = parts[1]
		}
	}
	return fields
}

// submitFIX reads a .fix file and prints each order (dry run for now).
// See issue #28 for full Alpaca REST submission implementation.
func submitFIX(path string) error {
	f, err := os.Open(path)
	if os.IsNotExist(err) {
		fmt.Printf("  no orders (%s not found)\n", path)
		return nil
	}
	if err != nil {
		return err
	}
	defer f.Close()

	count := 0
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		fields := parseFIX(line)
		side := map[string]string{"1": "buy", "2": "sell"}[fields["54"]]
		fmt.Printf("  %-6s %s qty=%s id=%s\n",
			side, fields["55"], fields["38"], fields["11"])
		count++
	}
	if count == 0 {
		fmt.Println("  no orders in file")
	}
	return scanner.Err()
}

func main() {
	fmt.Println("Low Frequency Trader v2 - Trade Executor\n")

	account, err := fetchAccount()
	if err != nil {
		log.Fatal("fetching account: ", err)
	}

	fmt.Printf("Account Balance:\n")
	fmt.Printf("  Cash:            $%s\n", account.Cash)
	fmt.Printf("  Buying Power:    $%s\n", account.BuyingPower)
	fmt.Printf("  Portfolio Value: $%s\n", account.PortfolioValue)

	fmt.Println("\nSell orders (docs/sell.fix):")
	if err := submitFIX("docs/sell.fix"); err != nil {
		log.Fatal("reading sell.fix: ", err)
	}

	fmt.Println("\nBuy orders (docs/buy.fix):")
	if err := submitFIX("docs/buy.fix"); err != nil {
		log.Fatal("reading buy.fix: ", err)
	}

	fmt.Println("\n✓ Execution complete (dry run — see issue #28 for live submission)")
}
