#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <unordered_set>


struct StockData
{
    std::string symbol;
    std::string name;
    std::string open;
    std::string close;
    std::string high;
    std::string low;
    std::string volume;
    std::string yearHigh;
    std::string yearLow;
    std::string changesPercentage;
    std::string change;
    std::string price;
    std::string exchange;
    float purchase_price = 0.0f;  // New field to store the price at which the stock was purchased
    float totalCost = 0.0f;


};

struct CommonObjects
{
    std::atomic<bool> start_download = false;
    std::atomic<bool> data_ready = false;
    std::atomic<bool> data_refresh = false;
    bool favoritesLoaded = false;


    std::string search;
    std::vector<StockData> stock_data; // Vector for the stocks list
    std::vector<StockData> purchased_stocks; // Vector for portfolio
    std::vector<StockData> favorite_stocks; // Vector for the favorites stocks list
    std::vector<int> purchased_quantities; // Vector to save how much shares I have from specific stock
    std::mutex data_mutex; // Mutex to protect access to shared data
    float money = 4000.0f; // Initial amount of money



};
