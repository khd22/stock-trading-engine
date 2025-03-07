# stock-trading-engine

I implemented a real-time stock trading engine in C++. It simulates order generation and matching for 1,024 tickers using lock-free data structures. I implemented customer data structures instead of using maps or dictionaries as requested

## Installing & Running
Clone the repository:
```bash
git clone https://github.com/khd22/stock-trading-engine.git
cd stock-trading-engine

```

Compile the code:

```bash
g++ -std=c++11 -o engine engine.cpp -pthread

```

To run the simulation

```bash
./engine
```

- Note: I made the simulation run for 15 seconds, but you can change that from main. (I added comments for that there)

