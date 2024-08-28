#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "DownloadThread.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include <atomic>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <thread>

// Helper function to format floats to 2 decimal places as strings
std::string formatFloat(float value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << value;
    return out.str();
}


// Checking if favorites file is not empty
bool DownloadThread::isFileNotEmpty(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate); // Positions the file pointer at the end of the file
    return file.tellg() > 0; // Returns the current position of the file pointer
}



// API key for accessing financial data
const std::string API_KEY = "jLziQYDeIANzzclVWfa5oYCExoVdzuNp";

// List of predefined stock symbols to fetch data for
std::vector<std::string> stockList = {
    "AAPL",  // Apple Inc.
    "MSFT",  // Microsoft Corporation
    "GOOGL", // Alphabet Inc. (Google)
    "AMZN",  // Amazon.com, Inc.
    "TSLA",  // Tesla, Inc.
    "NFLX",  // Netflix, Inc.
    "NVDA",  // NVIDIA Corporation
    "WMT",   // Walmart Inc.
    "BA"     // The Boeing Company
};

// Function to parse JSON data and create a StockData object
StockData parseStockData(const nlohmann::json& stock_info) {
    StockData data;
    data.symbol = stock_info["symbol"].get<std::string>();
    data.name = stock_info["name"].get<std::string>();
    data.open = formatFloat(stock_info["open"].get<float>());
    data.close = formatFloat(stock_info["previousClose"].get<float>());
    data.high = formatFloat(stock_info["dayHigh"].get<float>());
    data.low = formatFloat(stock_info["dayLow"].get<float>());
    data.volume = std::to_string(stock_info["volume"].get<int>());
    data.yearHigh = formatFloat(stock_info["yearHigh"].get<float>());
    data.yearLow = formatFloat(stock_info["yearLow"].get<float>());
    data.changesPercentage = formatFloat(stock_info["changesPercentage"].get<float>());
    data.change = formatFloat(stock_info["change"].get<float>()); // The absolute change in the stock price compared to the previous close
    data.price = formatFloat(stock_info["price"].get<float>());
    data.exchange = stock_info["exchange"].get<std::string>();

    return data;
}

// Function to fetch stock data from the API
void DownloadThread::fetchStockData(CommonObjects& common, const std::string& symbol) {
    httplib::Client cli("https://financialmodelingprep.com"); // creates an HTTP client instance that will be used to communicate with the server at the data provider site
    std::string url = "/api/v3/quote/" + symbol + "?apikey=" + API_KEY;
    auto res = cli.Get(url.c_str()); // Send a GET request to the API and store the response (c_str convert from string to char*)

    if (res && res->status == 200) {  // Check if the response is valid
        auto json_result = nlohmann::json::parse(res->body); // Parse the response body from an HTTP request into a JSON object

        if (!json_result.empty() && json_result.is_array() && json_result.size() > 0) {
            StockData data = parseStockData(json_result[0]);  // Parse the first element of the JSON array to create a StockData object

            std::lock_guard<std::mutex> lock(common.data_mutex); // Protect shared data

            if (common.favoritesLoaded) { // in case of stocks from vector exists in the favorites list
                for (auto& fav : common.favorite_stocks) {
                    if (fav.symbol == data.symbol) {
                        common.stock_data.push_back(fav);
                        common.data_ready.store(true); // Set the data_ready flag to true to indicate new data is available
                        return;
                    }
                }
            }

            if (!common.favoritesLoaded) { // Check if favorites have not been loaded yet
                common.favorite_stocks.push_back(data);
            }
            else {
                common.stock_data.push_back(data);

            }
            for (auto& stock : common.purchased_stocks) {
                if (stock.symbol == data.symbol) {
                    stock.price = data.price;
                }
            }
            common.data_ready.store(true); // Set the data_ready flag to true to indicate new data is available

        }
        else {
            std::cerr << "JSON response is empty or not an array for symbol: " << symbol << std::endl;
        }
    }
    else {
        std::cerr << "Failed to fetch data for " << symbol << ": " << (res ? res->status : 0) << std::endl;
        if (res) {
            std::cerr << "Response body: " << res->body << std::endl;
        }
    }
}

// Helper function to load favorite stocks from a file
void DownloadThread::loadFavoritesFromFile(CommonObjects& common, const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }

    std::string symbol;
    while (file >> symbol) { // While there is symbols in the Favorites file
        fetchStockData(common, symbol);
    }
    file.close();
}

// Main function of the download thread
void DownloadThread::run(CommonObjects& common) {
    // Load favorite stocks from file
    if (isFileNotEmpty("FavoritesList.txt") && !common.favoritesLoaded) {
        loadFavoritesFromFile(common, "FavoritesList.txt");
    }
    common.favoritesLoaded = true;

    // Fetch data for pre-defined stock symbols
    for (const auto& symbol : stockList) {
        fetchStockData(common, symbol);
    }

    // Continuous loop to fetch data based on user requests
    while (true) {
        if (common.start_download.load()) {
            fetchStockData(common, common.search);
            stockList.push_back(common.search);
            common.start_download.store(false);
        }

        if (common.data_refresh.load()) {
            {
                std::lock_guard<std::mutex> lock(common.data_mutex); // Protect shared data
                // Set flags to safely handle refresh
                common.favoritesLoaded = false;
                common.data_refresh.store(false);
            }

            // Clear and reload data outside of the mutex
            refreshData(common);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(800));
    }
}

void DownloadThread::refreshData(CommonObjects& common) {
    {
        std::lock_guard<std::mutex> lock(common.data_mutex); // Protect shared data
        common.stock_data.clear();
        common.favorite_stocks.clear();
    }

    // Re-load favorite stocks 
    if (isFileNotEmpty("FavoritesList.txt")) {
        loadFavoritesFromFile(common, "FavoritesList.txt");
    }

    common.favoritesLoaded = true;

    for (const auto& symbol : stockList) {
        fetchStockData(common, symbol);
    }
}

