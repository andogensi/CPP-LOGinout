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
#include <functional>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#elif defined(__APPLE__)
#include <CoreServices/CoreServices.h>
#endif

#if __cplusplus >= 202002L
#include <format>
#define HAS_STD_FORMAT
#endif

namespace logfunc_internal {

inline auto get_file_modify_time(const std::filesystem::path& path) 
    -> std::filesystem::file_time_type {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return std::filesystem::file_time_type::min();
    }
    return ftime;
}

/**
 * @brief クロスプラットフォームのファイル監視クラス
 * 
 * OSネイティブAPIを使用してファイル変更をイベント駆動で検知します。
 * - Windows: ReadDirectoryChangesW
 * - Linux: inotify
 * - macOS: FSEvents
 * 
 * イベント駆動方式により、ポーリングと比較して以下の利点があります：
 * - CPU使用率の削減
 * - 低レイテンシでの変更検知
 * - ファイルシステムへの負荷軽減
 */
class FileWatcher {
public:
    using ChangeCallback = std::function<void()>;

    FileWatcher() = default;
    ~FileWatcher() {
        stop();
    }

    // コピー禁止
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    /**
     * @brief ファイル監視を開始
     * @param file_path 監視対象のファイルパス
     * @param callback ファイル変更時に呼ばれるコールバック
     * @return 監視開始に成功したかどうか
     */
    bool start(const std::filesystem::path& file_path, ChangeCallback callback) {
        if (running_.load()) {
            stop();
        }

        file_path_ = file_path;
        callback_ = std::move(callback);
        running_.store(true);

#ifdef _WIN32
        return start_windows();
#elif defined(__linux__)
        return start_linux();
#elif defined(__APPLE__)
        return start_macos();
#else
        // フォールバック: ポーリング方式
        return start_polling();
#endif
    }

    /**
     * @brief ファイル監視を停止
     */
    void stop() {
        if (!running_.load()) {
            return;
        }
        running_.store(false);
        
        // 条件変数で待機中のスレッドを起こす
        {
            std::lock_guard<std::mutex> lock(cv_mtx_);
            change_detected_.store(true);
        }
        cv_.notify_all();

#ifdef _WIN32
        stop_windows();
#elif defined(__linux__)
        stop_linux();
#elif defined(__APPLE__)
        stop_macos();
#endif

        if (watcher_thread_.joinable()) {
            watcher_thread_.join();
        }
    }

    /**
     * @brief ファイル変更を待機（タイムアウト付き）
     * @param timeout 最大待機時間
     * @return 変更が検知された場合true、タイムアウトの場合false
     */
    bool wait_for_change(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(cv_mtx_);
        
        if (change_detected_.load()) {
            change_detected_.store(false);
            return true;
        }
        
        bool result = cv_.wait_for(lock, timeout, [this] {
            return change_detected_.load() || !running_.load();
        });
        
        if (result && change_detected_.load()) {
            change_detected_.store(false);
            return true;
        }
        return false;
    }

    /**
     * @brief ファイル変更を無期限で待機
     */
    void wait_for_change() {
        std::unique_lock<std::mutex> lock(cv_mtx_);
        cv_.wait(lock, [this] {
            return change_detected_.load() || !running_.load();
        });
        change_detected_.store(false);
    }

    /**
     * @brief 監視が実行中かどうか
     */
    bool is_running() const {
        return running_.load();
    }

    /**
     * @brief OSネイティブAPIが利用可能かどうか
     */
    static bool has_native_support() {
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
        return true;
#else
        return false;
#endif
    }

private:
    std::filesystem::path file_path_;
    ChangeCallback callback_;
    std::atomic<bool> running_{false};
    std::atomic<bool> change_detected_{false};
    std::thread watcher_thread_;
    std::mutex cv_mtx_;
    std::condition_variable cv_;

#ifdef _WIN32
    HANDLE dir_handle_ = INVALID_HANDLE_VALUE;
    HANDLE stop_event_ = nullptr;

    bool start_windows() {
        // ファイルの親ディレクトリを監視
        auto dir_path = file_path_.parent_path();
        if (dir_path.empty()) {
            dir_path = ".";
        }

        dir_handle_ = CreateFileW(
            dir_path.wstring().c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr
        );

        if (dir_handle_ == INVALID_HANDLE_VALUE) {
            return start_polling(); // フォールバック
        }

        stop_event_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
        if (stop_event_ == nullptr) {
            CloseHandle(dir_handle_);
            dir_handle_ = INVALID_HANDLE_VALUE;
            return start_polling();
        }

        watcher_thread_ = std::thread([this] { watch_windows(); });
        return true;
    }

    void watch_windows() {
        const DWORD buffer_size = 4096;
        std::vector<BYTE> buffer(buffer_size);
        OVERLAPPED overlapped{};
        overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        auto filename = file_path_.filename().wstring();

        while (running_.load()) {
            ResetEvent(overlapped.hEvent);

            BOOL result = ReadDirectoryChangesW(
                dir_handle_,
                buffer.data(),
                buffer_size,
                FALSE, // サブディレクトリは監視しない
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                nullptr,
                &overlapped,
                nullptr
            );

            if (!result) {
                break;
            }

            HANDLE handles[] = { overlapped.hEvent, stop_event_ };
            DWORD wait_result = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

            if (wait_result == WAIT_OBJECT_0) {
                // 変更検知
                DWORD bytes_returned = 0;
                if (GetOverlappedResult(dir_handle_, &overlapped, &bytes_returned, FALSE)) {
                    auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer.data());
                    do {
                        std::wstring changed_filename(info->FileName, info->FileNameLength / sizeof(WCHAR));
                        if (changed_filename == filename) {
                            notify_change();
                            break;
                        }
                        if (info->NextEntryOffset == 0) break;
                        info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                            reinterpret_cast<BYTE*>(info) + info->NextEntryOffset
                        );
                    } while (true);
                }
            } else {
                // 停止イベントまたはエラー
                break;
            }
        }

        CloseHandle(overlapped.hEvent);
    }

    void stop_windows() {
        if (stop_event_) {
            SetEvent(stop_event_);
        }
        if (dir_handle_ != INVALID_HANDLE_VALUE) {
            CancelIo(dir_handle_);
            CloseHandle(dir_handle_);
            dir_handle_ = INVALID_HANDLE_VALUE;
        }
        if (stop_event_) {
            CloseHandle(stop_event_);
            stop_event_ = nullptr;
        }
    }
#endif

#ifdef __linux__
    int inotify_fd_ = -1;
    int watch_fd_ = -1;
    int pipe_fd_[2] = {-1, -1};

    bool start_linux() {
        inotify_fd_ = inotify_init1(IN_NONBLOCK);
        if (inotify_fd_ < 0) {
            return start_polling();
        }

        // ファイルまたは親ディレクトリを監視
        std::string watch_path;
        if (std::filesystem::exists(file_path_)) {
            watch_path = file_path_.string();
        } else {
            auto dir_path = file_path_.parent_path();
            if (dir_path.empty()) dir_path = ".";
            watch_path = dir_path.string();
        }

        watch_fd_ = inotify_add_watch(
            inotify_fd_,
            watch_path.c_str(),
            IN_MODIFY | IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO
        );

        if (watch_fd_ < 0) {
            close(inotify_fd_);
            inotify_fd_ = -1;
            return start_polling();
        }

        if (pipe(pipe_fd_) < 0) {
            inotify_rm_watch(inotify_fd_, watch_fd_);
            close(inotify_fd_);
            inotify_fd_ = -1;
            return start_polling();
        }

        watcher_thread_ = std::thread([this] { watch_linux(); });
        return true;
    }

    void watch_linux() {
        auto filename = file_path_.filename().string();
        constexpr size_t event_size = sizeof(struct inotify_event);
        constexpr size_t buffer_size = 1024 * (event_size + 16);
        char buffer[buffer_size];

        struct pollfd fds[2];
        fds[0].fd = inotify_fd_;
        fds[0].events = POLLIN;
        fds[1].fd = pipe_fd_[0];
        fds[1].events = POLLIN;

        while (running_.load()) {
            int poll_result = poll(fds, 2, -1);
            
            if (poll_result < 0) {
                break;
            }

            if (fds[1].revents & POLLIN) {
                // 停止シグナル
                break;
            }

            if (fds[0].revents & POLLIN) {
                ssize_t len = read(inotify_fd_, buffer, buffer_size);
                if (len > 0) {
                    ssize_t i = 0;
                    while (i < len) {
                        auto* event = reinterpret_cast<struct inotify_event*>(&buffer[i]);
                        if (event->len > 0) {
                            std::string event_name(event->name);
                            if (event_name == filename) {
                                notify_change();
                            }
                        } else {
                            // ファイル自体を監視している場合
                            notify_change();
                        }
                        i += event_size + event->len;
                    }
                }
            }
        }
    }

    void stop_linux() {
        if (pipe_fd_[1] >= 0) {
            char c = 'x';
            [[maybe_unused]] auto _ = write(pipe_fd_[1], &c, 1);
        }
        if (watch_fd_ >= 0 && inotify_fd_ >= 0) {
            inotify_rm_watch(inotify_fd_, watch_fd_);
            watch_fd_ = -1;
        }
        if (inotify_fd_ >= 0) {
            close(inotify_fd_);
            inotify_fd_ = -1;
        }
        if (pipe_fd_[0] >= 0) {
            close(pipe_fd_[0]);
            pipe_fd_[0] = -1;
        }
        if (pipe_fd_[1] >= 0) {
            close(pipe_fd_[1]);
            pipe_fd_[1] = -1;
        }
    }
#endif

#ifdef __APPLE__
    FSEventStreamRef stream_ = nullptr;
    dispatch_queue_t queue_ = nullptr;

    bool start_macos() {
        auto dir_path = file_path_.parent_path();
        if (dir_path.empty()) {
            dir_path = ".";
        }

        CFStringRef path_ref = CFStringCreateWithCString(
            kCFAllocatorDefault,
            dir_path.c_str(),
            kCFStringEncodingUTF8
        );

        CFArrayRef paths = CFArrayCreate(
            kCFAllocatorDefault,
            reinterpret_cast<const void**>(&path_ref),
            1,
            &kCFTypeArrayCallBacks
        );

        FSEventStreamContext context{};
        context.info = this;

        stream_ = FSEventStreamCreate(
            kCFAllocatorDefault,
            &FileWatcher::fsevents_callback,
            &context,
            paths,
            kFSEventStreamEventIdSinceNow,
            0.1, // レイテンシ: 100ms
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer
        );

        CFRelease(paths);
        CFRelease(path_ref);

        if (!stream_) {
            return start_polling();
        }

        queue_ = dispatch_queue_create("com.logfunc.filewatcher", DISPATCH_QUEUE_SERIAL);
        FSEventStreamSetDispatchQueue(stream_, queue_);
        FSEventStreamStart(stream_);

        return true;
    }

    static void fsevents_callback(
        ConstFSEventStreamRef,
        void* info,
        size_t num_events,
        void* event_paths,
        const FSEventStreamEventFlags*,
        const FSEventStreamEventId*
    ) {
        auto* watcher = static_cast<FileWatcher*>(info);
        auto** paths = static_cast<char**>(event_paths);
        auto filename = watcher->file_path_.filename().string();

        for (size_t i = 0; i < num_events; ++i) {
            std::filesystem::path event_path(paths[i]);
            if (event_path.filename().string() == filename) {
                watcher->notify_change();
                break;
            }
        }
    }

    void stop_macos() {
        if (stream_) {
            FSEventStreamStop(stream_);
            FSEventStreamInvalidate(stream_);
            FSEventStreamRelease(stream_);
            stream_ = nullptr;
        }
        if (queue_) {
            dispatch_release(queue_);
            queue_ = nullptr;
        }
    }
#endif

    // フォールバック: ポーリング方式（改善版）
    bool start_polling() {
        watcher_thread_ = std::thread([this] { watch_polling(); });
        return true;
    }

    void watch_polling() {
        auto last_modify_time = std::filesystem::file_time_type::min();
        
        // 適応的ポーリング間隔
        std::chrono::milliseconds poll_interval{50};
        constexpr std::chrono::milliseconds min_interval{10};
        constexpr std::chrono::milliseconds max_interval{500};
        int no_change_count = 0;

        while (running_.load()) {
            std::error_code ec;
            auto current_time = std::filesystem::last_write_time(file_path_, ec);
            
            if (!ec && current_time != last_modify_time) {
                last_modify_time = current_time;
                notify_change();
                poll_interval = min_interval; // 変更があったら間隔を短く
                no_change_count = 0;
            } else {
                // 変更がない場合は徐々に間隔を長く
                ++no_change_count;
                if (no_change_count > 10 && poll_interval < max_interval) {
                    poll_interval = std::min(poll_interval * 2, max_interval);
                }
            }

            std::unique_lock<std::mutex> lock(cv_mtx_);
            cv_.wait_for(lock, poll_interval, [this] {
                return !running_.load();
            });
        }
    }

    void notify_change() {
        {
            std::lock_guard<std::mutex> lock(cv_mtx_);
            change_detected_.store(true);
        }
        cv_.notify_all();
        
        if (callback_) {
            callback_();
        }
    }
};

} // namespace logfunc_internal

/**
 * @brief ログ機能を提供するLoggerクラス
 * 
 * 状態をインスタンス変数として管理し、複数のロガーインスタンスを
 * 同時に使用可能。テスト容易性と拡張性を向上させる設計。
 */
class Logger {
public:
    // InputFileCacheをpublicに移動（loginf_try等で使用）
    struct InputFileCache {
        std::filesystem::file_time_type last_check_time;
        std::filesystem::file_time_type last_modify_time;
        std::chrono::steady_clock::time_point last_access;
        bool file_exists = false;
        static constexpr std::chrono::milliseconds cache_duration{10};
    };

private:
    // ファイルキャッシュ（インスタンス変数）
    std::unordered_map<std::string, std::unique_ptr<std::ofstream>> handles_;
    mutable std::mutex mtx_;
    std::unique_ptr<std::ofstream> null_stream_;
    bool silent_mode_ = true;
    
    // パス設定（インスタンス変数）
    std::string log_file_path_ = "log.txt";
    std::string input_file_path_ = "in.txt";
    
    // 入力ファイルキャッシュ（インスタンス変数）
    InputFileCache input_cache_{
        std::filesystem::file_time_type::min(),
        std::filesystem::file_time_type::min(),
        std::chrono::steady_clock::time_point{},
        false
    };
    
    // ファイル監視機能（イベント駆動方式）
    std::unique_ptr<logfunc_internal::FileWatcher> file_watcher_;
    bool use_event_driven_ = true;  // イベント駆動方式を使用するか

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
    Logger() = default;
    
    // コピー禁止（ファイルハンドルを持つため）
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // ムーブは許可
    Logger(Logger&&) = default;
    Logger& operator=(Logger&&) = default;
    
    ~Logger() {
        close_all();
    }

    // === パス設定 ===
    void set_log_path(std::string_view log_path) {
        std::lock_guard<std::mutex> lock(mtx_);
        log_file_path_ = log_path;
    }
    
    std::string get_log_path() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return log_file_path_;
    }
    
    void set_input_path(std::string_view input_path) {
        std::lock_guard<std::mutex> lock(mtx_);
        input_file_path_ = input_path;
    }
    
    std::string get_input_path() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return input_file_path_;
    }

    // === ファイルキャッシュ操作 ===
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

    // === ログ出力 ===
    template<typename... Args>
    void log(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        write_atomic(log_file_path_, oss.str());
    }
    
#ifdef HAS_STD_FORMAT
    template<typename... Args>
    void log_formatted(std::string_view format_str, Args&&... args) {
        std::string content = std::vformat(format_str, std::make_format_args(args...));
        write_atomic(log_file_path_, content);
    }
#endif

    template<typename... Args>
    void log_to(std::string_view filepath, Args&&... args) {
        std::ostringstream oss;
        (oss << ... << std::forward<Args>(args));
        write_atomic(filepath, oss.str());
    }

    // === 入力ファイル操作 ===
    void ensure_input_file_exists() {
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
    
    InputFileCache& get_input_cache() {
        return input_cache_;
    }
    
    /**
     * @brief イベント駆動モードの有効/無効を設定
     * @param enabled true: OSネイティブAPI使用, false: ポーリング使用
     */
    void set_event_driven_mode(bool enabled) {
        std::lock_guard<std::mutex> lock(mtx_);
        use_event_driven_ = enabled;
        if (file_watcher_) {
            file_watcher_->stop();
            file_watcher_.reset();
        }
    }
    
    /**
     * @brief イベント駆動モードが有効かどうか
     */
    bool is_event_driven_mode() const {
        std::lock_guard<std::mutex> lock(mtx_);
        return use_event_driven_;
    }
    
    /**
     * @brief OSネイティブのファイル監視がサポートされているか
     */
    static bool has_native_file_watch_support() {
        return logfunc_internal::FileWatcher::has_native_support();
    }

    template<typename T>
    void read_input(T& value) {
        ensure_input_file_exists();
        
        std::string input_path;
        bool event_driven;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            input_path = input_file_path_;
            event_driven = use_event_driven_;
        }
        
        std::cout << "[Waiting for input in " << input_path << "...]\n";
        
        if (event_driven) {
            read_input_event_driven(value, input_path);
        } else {
            read_input_polling(value, input_path);
        }
        
        std::cout << "[Read value: " << value << "]\n";
    }

private:
    // イベント駆動方式による入力読み取り
    template<typename T>
    void read_input_event_driven(T& value, const std::string& input_path) {
        // ファイル監視を開始
        auto watcher = std::make_unique<logfunc_internal::FileWatcher>();
        std::atomic<bool> file_changed{true}; // 初回は読み込みを試みる
        
        watcher->start(input_path, [&file_changed] {
            file_changed.store(true);
        });
        
        bool value_read = false;
        while (!value_read) {
            if (file_changed.load()) {
                file_changed.store(false);
                
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
                    std::cout << "[File updated, reading...]\n";
                }
            }
            
            if (!value_read) {
                // イベント待機（ブロッキングだがCPU負荷なし）
                watcher->wait_for_change(std::chrono::milliseconds(1000));
            }
        }
        
        watcher->stop();
    }
    
    // ポーリング方式による入力読み取り（フォールバック）
    template<typename T>
    void read_input_polling(T& value, const std::string& input_path) {
        auto last_modify_time = logfunc_internal::get_file_modify_time(input_path);
        bool value_read = false;
        
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
    }

public:

    template<typename T>
    bool try_read_input(T& value) {
        ensure_input_file_exists();
        
        std::string input_path;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            input_path = input_file_path_;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto current_modify_time = logfunc_internal::get_file_modify_time(input_path);
        
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
    bool read_input_timeout(T& value, std::chrono::milliseconds timeout) {
        ensure_input_file_exists();
        
        std::string input_path;
        bool event_driven;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            input_path = input_file_path_;
            event_driven = use_event_driven_;
        }
        
        std::cout << "[Waiting for input in " << input_path 
                  << " (timeout: " << timeout.count() << "ms)...]\n";
        
        bool result;
        if (event_driven) {
            result = read_input_timeout_event_driven(value, input_path, timeout);
        } else {
            result = read_input_timeout_polling(value, input_path, timeout);
        }
        
        if (result) {
            std::cout << "[Read value: " << value << "]\n";
        }
        return result;
    }

private:
    // イベント駆動方式によるタイムアウト付き入力読み取り
    template<typename T>
    bool read_input_timeout_event_driven(T& value, const std::string& input_path, 
                                         std::chrono::milliseconds timeout) {
        auto watcher = std::make_unique<logfunc_internal::FileWatcher>();
        std::atomic<bool> file_changed{true};
        
        watcher->start(input_path, [&file_changed] {
            file_changed.store(true);
        });
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            );
            
            if (elapsed >= timeout) {
                std::cout << "[Timeout reached]\n";
                watcher->stop();
                return false;
            }
            
            if (file_changed.load()) {
                file_changed.store(false);
                
                if (try_read_input(value)) {
                    watcher->stop();
                    return true;
                }
                std::cout << "[File updated, reading...]\n";
            }
            
            auto remaining = timeout - elapsed;
            watcher->wait_for_change(std::min(remaining, std::chrono::milliseconds(100)));
        }
    }
    
    // ポーリング方式によるタイムアウト付き入力読み取り
    template<typename T>
    bool read_input_timeout_polling(T& value, const std::string& input_path,
                                    std::chrono::milliseconds timeout) {
        auto last_modify_time = logfunc_internal::get_file_modify_time(input_path);
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed >= timeout) {
                std::cout << "[Timeout reached]\n";
                return false;
            }
            
            if (try_read_input(value)) {
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

public:

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
    
    // === 状態リセット（テスト用） ===
    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        handles_.clear();
        log_file_path_ = "log.txt";
        input_file_path_ = "in.txt";
        silent_mode_ = true;
        use_event_driven_ = true;
        if (file_watcher_) {
            file_watcher_->stop();
            file_watcher_.reset();
        }
        input_cache_ = InputFileCache{
            std::filesystem::file_time_type::min(),
            std::filesystem::file_time_type::min(),
            std::chrono::steady_clock::time_point{},
            false
        };
    }
};

// デフォルトのLoggerインスタンスを取得（後方互換性のため）
inline Logger& get_default_logger() {
    static Logger default_logger;
    return default_logger;
}

// ============================================================
// 後方互換性のためのグローバル関数（デフォルトLoggerへのラッパー）
// ============================================================

inline void init_log(std::string_view log_path) {
    get_default_logger().set_log_path(log_path);
}

inline void init_input(std::string_view input_path) {
    get_default_logger().set_input_path(input_path);
}

template<typename... Args>
inline void logff(Args&&... args) {
    get_default_logger().log(std::forward<Args>(args)...);
}

#ifdef HAS_STD_FORMAT
template<typename... Args>
inline void logff_formatted(std::string_view format_str, Args&&... args) {
    get_default_logger().log_formatted(format_str, std::forward<Args>(args)...);
}
#endif

template<typename... Args>
inline void logto(std::string_view filepath, Args&&... args) {
    get_default_logger().log_to(filepath, std::forward<Args>(args)...);
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
    get_default_logger().read_input(value);
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
    return get_default_logger().try_read_input(value);
}

template<typename T>
inline bool loginf_timeout(T& value, std::chrono::milliseconds timeout) {
    return get_default_logger().read_input_timeout(value, timeout);
}

template<typename T>
inline std::future<T> loginf_async() {
    return get_default_logger().read_input_async<T>();
}

template<typename T, typename Callback>
inline void loginf_async(Callback callback) {
    get_default_logger().read_input_async<T>(std::move(callback));
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
    get_default_logger().flush(filepath);
}

inline void log_close_all() {
    get_default_logger().close_all();
}

inline void log_set_silent_mode(bool silent) {
    get_default_logger().set_silent_mode(silent);
}

inline bool log_is_silent_mode() {
    return get_default_logger().is_silent_mode();
}

// イベント駆動モードの設定
inline void log_set_event_driven_mode(bool enabled) {
    get_default_logger().set_event_driven_mode(enabled);
}

inline bool log_is_event_driven_mode() {
    return get_default_logger().is_event_driven_mode();
}

inline bool log_has_native_file_watch_support() {
    return Logger::has_native_file_watch_support();
}

// テスト用：デフォルトロガーの状態をリセット
inline void log_reset() {
    get_default_logger().reset();
}
