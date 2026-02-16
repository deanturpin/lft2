package main

import (
	"encoding/json"
	"fmt"
	"log"
	"math"
	"os"
	"path/filepath"
	"sort"
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

type SymbolStats struct {
	Symbol        string  `json:"symbol"`
	AvgVolume     float64 `json:"avg_volume"`
	AvgPrice      float64 `json:"avg_price"`
	AvgVolatility float64 `json:"avg_volatility"`
	BarCount      int     `json:"bar_count"`
}

type MarketStats struct {
	VolumeMin    float64 `json:"volume_min"`
	VolumeMax    float64 `json:"volume_max"`
	VolumeMedian float64 `json:"volume_median"`
	PriceMin     float64 `json:"price_min"`
	PriceMax     float64 `json:"price_max"`
	PriceMedian  float64 `json:"price_median"`
	VolMin       float64 `json:"volatility_min"`
	VolMax       float64 `json:"volatility_max"`
	VolMedian    float64 `json:"volatility_median"`
}

type CandidatesOutput struct {
	Timestamp       string         `json:"timestamp"`
	Symbols         []string       `json:"symbols"`
	Criteria        FilterCriteria `json:"criteria"`
	MarketStats     MarketStats    `json:"market_stats"`
	AllSymbols      []SymbolStats  `json:"all_symbols"`
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

func median(values []float64) float64 {
	if len(values) == 0 {
		return 0
	}

	sorted := make([]float64, len(values))
	copy(sorted, values)
	sort.Float64s(sorted)

	mid := len(sorted) / 2
	if len(sorted)%2 == 0 {
		return (sorted[mid-1] + sorted[mid]) / 2
	}
	return sorted[mid]
}

func calculateMarketStats(allStats []SymbolStats) MarketStats {
	if len(allStats) == 0 {
		return MarketStats{}
	}

	volumes := make([]float64, 0, len(allStats))
	prices := make([]float64, 0, len(allStats))
	volatilities := make([]float64, 0, len(allStats))

	for _, s := range allStats {
		volumes = append(volumes, s.AvgVolume)
		prices = append(prices, s.AvgPrice)
		volatilities = append(volatilities, s.AvgVolatility)
	}

	return MarketStats{
		VolumeMin:    math.Floor(minFloat64(volumes)),
		VolumeMax:    math.Ceil(maxFloat64(volumes)),
		VolumeMedian: median(volumes),
		PriceMin:     minFloat64(prices),
		PriceMax:     maxFloat64(prices),
		PriceMedian:  median(prices),
		VolMin:       minFloat64(volatilities),
		VolMax:       maxFloat64(volatilities),
		VolMedian:    median(volatilities),
	}
}

func minFloat64(values []float64) float64 {
	if len(values) == 0 {
		return 0
	}
	min := values[0]
	for _, v := range values[1:] {
		if v < min {
			min = v
		}
	}
	return min
}

func maxFloat64(values []float64) float64 {
	if len(values) == 0 {
		return 0
	}
	max := values[0]
	for _, v := range values[1:] {
		if v > max {
			max = v
		}
	}
	return max
}

func main() {
	log.Println("Filter Module - Identifying candidate stocks")
	log.Println("")

	barsDir := "../../docs/bars"

	if _, err := os.Stat(barsDir); os.IsNotExist(err) {
		log.Fatalf("Error: bars directory not found: %s", barsDir)
	}

	// First pass: load all data and calculate statistics
	entries, err := os.ReadDir(barsDir)
	if err != nil {
		log.Fatalf("Error reading directory: %v", err)
	}

	var allStats []SymbolStats
	totalFiles := 0

	log.Println("Calculating market statistics...")
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

		avgVolume, avgPrice, avgVolatility := calculateStats(barData.Bars)
		allStats = append(allStats, SymbolStats{
			Symbol:        barData.Symbol,
			AvgVolume:     avgVolume,
			AvgPrice:      avgPrice,
			AvgVolatility: avgVolatility,
			BarCount:      barData.Count,
		})
	}

	// Calculate market-wide statistics
	marketStats := calculateMarketStats(allStats)

	log.Println("")
	log.Println("Market Statistics:")
	log.Printf("  Volume:     %.0f - %.0f (median: %.0f)",
		marketStats.VolumeMin, marketStats.VolumeMax, marketStats.VolumeMedian)
	log.Printf("  Price:      $%.2f - $%.2f (median: $%.2f)",
		marketStats.PriceMin, marketStats.PriceMax, marketStats.PriceMedian)
	log.Printf("  Volatility: %.3f%% - %.3f%% (median: %.3f%%)",
		marketStats.VolMin*100, marketStats.VolMax*100, marketStats.VolMedian*100)
	log.Println("")

	// Set criteria based on market statistics
	// Use 25th percentile for volume (accept stocks with reasonable liquidity)
	// Use median volatility (accept stocks with decent movement)
	criteria := FilterCriteria{
		MinAvgVolume:  marketStats.VolumeMedian * 0.5, // Half of median volume
		MinPrice:      10.0,                           // Keep minimum price floor
		MaxPrice:      marketStats.PriceMax * 1.1,     // Allow all prices up to max + 10%
		MinVolatility: marketStats.VolMedian * 0.5,    // Half of median volatility
		MinBarCount:   100,                            // Keep minimum bar count
	}

	log.Println("Filter Criteria (data-driven):")
	log.Printf("  Min avg volume:  %.0f (50%% of median)", criteria.MinAvgVolume)
	log.Printf("  Price range:     $%.2f - $%.2f", criteria.MinPrice, criteria.MaxPrice)
	log.Printf("  Min volatility:  %.3f%% (50%% of median)", criteria.MinVolatility*100)
	log.Printf("  Min bar count:   %d", criteria.MinBarCount)
	log.Println("")

	// Second pass: apply filter criteria
	var candidates []string

	for _, stats := range allStats {
		passes := stats.BarCount >= criteria.MinBarCount &&
			stats.AvgVolume >= criteria.MinAvgVolume &&
			stats.AvgPrice >= criteria.MinPrice &&
			stats.AvgPrice <= criteria.MaxPrice &&
			stats.AvgVolatility >= criteria.MinVolatility

		if passes {
			candidates = append(candidates, stats.Symbol)
			log.Printf("✓ %s (vol: %.0f, price: $%.2f, volatility: %.3f%%)",
				stats.Symbol, stats.AvgVolume, stats.AvgPrice, stats.AvgVolatility*100)
		} else {
			log.Printf("✗ %s (vol: %.0f, price: $%.2f, volatility: %.3f%%)",
				stats.Symbol, stats.AvgVolume, stats.AvgPrice, stats.AvgVolatility*100)
		}
	}

	log.Println("")
	log.Printf("Candidates: %d/%d", len(candidates), totalFiles)

	// Sort symbols by volume (highest first) for readability
	sort.Slice(allStats, func(i, j int) bool {
		return allStats[i].AvgVolume > allStats[j].AvgVolume
	})

	// Write candidates.json
	output := CandidatesOutput{
		Timestamp:       time.Now().UTC().Format(time.RFC3339),
		Symbols:         candidates,
		Criteria:        criteria,
		MarketStats:     marketStats,
		AllSymbols:      allStats,
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
