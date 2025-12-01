#include "logfunc_lib.h"
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>
#include <memory>

namespace {
    auto get_file_modify_time(const std::filesystem::path& path) 
        -> std::filesystem::file_time_type {
        std::error_code ec;
        auto ftime = std::filesystem::last_write_time(path, ec);
        if (ec) {
            return std::filesystem::file_time_type::min();
        }
        return ftime;
    }
}

// ============================================================
// Logger クラス実装
// ============================================================

Logger::Logger() 
    : silent_mode_(true)
    , log_file_path_("log.txt")
    , input_file_path_("in.txt")
    , input_cache_{
        std::filesystem::file_time_type::min(),
        std::filesystem::file_time_type::min(),
        std::chrono::steady_clock::time_point{},
        false
    }
{}

Logger::~Logger() {
    close_all();
}

std::ofstream& Logger::get_null_stream() {
    if (!null_stream_) {
        null_stream_ = std::make_unique<std::ofstream>();
    }
    return *null_stream_;
}

std::ofstream& Logger::get_or_open_internal(const std::string& path_str) {
    auto it = handles_.find(path_str);
    if (it != handles_.end() && it->second && it->second->is_open()) {
        return *it->second;
    }
    
    auto stream = std::make_unique<std::ofstream>(
        path_str, std::ios::app
    );
    
    if (stream && stream->is_open()) {
        stream->rdbuf()->pubsetbuf(nullptr, 0);
        auto& ref = *stream;
        handles_[path_str] = std::move(stream);
        return ref;
    }
    if (silent_mode_) {
        std::cerr << "[logfunc] Warning: Failed to open file: " << path_str << std::endl;
        return get_null_stream(); 
    } else {
        throw std::runtime_error("Failed to open file: " + path_str);
    }
}

void Logger::set_log_path(std::string_view log_path) {
    std::lock_guard<std::mutex> lock(mtx_);
    log_file_path_ = log_path;
}

std::string Logger::get_log_path() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return log_file_path_;
}

void Logger::set_input_path(std::string_view input_path) {
    std::lock_guard<std::mutex> lock(mtx_);
    input_file_path_ = input_path;
}

std::string Logger::get_input_path() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return input_file_path_;
}

std::ofstream& Logger::get_or_open(std::string_view path) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::string path_str{path};
    return get_or_open_internal(path_str);
}

void Logger::write_atomic(std::string_view path, const std::string& content) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::string path_str{path};
    auto& stream = get_or_open_internal(path_str);
    stream << content;
    stream.flush();
}

void Logger::flush(std::string_view path) {
    std::lock_guard<std::mutex> lock(mtx_);
    
    if (!path.empty()) {
        std::string path_str{path};
        auto it = handles_.find(path_str);
        if (it != handles_.end() && it->second) {
            it->second->flush();
        }
    } else {
        for (auto& [_, stream] : handles_) {
            if (stream) {
                stream->flush();
            }
        }
    }
}

void Logger::close_all() {
    std::lock_guard<std::mutex> lock(mtx_);
    handles_.clear();
}

void Logger::set_silent_mode(bool silent) {
    std::lock_guard<std::mutex> lock(mtx_);
    silent_mode_ = silent;
}

bool Logger::is_silent_mode() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return silent_mode_;
}

void Logger::ensure_input_file_exists() {
    namespace fs = std::filesystem;
    std::string input_path;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        input_path = input_file_path_;
    }
    
    if (!fs::exists(input_path)) {
        std::ofstream file(input_path);
        if (file) {
            file << "# Enter input values here (one per line)\n";
        }
    }
}

Logger::InputFileCache& Logger::get_input_cache() {
    return input_cache_;
}

void Logger::reset() {
    std::lock_guard<std::mutex> lock(mtx_);
    handles_.clear();
    log_file_path_ = "log.txt";
    input_file_path_ = "in.txt";
    silent_mode_ = true;
    input_cache_ = InputFileCache{
        std::filesystem::file_time_type::min(),
        std::filesystem::file_time_type::min(),
        std::chrono::steady_clock::time_point{},
        false
    };
}

// read_input テンプレートの明示的インスタンス化
template<typename T>
void Logger::read_input(T& value) {
    ensure_input_file_exists();
    
    std::string input_path;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        input_path = input_file_path_;
    }
    
    auto last_modify_time = get_file_modify_time(input_path);
    bool value_read = false;
    
    std::cout << "[Waiting for input in " << input_path << "...]\n";
    
    while (!value_read) {
        std::ifstream file(input_path);
        if (file) {
            std::string line;
            while (std::getline(file, line)) {
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                
                if (!line.empty() && line[0] != '#') {
                    std::istringstream iss(line);
                    if (iss >> value) {
                        value_read = true;
                        break;
                    }
                }
            }
        }
        
        if (!value_read) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto new_modify_time = get_file_modify_time(input_path);
            if (new_modify_time != last_modify_time) {
                last_modify_time = new_modify_time;
                std::cout << "[File updated, reading...]\n";
            }
        }
    }
    
    std::cout << "[Read value: " << value << "]\n";
}

template<typename T>
bool Logger::try_read_input(T& value) {
    ensure_input_file_exists();
    
    std::string input_path;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        input_path = input_file_path_;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto current_modify_time = get_file_modify_time(input_path);
    
    if (input_cache_.file_exists && 
        (now - input_cache_.last_access) < InputFileCache::cache_duration &&
        current_modify_time == input_cache_.last_modify_time) {
        return false;
    }
    
    std::ifstream file(input_path);
    
    if (!file) {
        input_cache_.file_exists = false;
        input_cache_.last_access = now;
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (!line.empty() && line[0] != '#') {
            std::istringstream iss(line);
            if (iss >> value) {
                input_cache_.last_modify_time = current_modify_time;
                input_cache_.last_access = now;
                input_cache_.file_exists = true;
                return true;
            }
        }
    }
    
    input_cache_.last_modify_time = current_modify_time;
    input_cache_.last_access = now;
    input_cache_.file_exists = true;
    return false;
}

template<typename T>
bool Logger::read_input_timeout(T& value, std::chrono::milliseconds timeout) {
    ensure_input_file_exists();
    
    std::string input_path;
    {
        std::lock_guard<std::mutex> lock(mtx_);
        input_path = input_file_path_;
    }
    
    auto last_modify_time = get_file_modify_time(input_path);
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "[Waiting for input in " << input_path 
              << " (timeout: " << timeout.count() << "ms)...]\n";
    
    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            std::cout << "[Timeout reached]\n";
            return false;
        }
        
        if (try_read_input(value)) {
            std::cout << "[Read value: " << value << "]\n";
            return true;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto new_modify_time = get_file_modify_time(input_path);
        if (new_modify_time != last_modify_time) {
            last_modify_time = new_modify_time;
            std::cout << "[File updated, reading...]\n";
        }
    }
}

// 明示的インスタンス化
template void Logger::read_input<int>(int&);
template void Logger::read_input<float>(float&);
template void Logger::read_input<double>(double&);
template bool Logger::try_read_input<int>(int&);
template bool Logger::try_read_input<float>(float&);
template bool Logger::try_read_input<double>(double&);
template bool Logger::read_input_timeout<int>(int&, std::chrono::milliseconds);
template bool Logger::read_input_timeout<float>(float&, std::chrono::milliseconds);
template bool Logger::read_input_timeout<double>(double&, std::chrono::milliseconds);

// ============================================================
// デフォルトロガー
// ============================================================

Logger& get_default_logger() {
    static Logger default_logger;
    return default_logger;
}

// ============================================================
// 後方互換性のためのグローバル関数実装
// ============================================================

void init_log(std::string_view log_path) {
    get_default_logger().set_log_path(log_path);
}

void init_input(std::string_view input_path) {
    get_default_logger().set_input_path(input_path);
}

void loginf(int& value) {
    get_default_logger().read_input(value);
}

void loginf(float& value) {
    get_default_logger().read_input(value);
}

void loginf(double& value) {
    get_default_logger().read_input(value);
}

bool loginf_try(int& value) {
    return get_default_logger().try_read_input(value);
}

bool loginf_try(float& value) {
    return get_default_logger().try_read_input(value);
}

bool loginf_try(double& value) {
    return get_default_logger().try_read_input(value);
}

bool loginf_timeout(int& value, std::chrono::milliseconds timeout) {
    return get_default_logger().read_input_timeout(value, timeout);
}

bool loginf_timeout(float& value, std::chrono::milliseconds timeout) {
    return get_default_logger().read_input_timeout(value, timeout);
}

bool loginf_timeout(double& value, std::chrono::milliseconds timeout) {
    return get_default_logger().read_input_timeout(value, timeout);
}

std::future<int> loginf_async_int() {
    return get_default_logger().read_input_async<int>();
}

std::future<float> loginf_async_float() {
    return get_default_logger().read_input_async<float>();
}

std::future<double> loginf_async_double() {
    return get_default_logger().read_input_async<double>();
}

void log_flush(std::string_view filepath) {
    get_default_logger().flush(filepath);
}

void log_close_all() {
    get_default_logger().close_all();
}

void log_set_silent_mode(bool silent) {
    get_default_logger().set_silent_mode(silent);
}

bool log_is_silent_mode() {
    return get_default_logger().is_silent_mode();
}

void log_reset() {
    get_default_logger().reset();
}
