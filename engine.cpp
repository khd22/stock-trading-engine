#include <iostream>
#include <atomic>
#include <thread>
#include <random>
#include <chrono>
#include <cstdint>

using namespace std;


constexpr int MAX_ORDERS_PER_SIDE = 1024;
constexpr int NUM_TICKERS = 1024; 


atomic<uint64_t> globalTimestamp{0}; // timestamp to track the order of arriving
atomic<bool> stopSimulation{false};

// Order structure to represent each order on the stock exchange
struct Order {
    bool isBuy;                
    uint32_t tickerIndex;      // I used numbers as symbols 
    atomic<uint32_t> quantity; // remaining quantity 
    double price;              
    uint64_t timestamp;        

    // Constructor
    Order(bool type, uint32_t index, uint32_t quant, double p, uint32_t time){
        isBuy = type;
        tickerIndex = index;
        quantity = quant;
        price = p;
        timestamp = time;
    }

    // default constructor
    Order(){
        isBuy = false;
        tickerIndex = 0;
        quantity = 0;
        price = 0;
        timestamp = 0;
    }
};

//  I used this to track the buys and sells for each ticker
// Arrays are used here since the number is fixed
struct TickerOrderBook {
    Order* buyOrders[MAX_ORDERS_PER_SIDE];
    Order* sellOrders[MAX_ORDERS_PER_SIDE];

    atomic<int> buyCount;
    atomic<int> sellCount;

    TickerOrderBook() : buyCount(0), sellCount(0) {
        for (int i = 0; i < MAX_ORDERS_PER_SIDE; ++i) {
            buyOrders[i] = nullptr;
            sellOrders[i] = nullptr;
        }
    }
};

TickerOrderBook tickers[NUM_TICKERS]; // Global array for all tickers


//
// addOrder:
//   Dynamically allocates a new Order and stores its pointer in the appropriate array(buy or sell).
//   Parameters: isBuy , tickerSymbol(used numbers for this), quantity, price.
//
void addOrder(bool isBuy, uint16_t tickerSymbol, uint32_t quantity, double price) {
    
    uint64_t timestamp = globalTimestamp.fetch_add(1, memory_order_relaxed);

    // check if it is within the range the exchange supports
    if (tickerSymbol >= NUM_TICKERS)
        return;
    
    // if it is a buy order, add it to the buy array
    if (isBuy) {
        int index = tickers[tickerSymbol].buyCount.fetch_add(1, memory_order_relaxed);
        if (index < MAX_ORDERS_PER_SIDE) {
            tickers[tickerSymbol].buyOrders[index] = new Order(isBuy, tickerSymbol, quantity, price, timestamp);
        }
    }
    // if it is a sell order, add it to the sell array
    else {
        int index = tickers[tickerSymbol].sellCount.fetch_add(1, memory_order_relaxed);
        if (index < MAX_ORDERS_PER_SIDE) {
            tickers[tickerSymbol].sellOrders[index] = new Order(isBuy, tickerSymbol, quantity, price, timestamp);
        }
    }
}

//
// matchOrdersForTicker:
//   The function finds the best buy order (highest price) and best sell order (lowest price)
//   and if the buy price is at least the sell price, it matches them. 
//   Also, it updates the quantity then print the trade details.
void matchOrdersForTicker(uint16_t tickerIndex) {
    TickerOrderBook &book = tickers[tickerIndex];
    while (true) {
        int buyCount  = book.buyCount.load(memory_order_relaxed);
        int sellCount = book.sellCount.load(memory_order_relaxed);

        Order *bestBuy = nullptr;
        Order *bestSell = nullptr;
        double highestBuyPrice = -1.0;
        double lowestSellPrice = 1e9;  // just a random large number

        // Scan for the best buy order (with quantity > 0)
        for (int i = 0; i < buyCount; i++) {
            Order *o = book.buyOrders[i];
            if (o && o->quantity.load(memory_order_relaxed) > 0 && o->price > highestBuyPrice) {
                highestBuyPrice = o->price;
                bestBuy = o;
            }
        }

        // Scan for the best sell order (with quantity > 0)
        for (int i = 0; i < sellCount; i++) {
            Order *o = book.sellOrders[i];
            if (o && o->quantity.load(memory_order_relaxed) > 0 && o->price < lowestSellPrice) {
                lowestSellPrice = o->price;
                bestSell = o;
            }
        }

        // If no valid match is found, exit loop
        if (!bestBuy || !bestSell || bestBuy->price < bestSell->price)
            break;

        // Calculate the quantity to match then substract it
        uint32_t buyQty  = bestBuy->quantity.load(memory_order_relaxed);
        uint32_t sellQty = bestSell->quantity.load(memory_order_relaxed);
        uint32_t matchQty = (buyQty < sellQty) ? buyQty : sellQty;

        bestBuy->quantity.fetch_sub(matchQty);
        bestSell->quantity.fetch_sub(matchQty);

        cout << "Ticker " << tickerIndex << " matched trade: "
             << matchQty << " shares at price " << bestSell->price << "\n";
    }
}

//
// matchOrder:
//   Iterates over all tickers and calls matchOrdersForTicker for each one.
//   The overall time-complexity is O(n) per ticker's order book since we are iterating the fixed arrays.
//
void matchOrder() {
    for (uint16_t ticker = 0; ticker < NUM_TICKERS; ticker++) {
        matchOrdersForTicker(ticker);
    }
}

//
// simulateOrders:
//   This function is a wrapper for addOrder and it would call it with random variables.
//
void simulateOrders() {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> tickerDist(0, NUM_TICKERS - 1);
    uniform_int_distribution<> qtyDist(1, 1000);
    uniform_real_distribution<> priceDist(10.0, 500.0);
    bernoulli_distribution typeDist(0.5);  // 50% chance buy or sell to make it balanced

    while (!stopSimulation.load(memory_order_relaxed)) {
        bool isBuy = typeDist(gen);
        uint16_t ticker = tickerDist(gen);
        uint32_t quantity = qtyDist(gen);
        double price = priceDist(gen);
        addOrder(isBuy, ticker, quantity, price);
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

//
// simulateMatching:
//   Continuously calls matchOrder() to process and match orders across all tickers.
//
void simulateMatching() {
    while (!stopSimulation.load(memory_order_relaxed)) {
        matchOrder();
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

//
// main:
//   Launches threads for order simulation and order matching.
//   The threads would run for 50 seconds before stopping
//
int main() {
    thread orderThread(simulateOrders);
    thread matchThread(simulateMatching);

    this_thread::sleep_for(chrono::seconds(15));  // you can change this for longer or shorter simulation
    stopSimulation.store(true, memory_order_relaxed);

    orderThread.join();
    matchThread.join();

    cout << "Simulation finished." << "\n";
    return 0;
}
