# Real-Time Market Data

A lightweight C++ project for building a **real-time market data client** using HTTP and (soon) WebSocket APIs — designed as part of a larger quant trading infrastructure.

## 🚀 Current Progress

- ✅ Basic project structure (`include/`, `src/`, `tests/`, `build/`)
- ✅ Implemented a simple **HTTPClient** using Boost.Beast
- ✅ Supports:
  - GET requests with custom headers & query params
  - POST and POST-JSON requests with automatic content-type handling
- ✅ Added unit test (`http_client_test`) — verified against `https://httpbin.org`

## 🛠️ Build & Run

```bash
# From project root
mkdir build && cd build
cmake ..
make
./http_client_test
