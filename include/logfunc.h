#pragma once

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
#include <future>
#include <optional>
#include <condition_variable>
#include <atomic>

#if __cplusplus >= 202002L
#include <format>
#define HAS_STD_FORMAT
#endif
namespace logfunc_internal {

class FileCache {
private:
    std::unordered_map<std::string, std::unique_ptr<std::ofstream>> handles_;
    mutable std::mutex mtx_;
    std::unique_ptr<std::ofstream> null_stream_;
    bool silent_mode_ = true; 
    
    std::ofstream& get_null_stream() {
        if (!null_stream_) {
            null_stream_ = std::make_unique<std::ofstream>();
        }
        return *null_stream_;
    }
    
    std::ofstream& get_or_open_internal(const std::string& path_str) {
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
    
public:
    std::ofstream& get_or_open(std::string_view path) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string path_str{path};
        return get_or_open_internal(path_str);
    }
    
    void write_atomic(std::string_view path, const std::string& content) {
        std::lock_guard<std::mutex> lock(mtx_);
        std::string path_str{path};
        auto& stream = get_or_open_internal(path_str);
        stream << content;
        stream.flush();
    }

    class LockedStream {
    private:
        std::ofstream& stream_;
        std::unique_lock<std::mutex> lock_;
        
    public:
        LockedStream(std::ofstream& stream, std::unique_lock<std::mutex> lock)
            : stream_(stream), lock_(std::move(lock)) {}
        
        template<typename T>
        LockedStream& operator<<(T&& value) {
            stream_ << std::forward<T>(value);
            return *this;
        }
        
        void flush() { stream_.flush(); }

        LockedStream(LockedStream&&) = default;
        LockedStream& operator=(LockedStream&&) = default;
        LockedStream(const LockedStream&) = delete;
        LockedStream& operator=(const LockedStream&) = delete;
    };

    LockedStream get_locked_stream(std::string_view path) {
        std::unique_lock<std::mutex> lock(mtx_);
        std::string path_str{path};
        auto& stream = get_or_open_internal(path_str);
        return LockedStream(stream, std::move(lock));
    }
    
    void flush(std::string_view path = {}) {
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
    
    void close_all() {
        std::lock_guard<std::mutex> lock(mtx_);
        handles_.clear();
    }

    void set_silent_mode(bool silent) {
        std::lock_guard<std::mutex> lock(mtx_);
        silent_mode_ = silent;
    }
    
    bool is_silent_mode() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return silent_mode_;
    }
    
    ~FileCache() {
        close_all();
    }
};

inline FileCache& get_file_cache() {
    static FileCache cache;
    return cache;
}

inline std::string& get_log_file_path() {
    static std::string log_path = "log.txt";
    return log_path;
}

inline std::string& get_input_file_path() {
    static std::string input_path = "in.txt";
    return input_path;
}

inline auto get_file_modify_time(const std::filesystem::path& path) 
    -> std::filesystem::file_time_type {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return std::filesystem::file_time_type::min();
    }
    return ftime;
}

// loginf_try用
struct InputFileCache {
    std::filesystem::file_time_type last_check_time;
    std::filesystem::file_time_type last_modify_time;
    std::chrono::steady_clock::time_point last_access;
    bool file_exists = false;
//これ10ms
    static constexpr std::chrono::milliseconds cache_duration{10};
};

inline InputFileCache& get_input_file_cache() {
    static InputFileCache cache{
        std::filesystem::file_time_type::min(),
        std::filesystem::file_time_type::min(),
        std::chrono::steady_clock::time_point{},
        false
    };
    return cache;
}

inline void ensure_input_file_exists() {
    namespace fs = std::filesystem;
    const auto& input_path = get_input_file_path();
    
    if (!fs::exists(input_path)) {
        std::ofstream file(input_path);
        if (file) {
            file << "# Enter input values here (one per line)\n";
        }
    }
}

} 
inline void init_log(std::string_view log_path) {
    logfunc_internal::get_log_file_path() = log_path;
}

inline void init_input(std::string_view input_path) {
    logfunc_internal::get_input_file_path() = input_path;
}


template<typename... Args>
inline void logff(Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    // アトミックに出力
    logfunc_internal::get_file_cache().write_atomic(
        logfunc_internal::get_log_file_path(), oss.str());
}

#ifdef HAS_STD_FORMAT
template<typename... Args>
inline void logff_formatted(std::string_view format_str, Args&&... args) {
    std::string content = std::vformat(format_str, std::make_format_args(args...));
    logfunc_internal::get_file_cache().write_atomic(
        logfunc_internal::get_log_file_path(), content);
}
#endif


template<typename... Args>
inline void logto(std::string_view filepath, Args&&... args) {
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    // アトミックに出力
    logfunc_internal::get_file_cache().write_atomic(filepath, oss.str());
}

template<typename... Args>
inline void logc(Args&&... args) {
    (std::cout << ... << std::forward<Args>(args));
}
template<typename... Args>
inline void logc_safe(Args&&... args) {
    static std::mutex cout_mtx;
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    std::lock_guard<std::mutex> lock(cout_mtx);
    std::cout << oss.str();
}


template<typename T>
inline void loginf_impl(T& value) {
    logfunc_internal::ensure_input_file_exists();
    
    const auto& input_path = logfunc_internal::get_input_file_path();
    auto last_modify_time = logfunc_internal::get_file_modify_time(input_path);
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
            
            auto new_modify_time = logfunc_internal::get_file_modify_time(input_path);
            if (new_modify_time != last_modify_time) {
                last_modify_time = new_modify_time;
                std::cout << "[File updated, reading...]\n";
            }
        }
    }
    
    std::cout << "[Read value: " << value << "]\n";
}

inline void loginf(int& value) {
    loginf_impl(value);
}

inline void loginf(float& value) {
    loginf_impl(value);
}

inline void loginf(double& value) {
    loginf_impl(value);
}

template<typename T>
inline bool loginf_try(T& value) {
    logfunc_internal::ensure_input_file_exists();
    
    const auto& input_path = logfunc_internal::get_input_file_path();
    auto& cache = logfunc_internal::get_input_file_cache();
    auto now = std::chrono::steady_clock::now();
    auto current_modify_time = logfunc_internal::get_file_modify_time(input_path);
    

    if (cache.file_exists && 
        (now - cache.last_access) < logfunc_internal::InputFileCache::cache_duration &&
        current_modify_time == cache.last_modify_time) {
        return false;  // ファイルを開かずに高速リターン！
    }
 
    std::ifstream file(input_path);
    
    if (!file) {
        cache.file_exists = false;
        cache.last_access = now;
        return false;
    }
    

    std::string line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        if (!line.empty() && line[0] != '#') {
            std::istringstream iss(line);
            if (iss >> value) {
                cache.last_modify_time = current_modify_time;
                cache.last_access = now;
                cache.file_exists = true;
                return true;
            }
        }
    }

    cache.last_modify_time = current_modify_time;
    cache.last_access = now;
    cache.file_exists = true;
    return false;
}
template<typename T>
inline bool loginf_timeout(T& value, std::chrono::milliseconds timeout) {
    logfunc_internal::ensure_input_file_exists();
    
    const auto& input_path = logfunc_internal::get_input_file_path();
    auto last_modify_time = logfunc_internal::get_file_modify_time(input_path);
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "[Waiting for input in " << input_path 
              << " (timeout: " << timeout.count() << "ms)...]\n";
    
    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed >= timeout) {
            std::cout << "[Timeout reached]\n";
            return false;
        }
        
        if (loginf_try(value)) {
            std::cout << "[Read value: " << value << "]\n";
            return true;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        auto new_modify_time = logfunc_internal::get_file_modify_time(input_path);
        if (new_modify_time != last_modify_time) {
            last_modify_time = new_modify_time;
            std::cout << "[File updated, reading...]\n";
        }
    }
}

template<typename T>
inline std::future<T> loginf_async() {
    return std::async(std::launch::async, []() {
        T value{};
        loginf_impl(value);
        return value;
    });
}

template<typename T, typename Callback>
inline void loginf_async(Callback callback) {
    std::thread([callback = std::move(callback)]() {
        T value{};
        loginf_impl(value);
        callback(value);
    }).detach();
}

template<typename T>
inline void loginc(T& value) {
    std::string line;
    if (std::getline(std::cin, line)) {
        std::istringstream iss(line);
        iss >> value;
    }
}
inline void log_flush(std::string_view filepath = {}) {
    logfunc_internal::get_file_cache().flush(filepath);
}


inline void log_close_all() {
    logfunc_internal::get_file_cache().close_all();
}

// サイレントモードの設定
inline void log_set_silent_mode(bool silent) {
    logfunc_internal::get_file_cache().set_silent_mode(silent);
}

inline bool log_is_silent_mode() {
    return logfunc_internal::get_file_cache().is_silent_mode();
}
