package main

import (
	"os"
	"testing"
)

// --- parseFIX ---

func TestParseFIX_Basic(t *testing.T) {
	line := "8=FIX.5.0SP2|35=D|55=AAPL|54=1|38=10|"
	fields := parseFIX(line)
	if fields["8"] != "FIX.5.0SP2" {
		t.Errorf("tag 8: got %q, want FIX.5.0SP2", fields["8"])
	}
	if fields["35"] != "D" {
		t.Errorf("tag 35: got %q, want D", fields["35"])
	}
	if fields["55"] != "AAPL" {
		t.Errorf("tag 55: got %q, want AAPL", fields["55"])
	}
}

func TestParseFIX_Empty(t *testing.T) {
	fields := parseFIX("")
	if len(fields) != 0 {
		t.Errorf("expected empty map for empty line, got %d entries", len(fields))
	}
}

func TestParseFIX_MalformedPair(t *testing.T) {
	// Pair with no '=' should be silently ignored
	fields := parseFIX("noequals|55=MSFT|")
	if _, ok := fields["noequals"]; ok {
		t.Error("malformed pair should not appear in result")
	}
	if fields["55"] != "MSFT" {
		t.Errorf("tag 55: got %q, want MSFT", fields["55"])
	}
}

func TestParseFIX_ValueContainsEquals(t *testing.T) {
	// Values that contain '=' (e.g. base64 or order IDs) must not be split
	line := "58=text=with=equals|55=TSLA|"
	fields := parseFIX(line)
	if fields["58"] != "text=with=equals" {
		t.Errorf("tag 58: got %q, want text=with=equals", fields["58"])
	}
}

// --- readOrders ---

func writeFixFile(t *testing.T, content string) string {
	t.Helper()
	f, err := os.CreateTemp(t.TempDir(), "*.fix")
	if err != nil {
		t.Fatal(err)
	}
	if _, err := f.WriteString(content); err != nil {
		t.Fatal(err)
	}
	f.Close()
	return f.Name()
}

func TestReadOrders_MissingFile(t *testing.T) {
	orders, err := readOrders("/nonexistent/orders.fix")
	if err != nil {
		t.Errorf("missing file should return nil error, got: %v", err)
	}
	if orders != nil {
		t.Error("missing file should return nil orders")
	}
}

func TestReadOrders_HeartbeatFiltered(t *testing.T) {
	// Heartbeat (MsgType 35=0) should be consumed but not returned as an order
	content := "8=FIX.5.0SP2|35=0|52=2024-01-01T00:00:00Z|58=entries|\n"
	f := writeFixFile(t, content)
	orders, err := readOrders(f)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(orders) != 0 {
		t.Errorf("heartbeat should be filtered out, got %d orders", len(orders))
	}
}

func TestReadOrders_OrderParsed(t *testing.T) {
	content := "8=FIX.5.0SP2|35=D|55=NVDA|11=ORDER_001|54=1|38=5|\n"
	f := writeFixFile(t, content)
	orders, err := readOrders(f)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(orders) != 1 {
		t.Fatalf("expected 1 order, got %d", len(orders))
	}
	if orders[0]["55"] != "NVDA" {
		t.Errorf("symbol tag 55: got %q, want NVDA", orders[0]["55"])
	}
}

func TestReadOrders_MixedContent(t *testing.T) {
	// Heartbeat + two orders + blank line
	content := "" +
		"8=FIX.5.0SP2|35=0|52=2024-01-01T00:00:00Z|58=2 buy order(s)|\n" +
		"\n" +
		"8=FIX.5.0SP2|35=D|55=AAPL|11=ORDER_001|\n" +
		"8=FIX.5.0SP2|35=D|55=MSFT|11=ORDER_002|\n"
	f := writeFixFile(t, content)
	orders, err := readOrders(f)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(orders) != 2 {
		t.Fatalf("expected 2 orders, got %d", len(orders))
	}
	if orders[0]["55"] != "AAPL" || orders[1]["55"] != "MSFT" {
		t.Errorf("unexpected symbols: %q %q", orders[0]["55"], orders[1]["55"])
	}
}
