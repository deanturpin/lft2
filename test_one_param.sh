#!/bin/bash
# Test one parameter set and output summary stats

tp=$1
sl=$2
tsl=$3

# Update params.h
sed -i '' "s/take_profit_pct = [0-9.]*,/take_profit_pct = $tp,/" src/params.h
sed -i '' "s/stop_loss_pct = [0-9.]*,/stop_loss_pct = $sl,/" src/params.h  
sed -i '' "s/trailing_stop_pct = [0-9.]*};/trailing_stop_pct = $tsl};/" src/params.h

# Rebuild
make build >/dev/null 2>&1

# Run backtest and extract summary
./backtest USO 2>/dev/null | awk '/mean_reversion:/ {print "USO mean_reversion: " $0}'
./backtest MSTR 2>/dev/null | awk '/mean_reversion:/ {print "MSTR mean_reversion: " $0}'
./backtest XLE 2>/dev/null | awk '/volatility_breakout:/ {print "XLE volatility_breakout: " $0}'
