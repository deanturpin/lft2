// Package alpaca provides a shared HTTP client for the Alpaca API,
// used across all Go modules in the LFT2 trading system.
package alpaca

import (
	"bytes"
	"fmt"
	"io"
	"net/http"
	"time"
)

// Client holds credentials and the base URLs for Alpaca's REST API.
type Client struct {
	APIKey    string
	APISecret string
	BaseURL   string // broker/account API  (paper-api.alpaca.markets)
	DataURL   string // market data API     (data.alpaca.markets)
}

// New returns a Client configured from the supplied credentials.
// baseURL defaults to the paper trading endpoint if empty.
// dataURL defaults to the standard data endpoint if empty.
func New(apiKey, apiSecret, baseURL, dataURL string) Client {
	if baseURL == "" {
		baseURL = "https://paper-api.alpaca.markets"
	}
	if dataURL == "" {
		dataURL = "https://data.alpaca.markets"
	}
	return Client{APIKey: apiKey, APISecret: apiSecret, BaseURL: baseURL, DataURL: dataURL}
}

var httpClient = &http.Client{Timeout: 10 * time.Second}

// Post performs an authenticated POST request with a JSON body and returns the response body.
func (c Client) Post(url string, body []byte) ([]byte, error) {
	req, err := http.NewRequest("POST", url, bytes.NewReader(body))
	if err != nil {
		return nil, err
	}

	req.Header.Set("APCA-API-KEY-ID", c.APIKey)
	req.Header.Set("APCA-API-SECRET-KEY", c.APISecret)
	req.Header.Set("Content-Type", "application/json")

	resp, err := httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("HTTP request failed: %w", err)
	}
	defer resp.Body.Close()

	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("reading response: %w", err)
	}

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d: %s", resp.StatusCode, string(respBody))
	}

	return respBody, nil
}

// Get performs an authenticated GET request and returns the response body.
func (c Client) Get(url string) ([]byte, error) {
	req, err := http.NewRequest("GET", url, nil)
	if err != nil {
		return nil, err
	}

	req.Header.Set("APCA-API-KEY-ID", c.APIKey)
	req.Header.Set("APCA-API-SECRET-KEY", c.APISecret)

	resp, err := httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("HTTP request failed: %w", err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("reading response: %w", err)
	}

	if resp.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("HTTP %d: %s", resp.StatusCode, string(body))
	}

	return body, nil
}
