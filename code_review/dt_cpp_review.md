# cpp review 17 Feb 2026

## Misc

- why tabs in the code? should we install default clang format at top level?

## Code

Prefer no braces for single line if statements, review whole codebase

```cpp
 if (trades.empty()) {
  return result;
 }
```

Create a file cxx module to hide all these paths: "docs/candidates.json", "docs/bars/{}.json" (and others)

Use starts_with instead of array index zero? `s[0]`

Can we use OR and simplify logic?

```cpp
  if (!(b.high >= b.close && b.high >= b.open && b.high >= b.low &&
        b.low <= b.close && b.low <= b.open))
    return false;
```

##  Review JSON parsing (move to header)

```cpp
  auto obj_start = content.find('{', pos);
```

```cpp
  auto obj_start = json_str.find('{', pos);
  if (obj_start == std::string::npos) break;
  auto obj_end = json_str.find('}', obj_start);
  if (obj_end == std::string::npos) break;
```

## hardcoded strategy... do we need a lookup/map

```cpp
  if (candidate.strategy == "volume_surge_dip")
   should_enter = volume_surge_dip(bars);
  else if (candidate.strategy == "mean_reversion")
   should_enter = mean_reversion(bars);
  else if (candidate.strategy == "sma_crossover")
   should_enter = sma_crossover(bars);
```

## what's the mock thing

```cpp
   auto mock_position = position{
    .entry_price = pos.avg_entry_price,
    .take_profit = pos.avg_entry_price * 1.10,
    .stop_loss = pos.avg_entry_price * 0.95,
    .trailing_stop = pos.avg_entry_price * 0.95  // Start at stop loss level
   };
```

## Redundant files/code

Review if we still needs this and delete if necessary.

- src/fetch.cxx
- src/main.cxx

## FIX

src/fix.h

Can this be normal constexpr functions rather than a class? Not sure what that adds.

## Entires/Exits

- why is params.h not used by exits code?
- How do we get trading info into the description field of new trades? We should be
  pushing as much info as possible into Alpaca for later analysis
-
