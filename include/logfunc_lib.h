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
#include <filesystem>

/**
 * @brief ログ機能を提供するLoggerクラス（ライブラリ版）
 * 
 * 状態をインスタンス変数として管理し、複数のロガーインスタンスを
 * 同時に使用可能。テスト容易性と拡張性を向上させる設計。
 */
class Logger {
public:
    struct InputFileCache {
        std::filesystem::file_time_type last_check_time;
        std::filesystem::file_time_type last_modify_time;
        std::chrono::steady_clock::time_point last_access;
        bool file_exists = false;
        static constexpr std::chrono::milliseconds cache_duration{10};
    };

private:
    std::unordered_map<std::string, std::unique_ptr<std::ofstream>> handles_;
    mutable std::mutex mtx_;
    std::unique_ptr<std::ofstream> null_stream_;
    bool silent_mode_ = true;
    std::string log_file_path_ = "log.txt";
    std::string input_file_path_ = "in.txt";
    InputFileCache input_cache_;

    std::ofstream& get_null_stream();
    std::ofstream& get_or_open_internal(const std::string& path_str);

public:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = default;
    Logger& operator=(Logger&&) = default;

    // パス設定
    void set_log_path(std::string_view log_path);
    std::string get_log_path() const;
    void set_input_path(std::string_view input_path);
    std::string get_input_path() const;

    // ファイルキャッシュ操作
    std::ofstream& get_or_open(std::string_view path);
    void write_atomic(std::string_view path, const std::string& content);
    void flush(std::string_view path = {});
    void close_all();
    void set_silent_mode(bool silent);
    bool is_silent_mode() const;

    // 入力ファイル操作
    void ensure_input_file_exists();
    InputFileCache& get_input_cache();

    // 状態リセット（テスト用）
    void reset();

    // ログ出力（テンプレート版）
    template<typename... Args>
    void log(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        write_atomic(log_file_path_, oss.str());
    }

    template<typename... Args>
    void log_to(std::string_view filepath, Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        write_atomic(filepath, oss.str());
    }

    // 入力読み取り（テンプレート版）
    template<typename T>
    void read_input(T& value);

    template<typename T>
    bool try_read_input(T& value);

    template<typename T>
    bool read_input_timeout(T& value, std::chrono::milliseconds timeout);

    template<typename T>
    std::future<T> read_input_async() {
        return std::async(std::launch::async, [this]() {
            T value{};
            read_input(value);
            return value;
        });
    }

    template<typename T, typename Callback>
    void read_input_async(Callback callback) {
        std::thread([this, callback = std::move(callback)]() {
            T value{};
            read_input(value);
            callback(value);
        }).detach();
    }
};

// デフォルトのLoggerインスタンスを取得
Logger& get_default_logger();

// ============================================================
// 後方互換性のためのグローバル関数
// ============================================================

void init_log(std::string_view log_path);
void init_input(std::string_view input_path);
void log_flush(std::string_view filepath = {});
void log_close_all();
void log_set_silent_mode(bool silent);
bool log_is_silent_mode();
void log_reset();

// ログ出力
template<typename... Args>
void logff(Args&&... args) {
    get_default_logger().log(std::forward<Args>(args)...);
}

template<typename... Args>
void logto(std::string_view filepath, Args&&... args) {
    get_default_logger().log_to(filepath, std::forward<Args>(args)...);
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

// 入力読み取り
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
    get_default_logger().read_input_async<T>(std::move(callback));
}

template<typename T>
void loginc(T& value) {
    std::string line;
    if (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        iss >> value;
    }
}
