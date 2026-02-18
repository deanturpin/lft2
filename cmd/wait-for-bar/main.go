package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"time"

	"github.com/deanturpin/lft2/internal/alpaca"
)

// Alpaca clock response
type Clock struct {
	Timestamp string `json:"timestamp"`
	IsOpen    bool   `json:"is_open"`
	NextOpen  string `json:"next_open"`
	NextClose string `json:"next_close"`
}

func fetchClock(client alpaca.Client) (*Clock, error) {
	body, err := client.Get(client.BaseURL + "/v2/clock")
	if err != nil {
		return nil, err
	}

	var clock Clock
	if err := json.Unmarshal(body, &clock); err != nil {
		return nil, err
	}

	return &clock, nil
}

func main() {
	fmt.Println("Low Frequency Trader v2 - Wait for Bar\n")

	apiKey := os.Getenv("ALPACA_API_KEY")
	apiSecret := os.Getenv("ALPACA_API_SECRET")

	if apiKey == "" || apiSecret == "" {
		log.Fatal("ALPACA_API_KEY and ALPACA_API_SECRET must be set")
	}

	client := alpaca.New(apiKey, apiSecret, os.Getenv("ALPACA_BASE_URL"), "")

	clock, err := fetchClock(client)
	if err != nil {
		log.Fatalf("Failed to fetch exchange clock: %v", err)
	}

	fmt.Printf("Exchange time: %s\n", clock.Timestamp)
	fmt.Printf("Market open:   %v\n", clock.IsOpen)

	if !clock.IsOpen {
		fmt.Printf("Next open:     %s\n", clock.NextOpen)
		fmt.Println("\nMarket is closed - exiting loop")
		os.Exit(1)
	}

	// Parse exchange timestamp from Alpaca (RFC3339)
	exchangeNow, err := time.Parse(time.RFC3339Nano, clock.Timestamp)
	if err != nil {
		log.Fatalf("Failed to parse exchange time: %v", err)
	}

	// Calculate next 5-minute boundary, offset by 35 seconds per Alpaca bar publishing delay
	// Alpaca publishes bars ~35 seconds after the bar closes
	const barIntervalMin = 5
	const publishDelaySec = 35

	currentMin := exchangeNow.Minute()
	currentSec := exchangeNow.Second()

	// Minutes until next 5-min boundary
	nextBoundaryMin := ((currentMin/barIntervalMin)+1)*barIntervalMin - currentMin
	waitSec := nextBoundaryMin*60 - currentSec + publishDelaySec

	// Calculate the target time
	target := exchangeNow.Add(time.Duration(waitSec) * time.Second).Truncate(time.Second)

	fmt.Printf("\nCurrent bar:   %02d:%02d (exchange time)\n", exchangeNow.Hour(), exchangeNow.Minute())
	fmt.Printf("Next bar at:   %02d:%02d + %ds publish delay\n",
		target.Add(-time.Duration(publishDelaySec)*time.Second).Hour(),
		target.Add(-time.Duration(publishDelaySec)*time.Second).Minute(),
		publishDelaySec)
	fmt.Printf("Waiting until: %02d:%02d:%02d UTC (%ds)\n",
		target.Hour(), target.Minute(), target.Second(), waitSec)

	time.Sleep(time.Duration(waitSec) * time.Second)

	fmt.Println("\nBar data should now be available")
}
