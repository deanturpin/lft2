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

	// Fetch today's activities
	now := time.Now()
	today := now.Format("2006-01-02")
	yesterday := now.AddDate(0, 0, -1).Format("2006-01-02")

	fmt.Printf("Fetching activities from %s onwards (filtering to %s)...\n", yesterday, today)

	// Alpaca /v2/account/activities endpoint
	url := fmt.Sprintf("%s/v2/account/activities?activity_types=FILL&after=%sT00:00:00Z",
		client.BaseURL, yesterday)

	body, err := client.Get(url)
	if err != nil {
		log.Fatalf("fetching activities: %v", err)
	}

	var activities []Activity
	if err := json.Unmarshal(body, &activities); err != nil {
		log.Fatalf("parsing activities: %v", err)
	}

	// Filter to today only
	var targetActivities []Activity
	for _, act := range activities {
		if len(act.TransactTime) >= 10 && act.TransactTime[:10] == today {
			targetActivities = append(targetActivities, act)
		}
	}

	fmt.Printf("Found %d trades on %s\n", len(targetActivities), today)

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
		Date:       today,
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

	fmt.Printf("✓ Wrote %s (%d activities)\n", outFile, len(targetActivities))

	// Generate HTML summary page
	htmlFile := "docs/daily-summary.html"
	html := generateHTML(summary)
	if err := os.WriteFile(htmlFile, []byte(html), 0644); err != nil {
		log.Fatalf("writing %s: %v", htmlFile, err)
	}

	fmt.Printf("✓ Wrote %s\n", htmlFile)
}

func generateHTML(s DailySummary) string {
	html := `<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Daily Trading Summary - ` + s.Date + `</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', monospace;
            max-width: 1200px;
            margin: 40px auto;
            padding: 20px;
            background: #0a0e14;
            color: #c5cdd9;
        }
        h1 {
            color: #6cb6ff;
            border-bottom: 2px solid #1a1f29;
            padding-bottom: 10px;
        }
        .summary {
            background: #1a1f29;
            padding: 20px;
            border-radius: 8px;
            margin: 20px 0;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .stat {
            padding: 10px;
        }
        .stat-label {
            color: #7d8793;
            font-size: 0.9em;
        }
        .stat-value {
            font-size: 1.5em;
            font-weight: bold;
            color: #6cb6ff;
        }
        table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
            background: #1a1f29;
        }
        th {
            background: #0f1419;
            padding: 12px;
            text-align: left;
            color: #6cb6ff;
            border-bottom: 2px solid #0a0e14;
        }
        td {
            padding: 10px 12px;
            border-bottom: 1px solid #0f1419;
        }
        tr:hover {
            background: #0f1419;
        }
        .buy { color: #7fd962; }
        .sell { color: #ff6666; }
        .time { color: #7d8793; font-size: 0.9em; }
        .no-trades {
            text-align: center;
            padding: 40px;
            color: #7d8793;
            font-style: italic;
        }
    </style>
</head>
<body>
    <h1>Daily Trading Summary</h1>
    <p style="color: #7d8793;">Date: ` + s.Date + `</p>

    <div class="summary">
        <div class="stat">
            <div class="stat-label">Total Trades</div>
            <div class="stat-value">` + fmt.Sprintf("%d", s.Summary.TotalTrades) + `</div>
        </div>
        <div class="stat">
            <div class="stat-label">Buys</div>
            <div class="stat-value buy">` + fmt.Sprintf("%d", s.Summary.Buys) + `</div>
        </div>
        <div class="stat">
            <div class="stat-label">Sells</div>
            <div class="stat-value sell">` + fmt.Sprintf("%d", s.Summary.Sells) + `</div>
        </div>
    </div>
`

	if len(s.Activities) == 0 {
		html += `    <div class="no-trades">No trades executed today</div>`
	} else {
		html += `    <table>
        <thead>
            <tr>
                <th>Time</th>
                <th>Symbol</th>
                <th>Side</th>
                <th>Quantity</th>
                <th>Price</th>
                <th>Net Amount</th>
            </tr>
        </thead>
        <tbody>
`
		for _, act := range s.Activities {
			time := act.TransactTime
			if len(time) >= 19 {
				time = time[11:19] // Extract HH:MM:SS
			}
			sideClass := ""
			if act.Side == "buy" {
				sideClass = " class=\"buy\""
			} else if act.Side == "sell" {
				sideClass = " class=\"sell\""
			}

			html += `            <tr>
                <td class="time">` + time + `</td>
                <td><strong>` + act.Symbol + `</strong></td>
                <td` + sideClass + `>` + act.Side + `</td>
                <td>` + act.Qty + `</td>
                <td>$` + act.Price + `</td>
                <td>$` + act.NetAmount + `</td>
            </tr>
`
		}
		html += `        </tbody>
    </table>`
	}

	html += `
    <p style="margin-top: 40px; color: #7d8793; font-size: 0.9em;">
        Generated by <a href="https://github.com/deanturpin/lft2" style="color: #6cb6ff;">LFT2</a>
        at ` + time.Now().Format("2006-01-02 15:04:05 MST") + `
    </p>
</body>
</html>
`
	return html
}
