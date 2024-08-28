#include "DrawThread.h"
#include "GuiMain.h"
#include "../../shared/ImGuiSrc/imgui.h"
#include <atomic>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cctype>
#include <algorithm>

// Helper function to convert a string to uppercase
std::string toUpper(const std::string& str) {
    std::string upperStr = str;
    std::transform(upperStr.begin(), upperStr.end(), upperStr.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return upperStr;
}

// Helper function to check if a stock is in the data list
bool isStockInList(const std::vector<StockData>& stock_data, const std::string& symbol) {
    std::string symbol_ = toUpper(symbol);
    for (const auto& stock : stock_data) {
        if (stock.symbol == symbol_) {
            return true;
        }
    }
    return false;
}

// Helper function to check if a stock is in the favorites list
bool isStockInFavorites(const std::vector<StockData>& favorites, const std::string& symbol) {
    for (const auto& stock : favorites) {
        if (stock.symbol == symbol) {
            return true;
        }
    }
    return false;
}

// Helper function to remove a stock from the favorites list
void removeStockFromFavorites(CommonObjects& common, const std::string& symbol) {

    // Use a for loop to find and remove the stock
    for (auto it = common.favorite_stocks.begin(); it != common.favorite_stocks.end(); ++it) {
        if (it->symbol == symbol) {
            common.favorite_stocks.erase(it);
            break;  // Exit the loop after removing the stock
        }
    }

    // Update the FavoritesList.txt file
    std::ofstream outFile("FavoritesList.txt");
    if (!outFile) {
        std::cerr << "Can't open the file FavoritesList.txt" << std::endl;
        return;
    }

    for (const auto& stock : common.favorite_stocks) {
        outFile << stock.symbol << std::endl;
    }
    outFile.close();
}

// Main function to draw the ImGui application window
void DrawAppWindow(void* common_ptr) {
    auto common = static_cast<CommonObjects*>(common_ptr); // Static cast more safer
    if (!common) return;

    std::ofstream favoritesFile;
    static auto errorTime = std::chrono::steady_clock::now(); // to avoid spamming the user with the same error message
    static bool showPortfolio = false;
    static bool insufficientFunds = false;


    // Set the window position and size of the main table 
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always); // Forces the position and size to be applied every time the window is drawn
    ImGui::SetNextWindowSize(ImVec2(750, 750), ImGuiCond_Always);

    if (showPortfolio) {
        // Begin the portfolio window
        ImGui::Begin("Portfolio", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

        // Add "Market" button to switch back to the market view
        if (ImGui::Button("Market")) {
            showPortfolio = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            std::lock_guard<std::mutex> lock(common->data_mutex); // Protect shared data
            common->data_refresh.store(true); // The download thread will start his process
        }

        ImGui::SameLine();
        if (ImGui::Button("Exit")) {
            exit(0);
        }

        // Display current balance
        {
            std::lock_guard<std::mutex> lock(common->data_mutex); // Protect shared data
            ImGui::Text("Current Balance: $%.2f", common->money);
        }

        // Begin table for displaying purchased stocks
        if (ImGui::BeginTable("Purchased Stocks Table", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Symbol");
            ImGui::TableSetupColumn("Price");
            ImGui::TableSetupColumn("Quantity");
            ImGui::TableSetupColumn("Total Cost");
            ImGui::TableSetupColumn("Gain/Loss");
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 230.0f);
            ImGui::TableHeadersRow();

            {
                std::lock_guard<std::mutex> lock(common->data_mutex); // Protect shared data
                float total_invested = 0.0f;
                float total_value = 0.0f;

                for (size_t i = 0; i < common->purchased_stocks.size();) {
                    StockData data = common->purchased_stocks[i];
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text(data.symbol.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text(data.price.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", common->purchased_quantities[i]);
                    ImGui::TableSetColumnIndex(3);

                    float totalCost = common->purchased_stocks[i].totalCost; // Total value I paid for the shared I own each stock
                    ImGui::Text("$%.2f", totalCost);


                    // Calculate gain/loss each stock
                    float current_value = std::stof(data.price) * common->purchased_quantities[i]; // Current value of the shares I own
                    float gain_loss = current_value - totalCost;
                    float gain_loss_percentage = (gain_loss / totalCost) * 100;
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextColored(gain_loss >= 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                        "%.2f (%.2f%%)", gain_loss, gain_loss_percentage);

                    total_invested += totalCost;
                    total_value += current_value;

                    ImGui::TableSetColumnIndex(5);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color for Sell button
                    if (ImGui::Button(("Sell##" + data.symbol).c_str())) { // ## used to prevent showing symbol name nex to the sell button 
                        common->money += std::stof(data.price); // The click event is detected at the point in the loop where i corresponds to (how the i know what i remove)
                        if (common->purchased_quantities[i] > 1) {
                            common->purchased_quantities[i] -= 1;
                            common->purchased_stocks[i].totalCost -= common->purchased_stocks[i].purchase_price; // Adjust total cost

                            ++i; // Increment index only if not erasing
                        }
                        else { // handles the case where user sell his last share of specific stock
                            common->purchased_stocks.erase(common->purchased_stocks.begin() + i);
                            common->purchased_quantities.erase(common->purchased_quantities.begin() + i);
                        }
                    }
                    else { // handles the case where the "Sell" button was not clicked.
                        ++i;
                    }
                    ImGui::PopStyleColor();

                }

                // Display overall portfolio performance
                float total_gain_loss = total_value - total_invested;
                float total_gain_loss_percentage = 0.0f;
                if (total_invested > 0.0f) {
                    total_gain_loss_percentage = (total_gain_loss / total_invested) * 100.0f;
                }

                StockData data;
                ImGui::Separator();
                ImGui::Text("Total Invested: $%.2f", total_invested);
                ImGui::Text("Total Current Value: $%.2f", total_value);
                ImGui::TextColored(total_gain_loss >= 0 ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                    "Total Gain/Loss: $%.2f (%.2f%%)", total_gain_loss, total_gain_loss_percentage);
            }
            ImGui::EndTable();
        }
        ImGui::End(); // End the "Portfolio" window
    }

    else {
        // Begin the market window
        ImGui::Begin("Stock Market Tracker", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Text("Welcome to our Stock Market Application - Track real-time stock data for various companies.");
        static char buff[200]; // For searching stocks
        ImGui::InputText("Stock Symbol", buff, sizeof(buff));
        ImGui::SameLine();

        static bool stockAlreadyInList = false;
        // Search button logic: Only add the stock if it is not already in the list
        if (ImGui::Button("Search")) {
            std::lock_guard<std::mutex> lock(common->data_mutex); // Protect shared data
            if (!isStockInList(common->stock_data, buff)) {
                common->search = buff;
                common->start_download.store(true); // The download thread will start his process
            }
            else {
                stockAlreadyInList = true;
                errorTime = std::chrono::steady_clock::now(); // Update error time

            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            std::lock_guard<std::mutex> lock(common->data_mutex); // Protect shared data
            common->data_refresh.store(true); // The download thread will start his process
        }

        ImGui::SameLine();
        if (ImGui::Button("Portfolio")) { // Add "Portfolio" button
            showPortfolio = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Exit")) {
            exit(0);
        }

        static bool stockAlreadyInFavorites = false;

        // Check if new data is ready to be displayed
        if (common->data_ready.load()) {
            // Begin table for displaying stock data
            if (ImGui::BeginTable("Market Stock", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                // Set the headers
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Price");
                ImGui::TableSetupColumn("Open");
                ImGui::TableSetupColumn("Close");
                ImGui::TableHeadersRow();

                {
                    std::lock_guard<std::mutex> lock(common->data_mutex); // Protect shared data

                    // Display each stock's data in the table
                    for (const auto& data : common->stock_data) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text(data.name.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text(data.price.c_str());
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text(data.open.c_str());
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text(data.close.c_str());
                        ImGui::TableSetColumnIndex(4);

                        // Show additional stock details
                        if (ImGui::TreeNode(("Details##" + data.symbol).c_str())) {
                            ImGui::Text("Day High: %s", data.high.c_str());
                            ImGui::Text("Day Low: %s", data.low.c_str());
                            ImGui::Text("Year High: %s", data.yearHigh.c_str());
                            ImGui::Text("Year Low: %s", data.yearLow.c_str());
                            ImGui::Text("Change: %s", data.change.c_str());
                            ImGui::Text("Exchange: %s", data.exchange.c_str());
                            ImGui::Text("Volume: %s", data.volume.c_str());

                            ImGui::TreePop();
                        }

                        ImGui::TableSetColumnIndex(5);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
                        if (ImGui::Button(("Add to favorites##" + data.symbol).c_str())) {
                            // Add the stock to favorites if not already in the list
                            if (isStockInFavorites(common->favorite_stocks, data.symbol)) {
                                stockAlreadyInFavorites = true;
                                errorTime = std::chrono::steady_clock::now();
                            }
                            else {
                                common->favorite_stocks.push_back(data);
                                favoritesFile.open("FavoritesList.txt", std::ios_base::app);
                                if (!favoritesFile) {
                                    std::cerr << "Can't open the file FavoritesList.txt" << std::endl;
                                    return;
                                }

                                favoritesFile << data.symbol << std::endl;
                                favoritesFile.close();
                            }
                        }
                        ImGui::PopStyleColor();



                        ImGui::TableSetColumnIndex(6); // Add this column index
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.7f, 0.0f, 1.0f));
                        if (ImGui::Button(("Buy##" + data.symbol).c_str())) {

                            float cost = std::stof(data.price);
                            if (cost <= common->money) {
                                bool ownShares = false;
                                size_t index = 0;
                                common->money -= cost;

                                for (size_t i = 0; i < common->purchased_stocks.size(); ++i) { // Checking if user already own shares of the stock
                                    if (common->purchased_stocks[i].symbol == data.symbol) {
                                        index = i;
                                        ownShares = true;
                                        break;
                                    }
                                }

                                if (ownShares) {
                                    common->purchased_quantities[index] += 1;
                                    common->purchased_stocks[index].purchase_price = cost;
                                    common->purchased_stocks[index].totalCost += cost;
                                    ownShares = false;

                                }

                                else {
                                    StockData purchased_data = data;
                                    purchased_data.purchase_price = cost;  // Store the purchase price
                                    purchased_data.totalCost = cost;
                                    common->purchased_stocks.push_back(purchased_data);
                                    common->purchased_quantities.push_back(1);
                                }
                            }

                            else {
                                insufficientFunds = true;
                                errorTime = std::chrono::steady_clock::now();
                            }
                        }
                        ImGui::PopStyleColor();

                    }
                }
                ImGui::EndTable();
            }

            // Display a red message if the stock is already in the list, and do so for 3 seconds
            if (stockAlreadyInList) {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Stock is already in the list!");
                if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - errorTime).count() > 1) {
                    stockAlreadyInList = false; // Reset the flag after 3 seconds
                }
            }

            // Display error message if stock is already in favorites
            if (stockAlreadyInFavorites) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Stock already exists in favorites!");
                if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - errorTime).count() > 1) {
                    stockAlreadyInFavorites = false;
                }
            }

            // Display error message if there are insufficient funds
            if (insufficientFunds) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Insufficient funds to buy stock!");
                if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - errorTime).count() > 1) {
                    insufficientFunds = false;
                }
            }

            // Set the window position and size of the favorites table
            ImGui::SetNextWindowPos(ImVec2(770, 0), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(480, 500), ImGuiCond_Always);

            // Begin table for displaying favorite stocks
            ImGui::Begin("Favorite Stocks", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
            if (ImGui::BeginTable("Favorite Stocks Table", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Symbol");
                ImGui::TableSetupColumn("Price");
                ImGui::TableHeadersRow();

                {
                    std::lock_guard<std::mutex> lock(common->data_mutex); // Protect shared data

                    for (const auto& data : common->favorite_stocks) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text(data.symbol.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text(data.price.c_str());
                        ImGui::TableSetColumnIndex(2);

                        if (ImGui::TreeNode(("Show Info##" + data.symbol).c_str())) {
                            ImGui::Text("Open: %s", data.open.c_str());
                            ImGui::Text("Close: %s", data.close.c_str());
                            ImGui::Text("Day High: %s", data.high.c_str());
                            ImGui::Text("Day Low: %s", data.low.c_str());
                            ImGui::Text("Year High: %s", data.yearHigh.c_str());
                            ImGui::Text("Year Low: %s", data.yearLow.c_str());
                            ImGui::Text("Change: %s", data.change.c_str());
                            ImGui::Text("Exchange: %s", data.exchange.c_str());
                            ImGui::Text("Volume: %s", data.volume.c_str());
                            ImGui::TreePop();
                        }
                        ImGui::TableSetColumnIndex(3);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Red color for remove button
                        if (ImGui::Button(("Remove##" + data.symbol).c_str())) {
                            removeStockFromFavorites(*common, data.symbol);
                        }
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::End(); // End the "Favorite Stocks" window
        }
        ImGui::End(); // End the "Stock Market Tracker" window
    }
}

void DrawThread::run(CommonObjects& common) {
    GuiMain(DrawAppWindow, &common);
}