package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"time"
)

type FilterCriteria struct {
	MinAvgVolume  float64 `json:"min_avg_volume"`
	MinPrice      float64 `json:"min_price"`
	MaxPrice      float64 `json:"max_price"`
	MinVolatility float64 `json:"min_volatility"`
	MinBarCount   int     `json:"min_bar_count"`
}

type BarData struct {
	Symbol    string `json:"symbol"`
	Bars      []Bar  `json:"bars"`
	Count     int    `json:"count"`
	FetchedAt string `json:"fetched_at"`
}

type Bar struct {
	Timestamp string  `json:"t"`
	Open      float64 `json:"o"`
	High      float64 `json:"h"`
	Low       float64 `json:"l"`
	Close     float64 `json:"c"`
	Volume    int64   `json:"v"`
}

type CandidatesOutput struct {
	Timestamp       string         `json:"timestamp"`
	Symbols         []string       `json:"symbols"`
	Criteria        FilterCriteria `json:"criteria"`
	TotalCandidates int            `json:"total_candidates"`
}

func calculateStats(bars []Bar) (avgVolume float64, avgPrice float64, avgVolatility float64) {
	if len(bars) == 0 {
		return 0, 0, 0
	}

	var totalVolume int64
	var totalPrice float64
	var totalRange float64

	for _, bar := range bars {
		totalVolume += bar.Volume
		totalPrice += bar.Close
		if bar.Close > 0 {
			totalRange += (bar.High - bar.Low) / bar.Close
		}
	}

	count := float64(len(bars))
	avgVolume = float64(totalVolume) / count
	avgPrice = totalPrice / count
	avgVolatility = totalRange / count

	return avgVolume, avgPrice, avgVolatility
}

func passesFilter(data *BarData, criteria FilterCriteria) bool {
	if data.Count < criteria.MinBarCount {
		return false
	}

	avgVolume, avgPrice, avgVolatility := calculateStats(data.Bars)

	if avgVolume < criteria.MinAvgVolume {
		return false
	}

	if avgPrice < criteria.MinPrice || avgPrice > criteria.MaxPrice {
		return false
	}

	if avgVolatility < criteria.MinVolatility {
		return false
	}

	return true
}

func main() {
	log.Println("Filter Module - Identifying candidate stocks")
	log.Println("")

	barsDir := "../../docs/bars"

	if _, err := os.Stat(barsDir); os.IsNotExist(err) {
		log.Fatalf("Error: bars directory not found: %s", barsDir)
	}

	criteria := FilterCriteria{
		MinAvgVolume:  1_000_000.0,
		MinPrice:      10.0,
		MaxPrice:      500.0,
		MinVolatility: 0.01, // 1% daily range minimum
		MinBarCount:   100,
	}

	var candidates []string
	totalFiles := 0

	// Process all JSON files
	entries, err := os.ReadDir(barsDir)
	if err != nil {
		log.Fatalf("Error reading directory: %v", err)
	}

	for _, entry := range entries {
		if entry.IsDir() || filepath.Ext(entry.Name()) != ".json" {
			continue
		}

		totalFiles++
		filePath := filepath.Join(barsDir, entry.Name())

		data, err := os.ReadFile(filePath)
		if err != nil {
			log.Printf("✗ %s: could not read file: %v", entry.Name(), err)
			continue
		}

		var barData BarData
		if err := json.Unmarshal(data, &barData); err != nil {
			log.Printf("✗ %s: could not parse JSON: %v", entry.Name(), err)
			continue
		}

		if barData.Symbol == "" {
			log.Printf("✗ %s: missing symbol", entry.Name())
			continue
		}

		if passesFilter(&barData, criteria) {
			candidates = append(candidates, barData.Symbol)
			log.Printf("✓ %s (%d bars)", barData.Symbol, barData.Count)
		} else {
			avgVolume, avgPrice, avgVolatility := calculateStats(barData.Bars)
			log.Printf("✗ %s (bars: %d, vol: %.0f, price: $%.2f, volatility: %.3f%%)",
				barData.Symbol, barData.Count, avgVolume, avgPrice, avgVolatility*100)
		}
	}

	log.Println("")
	log.Printf("Candidates: %d/%d", len(candidates), totalFiles)

	// Write candidates.json
	output := CandidatesOutput{
		Timestamp:       time.Now().UTC().Format(time.RFC3339),
		Symbols:         candidates,
		Criteria:        criteria,
		TotalCandidates: len(candidates),
	}

	outputFile := "../../docs/candidates.json"
	file, err := os.Create(outputFile)
	if err != nil {
		log.Fatalf("Error creating output file: %v", err)
	}
	defer file.Close()

	encoder := json.NewEncoder(file)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(output); err != nil {
		log.Fatalf("Error encoding JSON: %v", err)
	}

	log.Printf("Wrote %s", outputFile)
	fmt.Println("\nFilter complete!")
}
