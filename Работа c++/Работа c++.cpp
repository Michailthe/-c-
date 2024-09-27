#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

struct ActivityReport {
    std::string username;
    double activityPercentage;
};

std::mutex reportMutex;
std::condition_variable reportCondition;
std::atomic<bool> stop{ false };
std::vector<ActivityReport> reports;

void sendReportsToServer() {
    WSAData wsaData;
    WORD versionRequested = MAKEWORD(2, 2);
    if (WSAStartup(versionRequested, &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket." << std::endl;
        WSACleanup();
        return;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != 0) {
        std::cerr << "Failed to connect to server." << std::endl;
        closesocket(sock);
        WSACleanup();
        return;
    }

    while (!stop) {
        std::unique_lock<std::mutex> lock(reportMutex);
        reportCondition.wait(lock, [] { return !reports.empty(); });

        std::cout << "Sending reports..." << std::endl;
        for (auto&& report : reports) {
            std::string data = report.username + "," + std::to_string(report.activityPercentage);
            int sent = send(sock, data.c_str(), data.length(), 0);
            if (sent < 0) {
                std::cerr << "Failed to send report to server." << std::endl;
            }
        }
        reports.clear();
    }

    closesocket(sock);
    WSACleanup();
}

void monitorWorkActivity() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    while (!stop) {
        auto currentTime = std::time(nullptr);
        std::unique_lock<std::mutex> lock(reportMutex);
        ActivityReport report;
        report.username = getenv("USERNAME");
        report.activityPercentage = (double)(currentTime % 60) / 60.0;
        reports.push_back(report);
        reportCondition.notify_all();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    std::thread sendThread(sendReportsToServer);
    std::thread monitorThread(monitorWorkActivity);

    sendThread.join();
    monitorThread.join();

    return 0;