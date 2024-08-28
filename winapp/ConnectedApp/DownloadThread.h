#pragma once
#include "CommonObject.h"

class DownloadThread
{
public:
	void run(CommonObjects& common);
	void SetUrl(std::string_view new_url);
private:
	void fetchStockData(CommonObjects& common, const std::string& symbol);
	void loadFavoritesFromFile(CommonObjects& common, const std::string& filename);
	void refreshData(CommonObjects& common);
	bool isFileNotEmpty(const std::string& filename);


	std::string _download_url;
};

