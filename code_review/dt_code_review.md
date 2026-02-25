# General code review 17 Feb 2026

## Documentation

- is DEPLOYMENT.md still required/relevant?
- update all claude docs (including ignore)
- what is this web/README.md for? Can we consolidate with About and/or top level readme?

## Go

- why go stuff at top level? go.work
- review alpaca usage across all Go modules, seems a bit light: internal/alpaca/alpaca.go
- do we accept duplication of alpaca account access in worker code: workers/account.js?
  I.e., could we actually do this in Go?

## Misc

- what's compile_commands.json? why generate, why ignore?
- consider renaming prices/ to be clearly saved examples data... snapshot examples?
- report full watchlist in github pages: watchlist.json
