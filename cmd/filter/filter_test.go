package main

import (
	"math"
	"testing"
)

// makeBar is a helper that creates a Bar with close=c, high=c+spread, low=c-spread.
func makeBar(c, spread float64, volume int64) Bar {
	return Bar{Close: c, High: c + spread, Low: c - spread, Volume: volume}
}

// makeBars returns n identical bars.
func makeBars(n int, c, spread float64, volume int64) []Bar {
	bars := make([]Bar, n)
	for i := range bars {
		bars[i] = makeBar(c, spread, volume)
	}
	return bars
}

// --- calculateStats ---

func TestCalculateStats_Empty(t *testing.T) {
	vol, price, vty := calculateStats(nil)
	if vol != 0 || price != 0 || vty != 0 {
		t.Errorf("expected zeros for empty bars, got %.0f %.2f %.4f", vol, price, vty)
	}
}

func TestCalculateStats_Single(t *testing.T) {
	bars := []Bar{makeBar(100.0, 1.0, 1000)}
	vol, price, vty := calculateStats(bars)
	if vol != 1000 {
		t.Errorf("avg volume: got %.0f, want 1000", vol)
	}
	if price != 100.0 {
		t.Errorf("avg price: got %.2f, want 100.00", price)
	}
	// volatility = (high-low)/close = 2/100 = 0.02
	if math.Abs(vty-0.02) > 1e-9 {
		t.Errorf("avg volatility: got %.6f, want 0.020000", vty)
	}
}

func TestCalculateStats_Multiple(t *testing.T) {
	bars := []Bar{
		makeBar(100.0, 1.0, 1000),
		makeBar(200.0, 2.0, 3000),
	}
	vol, price, vty := calculateStats(bars)
	if vol != 2000 {
		t.Errorf("avg volume: got %.0f, want 2000", vol)
	}
	if price != 150.0 {
		t.Errorf("avg price: got %.2f, want 150.00", price)
	}
	// bar1: 2/100=0.02, bar2: 4/200=0.02 → avg 0.02
	if math.Abs(vty-0.02) > 1e-9 {
		t.Errorf("avg volatility: got %.6f, want 0.020000", vty)
	}
}

// --- median ---

func TestMedian_Empty(t *testing.T) {
	if median(nil) != 0 {
		t.Error("expected 0 for empty slice")
	}
}

func TestMedian_OddCount(t *testing.T) {
	got := median([]float64{3, 1, 2})
	if got != 2 {
		t.Errorf("got %.1f, want 2.0", got)
	}
}

func TestMedian_EvenCount(t *testing.T) {
	got := median([]float64{1, 2, 3, 4})
	if got != 2.5 {
		t.Errorf("got %.1f, want 2.5", got)
	}
}

// --- filterReason ---

func barData(symbol string, bars []Bar) *BarData {
	return &BarData{Symbol: symbol, Bars: bars, Count: len(bars)}
}

var defaultCriteria = FilterCriteria{
	MinAvgVolume:   1000,
	MinPrice:       10.0,
	MaxPrice:       500.0,
	MinVolatility:  0.001,
	MinBarCount:    100,
	MaxBarRangePct: 0.5,
}

func TestFilterReason_InsufficientBars(t *testing.T) {
	bars := makeBars(50, 100.0, 0.5, 2000)
	reason := filterReason(barData("X", bars), defaultCriteria)
	if reason == "" {
		t.Error("expected rejection for insufficient bars, got pass")
	}
}

func TestFilterReason_LowVolume(t *testing.T) {
	bars := makeBars(100, 100.0, 0.5, 100) // volume 100, below 1000 minimum
	reason := filterReason(barData("X", bars), defaultCriteria)
	if reason == "" {
		t.Error("expected rejection for low volume, got pass")
	}
}

func TestFilterReason_PriceTooLow(t *testing.T) {
	bars := makeBars(100, 5.0, 0.01, 5000) // price $5, below $10 minimum
	reason := filterReason(barData("X", bars), defaultCriteria)
	if reason == "" {
		t.Error("expected rejection for low price, got pass")
	}
}

func TestFilterReason_PriceTooHigh(t *testing.T) {
	bars := makeBars(100, 600.0, 1.0, 5000) // price $600, above $500 maximum
	reason := filterReason(barData("X", bars), defaultCriteria)
	if reason == "" {
		t.Error("expected rejection for high price, got pass")
	}
}

func TestFilterReason_LowVolatility(t *testing.T) {
	// spread=0.0001 → volatility = 0.0002/100 = 0.000002, below 0.001 minimum
	bars := makeBars(100, 0.0001, 2000, 2000)
	reason := filterReason(barData("X", bars), defaultCriteria)
	if reason == "" {
		t.Error("expected rejection for low volatility, got pass")
	}
}

func TestFilterReason_SpreadTooWide(t *testing.T) {
	// 99 bars that pass everything, then one last bar with wide spread
	bars := makeBars(99, 100.0, 0.1, 2000)
	// Last bar: spread=1.0 → range = 2/100 = 2% → exceeds 0.5% limit
	bars = append(bars, makeBar(100.0, 1.0, 2000))
	reason := filterReason(barData("X", bars), defaultCriteria)
	if reason == "" {
		t.Error("expected rejection for wide spread, got pass")
	}
}

func TestFilterReason_Passes(t *testing.T) {
	// 100 bars: price $100, spread $0.1 → range 0.2% < 0.5%, vol 0.002 > 0.001
	bars := makeBars(100, 100.0, 0.1, 2000)
	reason := filterReason(barData("X", bars), defaultCriteria)
	if reason != "" {
		t.Errorf("expected pass, got: %s", reason)
	}
}
