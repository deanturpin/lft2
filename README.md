# Low Frequency Trader

LFT is a fully automated algorithmic trading platform, written in the latest C++ (26) and designed from the ground up to be `constepxr`, thereby allowing compile-time unit testing and guaranteed memory safety.

## Project status

This is a total redesign of the original [LFT project](https://github.com/deanturpin/lft), which will continue to run in the background whilst this second revision is being developed.

LFTv1 was a good proof of concept, but when you get into the details of why a trade happened it was quite hard to reason about. So this time we'll get all our strategies solid up front in unit tests and ensure that backtest and live behaviours are indistinguishable.

### To be done

- [ ] Compile time strategy implementation
- [ ] Alpaca exchange integration
- [ ] GitHub Actions pipeline (rather than VPS)
