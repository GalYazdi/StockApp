// ConnectedApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <thread>
#include "CommonObject.h"
#include "DrawThread.h"
#include "DownloadThread.h"



int main()
{
    CommonObjects common;
    DrawThread draw;
    auto draw_th = std::jthread([&] {draw.run(common); });
    DownloadThread down;
    auto down_th = std::jthread([&] {down.run(common); });
    std::cout << "Running...\n";
    down_th.join();
    draw_th.join();
}

