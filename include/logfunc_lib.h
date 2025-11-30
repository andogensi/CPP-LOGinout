#pragma once

#include <string_view>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <future>
#include <chrono>
#include <thread>
#include <type_traits>


namespace logfunc_internal {
class FileCache;
FileCache& get_file_cache();
std::string& get_log_file_path();
void write_atomic_to_file(std::string_view path, const std::string& content);

}
void init_log(std::string_view log_path);
void init_input(std::string_view input_path);


void log_flush(std::string_view filepath = {});

void log_close_all();
void log_set_silent_mode(bool silent);
bool log_is_silent_mode();


template<typename... Args>
void logff(Args&&... args) {
  
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    logfunc_internal::write_atomic_to_file(
        logfunc_internal::get_log_file_path(), oss.str());
}
template<typename... Args>
void logto(std::string_view filepath, Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
  
    logfunc_internal::write_atomic_to_file(filepath, oss.str());
}

template<typename... Args>
void logc(Args&&... args) {
    (std::cout << ... << std::forward<Args>(args));
}
template<typename... Args>
void logc_safe(Args&&... args) {
    static std::mutex cout_mtx;
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    std::lock_guard<std::mutex> lock(cout_mtx);
    std::cout << oss.str();
}

void loginf(int& value);
void loginf(float& value);
void loginf(double& value);
bool loginf_try(int& value);
bool loginf_try(float& value);
bool loginf_try(double& value);
bool loginf_timeout(int& value, std::chrono::milliseconds timeout);
bool loginf_timeout(float& value, std::chrono::milliseconds timeout);
bool loginf_timeout(double& value, std::chrono::milliseconds timeout);

std::future<int> loginf_async_int();
std::future<float> loginf_async_float();
std::future<double> loginf_async_double();
template<typename T, typename Callback>
void loginf_async(Callback callback) {
    std::thread([callback = std::move(callback)]() {
        T value{};
        if constexpr (std::is_same_v<T, int>) {
            loginf(value);
        } else if constexpr (std::is_same_v<T, float>) {
            loginf(value);
        } else if constexpr (std::is_same_v<T, double>) {
            loginf(value);
        }
        callback(value);
    }).detach();
}
template<typename T>
void loginc(T& value) {
    std::string line;
    if (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        iss >> value;
    }
}
