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

// Position data from Alpaca /v2/positions
type Position struct {
	Symbol string `json:"symbol"`
	Qty    string `json:"qty"`
	Side   string `json:"side"`
}

// OrderRequest is the JSON body for POST /v2/orders
type OrderRequest struct {
	Symbol      string `json:"symbol"`
	Qty         string `json:"qty"`
	Side        string `json:"side"`        // "buy" or "sell"
	Type        string `json:"type"`        // "market"
	TimeInForce string `json:"time_in_force"` // "day"
	ClientOrdID string `json:"client_order_id,omitempty"`
}

var client alpaca.Client

// Strategy name abbreviations (2 chars each, can chain: "mr+bb+vo")
var strategyAbbrev = map[string]string{
	"mean_reversion":      "mr",
	"sma_crossover":       "sc",
	"rsi_oversold":        "ro",
	"volatility_breakout": "vb",
	"bollinger_breakout":  "bb",
	"momentum":            "mo",
	"price_dip":           "pd",
	"volume_surge":        "vs",
	"macd_crossover":      "mc",
	"gap_fill":            "gf",
	"morning_breakout":    "mb",
}

// Strategy allowlist for live trading (true = enabled, false = disabled)
// All strategies still backtest for analysis, but only enabled ones trade live
// Disabled = conceptually unsuitable for intraday (backtest filter handles poor performers)
var strategyAllowlist = map[string]bool{
	"mean_reversion":      true,  // ✅ Intraday mean reversion works well
	"volatility_breakout": true,  // ✅ Catches intraday bursts
	"bollinger_breakout":  true,  // ✅ Intraday breakouts
	"price_dip":           true,  // ✅ Quick intraday reversals
	"volume_surge":        true,  // ✅ Intraday momentum bursts
	"macd_crossover":      true,  // ✅ Can signal intraday momentum shifts
	"gap_fill":            true,  // ✅ Intraday mean reversion
	"morning_breakout":    true,  // ✅ Early session momentum
	"sma_crossover":       true,  // ✅ Let backtest decide (made +£69.50)
	"rsi_oversold":        false, // ❌ Needs 4-48 hours, not 1-2 hour intraday holds
	"momentum":            false, // ❌ Late session entries fade before close
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

func fetchPositions() (map[string]Position, error) {
	body, err := client.Get(client.BaseURL + "/v2/positions")
	if err != nil {
		return nil, err
	}
	var list []Position
	if err := json.Unmarshal(body, &list); err != nil {
		return nil, err
	}
	// Index by symbol for O(1) lookup
	positions := make(map[string]Position, len(list))
	for _, p := range list {
		positions[p.Symbol] = p
	}
	return positions, nil
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

// readOrders parses a .fix file and returns the list of order field maps
// (heartbeat lines are filtered out). Returns nil if the file doesn't exist.
func readOrders(path string) ([]map[string]string, error) {
	f, err := os.Open(path)
	if os.IsNotExist(err) {
		fmt.Printf("  [skip] %s not found\n", path)
		return nil, nil
	}
	if err != nil {
		return nil, err
	}
	defer f.Close()

	var orders []map[string]string
	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		line := strings.TrimSpace(scanner.Text())
		if line == "" {
			continue
		}
		fields := parseFIX(line)
		if fields["35"] == "0" {
			// Heartbeat — confirm pipeline ran
			fmt.Printf("  [heartbeat] ts=%s text=%s\n", fields["52"], fields["58"])
			continue
		}
		orders = append(orders, fields)
	}
	return orders, scanner.Err()
}

// submitOrder posts a single order to Alpaca and prints the result.
func submitOrder(req OrderRequest) error {
	body, err := json.Marshal(req)
	if err != nil {
		return fmt.Errorf("marshalling order: %w", err)
	}

	fmt.Printf("  [POST] symbol=%s side=%s qty=%s type=%s tif=%s id=%s\n",
		req.Symbol, req.Side, req.Qty, req.Type, req.TimeInForce, req.ClientOrdID)
	fmt.Printf("         body: %s\n", string(body))

	resp, err := client.Post(client.BaseURL+"/v2/orders", body)
	if err != nil {
		return fmt.Errorf("submitting order: %w", err)
	}

	// Pretty-print the response for debugging
	var pretty map[string]interface{}
	if err := json.Unmarshal(resp, &pretty); err == nil {
		id, _ := pretty["id"].(string)
		status, _ := pretty["status"].(string)
		filledQty, _ := pretty["filled_qty"].(string)
		fmt.Printf("  [OK]   order_id=%s status=%s filled_qty=%s\n", id, status, filledQty)
	} else {
		fmt.Printf("  [OK]   raw response: %s\n", string(resp))
	}
	return nil
}

func main() {
	apiKey := os.Getenv("ALPACA_API_KEY")
	apiSecret := os.Getenv("ALPACA_API_SECRET")
	if apiKey == "" || apiSecret == "" {
		log.Fatal("ALPACA_API_KEY and ALPACA_API_SECRET must be set")
	}
	client = alpaca.New(apiKey, apiSecret, os.Getenv("ALPACA_BASE_URL"), "")

	fmt.Println("Low Frequency Trader v2 - Trade Executor")
	fmt.Println(strings.Repeat("─", 50))

	// ── Account ──────────────────────────────────────────
	fmt.Println("\n[account]")
	account, err := fetchAccount()
	if err != nil {
		log.Fatal("fetching account: ", err)
	}
	fmt.Printf("  Cash:            $%s\n", account.Cash)
	fmt.Printf("  Buying Power:    $%s\n", account.BuyingPower)
	fmt.Printf("  Portfolio Value: $%s\n", account.PortfolioValue)

	// ── Positions ─────────────────────────────────────────
	fmt.Println("\n[positions]")
	positions, err := fetchPositions()
	if err != nil {
		log.Fatal("fetching positions: ", err)
	}
	if len(positions) == 0 {
		fmt.Println("  (none)")
	}
	for sym, p := range positions {
		fmt.Printf("  %-6s qty=%s side=%s\n", sym, p.Qty, p.Side)
	}

	// ── Buys first ────────────────────────────────────────
	fmt.Println("\n[buy orders] docs/buy.fix")
	buyOrders, err := readOrders("docs/buy.fix")
	if err != nil {
		log.Fatal("reading buy.fix: ", err)
	}

	// Merge multiple strategy signals for same symbol into single order
	type mergedOrder struct {
		symbol      string
		totalQty    int
		strategies  []string
		clientOrdID string // Use first strategy's order ID
	}
	merged := make(map[string]*mergedOrder)

	for _, fields := range buyOrders {
		symbol := fields["55"]
		clientOrdID := fields["11"]
		strategy := fields["58"]
		qtyStr := fields["38"]

		if symbol == "" {
			fmt.Printf("  [skip] missing symbol\n")
			continue
		}

		// Skip disabled strategies (unsuitable for day trading)
		if allowed, exists := strategyAllowlist[strategy]; exists && !allowed {
			fmt.Printf("  [skip] %s strategy=%s (disabled in allowlist)\n", symbol, strategy)
			continue
		}

		// Skip if we already hold this stock
		if held, ok := positions[symbol]; ok {
			fmt.Printf("  [skip] %s already held (qty=%s side=%s)\n",
				symbol, held.Qty, held.Side)
			continue
		}

		// Parse quantity
		var qty int
		if _, err := fmt.Sscanf(qtyStr, "%d", &qty); err != nil || qty <= 0 {
			fmt.Printf("  [skip] %s invalid qty=%s\n", symbol, qtyStr)
			continue
		}

		// Track multiple strategies for same symbol (don't increase qty)
		if order, exists := merged[symbol]; exists {
			order.strategies = append(order.strategies, strategy)
			fmt.Printf("  [merge] %s strategy=%s (multiple signals, qty unchanged)\n",
				symbol, strategy)
		} else {
			merged[symbol] = &mergedOrder{
				symbol:      symbol,
				totalQty:    qty,
				strategies:  []string{strategy},
				clientOrdID: clientOrdID,
			}
			fmt.Printf("  [queue] %s %d shares (strategy=%s)\n", symbol, qty, strategy)
		}
	}

	// Submit merged orders
	buysSubmitted := 0
	for _, order := range merged {
		strategyLabel := order.strategies[0]
		clientOrdID := order.clientOrdID

		// Update client_order_id if multiple strategies triggered
		if len(order.strategies) > 1 {
			// Build compact strategy code: "mr+bb+vb" instead of "multiple_3"
			var codes []string
			for _, strat := range order.strategies {
				if abbrev, ok := strategyAbbrev[strat]; ok {
					codes = append(codes, abbrev)
				} else {
					codes = append(codes, strat[:2]) // Fallback: first 2 chars
				}
			}
			strategyLabel = strings.Join(codes, "+")

			// Replace strategy name in client_order_id: AAPL_mean_reversion_... → AAPL_mr+bb+vb_...
			// Format: symbol_strategy_tp_sl_tsl_timestamp
			parts := strings.SplitN(clientOrdID, "_", 3) // Split into [symbol, strategy, rest]
			if len(parts) >= 3 {
				clientOrdID = fmt.Sprintf("%s_%s_%s", parts[0], strategyLabel, parts[2])
			}
		}

		fmt.Printf("  [buy]  %s qty=%d strategy=%s id=%s\n",
			order.symbol, order.totalQty, strategyLabel, clientOrdID)
		if err := submitOrder(OrderRequest{
			Symbol:      order.symbol,
			Qty:         fmt.Sprintf("%d", order.totalQty),
			Side:        "buy",
			Type:        "market",
			TimeInForce: "day",
			ClientOrdID: clientOrdID,
		}); err != nil {
			fmt.Printf("  [ERROR] %v\n", err)
		} else {
			buysSubmitted++
		}
	}
	if buysSubmitted == 0 && len(buyOrders) == 0 {
		fmt.Println("  (no orders)")
	}

	// ── Sells after buys ──────────────────────────────────
	fmt.Println("\n[sell orders] docs/sell.fix")
	sellOrders, err := readOrders("docs/sell.fix")
	if err != nil {
		log.Fatal("reading sell.fix: ", err)
	}

	sellsSubmitted := 0
	for _, fields := range sellOrders {
		symbol := fields["55"]
		clOrdID := fields["11"]
		exitReason := fields["58"] // FIX tag 58: TEXT field from exits.cxx

		if symbol == "" {
			fmt.Printf("  [skip] missing symbol in order id=%s\n", clOrdID)
			continue
		}

		// Check we actually hold this — don't sell what we don't own.
		// This should never happen: exits.cxx reads positions.json which is
		// written by the account module from the same live API. If it does,
		// there is a pipeline ordering bug or a stale positions.json.
		held, ok := positions[symbol]
		if !ok {
			fmt.Printf("  [WARNING] %s in sell.fix but NOT in live positions — pipeline bug? skipping\n", symbol)
			continue
		}

		reasonLabel := exitReason
		if reasonLabel == "" {
			reasonLabel = "unknown"
		}
		fmt.Printf("  [sell] %s qty=%s reason=%s\n", symbol, held.Qty, reasonLabel)
		if err := submitOrder(OrderRequest{
			Symbol:      symbol,
			Qty:         held.Qty,
			Side:        "sell",
			Type:        "market",
			TimeInForce: "day",
			ClientOrdID: clOrdID,
		}); err != nil {
			fmt.Printf("  [ERROR] %v\n", err)
		}
		sellsSubmitted++
	}
	if sellsSubmitted == 0 && len(sellOrders) == 0 {
		fmt.Println("  (no orders)")
	}

	fmt.Println("\n" + strings.Repeat("─", 50))
	fmt.Printf("✓ Execution complete  buys=%d  sells=%d\n", buysSubmitted, sellsSubmitted)
}
