# logfunc ライブラリ

*[English Version](README.md)*

C++向けのシンプルな入出力ライブラリです。
ファイルとコンソール間でのデータ入出力を簡単に切り替えられます。

## 特徴

- **Modern C++** - C++17以降に対応（`std::filesystem`, `std::string_view`等）
- **ヘッダーオンリー** - `logfunc.h`をインクルードするだけで使用可能
- **シンプルなAPI** - 直感的な関数名（`logff`, `logc`, `loginf`, `loginc`）
- **型安全** - テンプレートベースの可変長引数で任意の型をサポート
- **ストリームスタイル** - `std::ofstream`/`std::ifstream`によるRAII準拠
- **カスタマイズ可能** - `std::string`ベースのパス管理
- **自動ファイル監視** - OSネイティブAPI（イベント駆動）と`std::filesystem`によるフォールバック
- **高パフォーマンス** - `std::unique_ptr`によるスマートなリソース管理
- **スレッドセーフ** - `std::mutex`による排他制御
- **クロスプラットフォーム** - Windows/Linux対応

---

## 使い方1: ヘッダーオンリー版（推奨）

`include/logfunc.h`をインクルードするだけで使えます。

```cpp
#include "include/logfunc.h"

int main() {
    // Modern C++: ストリームスタイルで簡単に出力
    int x = 42;
    float pi = 3.14f;
    logff("x: ", x, ", pi: ", pi, "\n");  // log.txtに出力
    
    logc("World\n");   // コンソールに出力
    
    int input;
    loginf(input);  // in.txtから入力（std::filesystemで更新を自動検知）
    logff("input value: ", input, "\n");
    
    return 0;
}
```

**ビルド方法:**
```powershell
# MinGW/GCC (C++17以降が必要)
g++ -std=c++17 examples/example.cpp -I include -o example.exe

# MSVC (C++17以降が必要)
cl /std:c++17 examples/example.cpp /I include

# 実行
.\example.exe
```

---

## 使い方2: CMakeを使ったビルド（推奨）

CMakeを使用すると、他のプロジェクトへの組み込みが簡単になります。

### 方法A: add_subdirectory で組み込む

```cmake
# あなたのプロジェクトの CMakeLists.txt
cmake_minimum_required(VERSION 3.14)
project(MyProject)

# logfuncをサブディレクトリとして追加
add_subdirectory(path/to/logfunc)

# あなたの実行ファイル
add_executable(my_app main.cpp)

# ヘッダーオンリー版を使用（推奨）
target_link_libraries(my_app PRIVATE logfunc::header_only)

# または、ライブラリ版を使用
# target_link_libraries(my_app PRIVATE logfunc::lib)
```

### 方法B: FetchContent で直接取得

```cmake
include(FetchContent)

FetchContent_Declare(
    logfunc
    GIT_REPOSITORY https://github.com/andogensi/CPP-LOGinout.git
    GIT_TAG main
)
FetchContent_MakeAvailable(logfunc)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE logfunc::header_only)
```

### 方法C: find_package で使用（インストール後）

```cmake
find_package(logfunc REQUIRED)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE logfunc::header_only)
```

### CMakeビルド手順

```powershell
# ビルドディレクトリを作成
mkdir build
cd build

# CMakeの設定
cmake .. -G "MinGW Makefiles"  # MinGWの場合
# cmake .. -G "Visual Studio 17 2022"  # Visual Studioの場合

# サンプルも一緒にビルドする場合
cmake .. -DLOGFUNC_BUILD_EXAMPLES=ON

# ビルド
cmake --build .

# インストール（オプション）
cmake --install . --prefix C:/local/logfunc
```

### CMakeオプション

| オプション | デフォルト | 説明 |
|-----------|-----------|------|
| `LOGFUNC_BUILD_EXAMPLES` | OFF | サンプルプログラムをビルド |
| `LOGFUNC_HEADER_ONLY` | ON | デフォルトでヘッダーオンリー版を使用 |

---

## 使い方3: ライブラリ版（バイナリ配布向け）

静的ライブラリとして使用する場合。

### ライブラリのビルド

```powershell
# MinGW/GCC
g++ -std=c++17 -c src/logfunc_lib.cpp -I include -o logfunc_lib.o
ar rcs liblogfunc.a logfunc_lib.o

# MSVC
cl /std:c++17 /c src/logfunc_lib.cpp /I include /EHsc
lib logfunc_lib.obj /OUT:logfunc.lib
```

**注意:** C++17以降が必要です（`std::filesystem`, `std::string_view`を使用）

### ライブラリの使用

```cpp
#include "include/logfunc_lib.h"

int main() {
    int x = 42;
    logff("Value: ", x, "\n");  // Modern C++: 可変長テンプレート
    logc("World\n");
    return 0;
}
```

**ビルド方法:**
```powershell
# MinGW/GCC (C++17以降が必要)
g++ -std=c++17 examples/example_lib.cpp -I include -L. -llogfunc -o example_lib.exe

# MSVC (C++17以降が必要)
cl /std:c++17 examples/example_lib.cpp /I include logfunc.lib /EHsc
```

---

## Modern C++の特徴

このライブラリは**Modern C++ (C++17以降)**の機能を活用しています:

### 主要な改善点

1. **`FILE*` → `std::ofstream`/`std::ifstream`**
   - RAII原則に従った自動リソース管理
   - 例外安全性の向上
   - スマートポインタ(`std::unique_ptr`)による所有権管理

2. **`char[]` → `std::string`**
   - 動的メモリ管理が不要
   - バッファオーバーフローの心配なし
   - `std::string_view`による効率的な文字列参照

3. **`std::filesystem`の活用**
   - プラットフォーム非依存のファイル操作
   - `last_write_time()`による正確な更新検知
   - パス操作の簡潔化

4. **可変長テンプレート引数**
   - `va_list`を使わないタイプセーフな実装
   - コンパイル時の型チェック
   - 任意の型を自然に出力可能

### コード例の比較

**従来のCスタイル:**
```cpp
// FILE*を使った実装
FILE* fp = fopen("log.txt", "a");
fprintf(fp, "Value: %d\n", 42);
fclose(fp);  // 手動でクローズが必要
```

**Modern C++スタイル:**
```cpp
// std::ofstreamを使った実装
{
    std::ofstream file("log.txt", std::ios::app);
    file << "Value: " << 42 << "\n";
}  // スコープを抜けると自動的にクローズ (RAII)
```

**このライブラリの使用:**
```cpp
// さらにシンプル
logff("Value: ", 42, "\n");
```

---

## API リファレンス

### 初期化関数

| 関数 | 説明 |
|------|------|
| `init_log(std::string_view path)` | ログファイルのパスを設定（デフォルト: "log.txt"） |
| `init_input(std::string_view path)` | 入力ファイルのパスを設定（デフォルト: "in.txt"） |
| `log_set_event_driven_mode(bool)` | ファイル監視モードを設定（true: OSネイティブAPI, false: ポーリング） |
| `log_is_event_driven_mode()` | 現在のファイル監視モードを取得 |
| `log_has_native_file_watch_support()` | OSネイティブファイル監視がサポートされているか |
| `log_reset()` | デフォルトロガーの状態をリセット（テスト用） |

### 出力関数

| 関数 | 説明 | 出力先 |
|------|------|--------|
| `logff(args...)` | 可変長引数をストリームスタイルで出力 | デフォルトまたは設定されたログファイル |
| `logto(filepath, args...)` | 指定ファイルに可変長引数を出力 | 指定されたファイル |
| `logc(args...)` | 可変長引数をコンソールに出力 | コンソール |
| `logc_safe(args...)` | スレッドセーフにコンソールに出力 | コンソール |
| `log_flush(filepath)` | 指定ファイルのバッファを強制フラッシュ（省略時は全ファイル） | - |
| `log_close_all()` | 全てのファイルハンドルをクローズ | - |

**Modern C++の可変長テンプレート:**

このライブラリは`template<typename... Args>`を使用し、任意の数・任意の型の引数を受け取れます。

**使用例:**
```cpp
// ストリームスタイルで自然に出力
int x = 42;
float y = 3.14f;
logff("x: ", x, ", y: ", y, "\n");  // log.txtに "x: 42, y: 3.14"

// 文字列リテラルも自然に扱える
logff("Status: ", "OK", "\n");  // log.txtに "Status: OK"

// ファイルパスを変更
init_log("custom.txt");
logff("Result: ", 100, "\n");  // custom.txtに出力

// 特定のファイルに出力（logto関数）
logto("debug.txt", "Debug: value = ", 42, "\n");
logto("result.txt", 3.14159, "\n");
logto("data.txt", 42, "\n");

// 単純な値の出力
logff(42, "\n");  // log.txtに "42"
logff(3.14f, "\n");  // log.txtに "3.14"

// 複数の値を一度に
logff("a=", 1, " b=", 2, " c=", 3, "\n");

// 標準ライブラリの型もサポート
std::string name = "Alice";
logff("Name: ", name, "\n");
```

**型安全性:**
- コンパイル時に型チェックが行われます
- `<<`演算子が定義されている任意の型を出力可能
- printf形式の`%d`と`%s`の不一致エラーが発生しません

### 入力関数

#### ブロッキング版（従来版）

| 関数 | 説明 | 入力元 |
|------|------|--------|
| `loginf(variable)` | 値を1行読み込み（**メインスレッドをブロック**） | `in.txt` |
| `loginc(variable)` | 値を1行読み込み | コンソール |

**対応型:** `int`, `float`, `double`

**注意:**
- `loginf()`は`in.txt`の更新を自動的に監視します
- ファイルが存在しない場合は自動作成されます
- `#`で始まる行はコメントとして扱われます
- ⚠️ **ファイルが更新されるまでメインスレッドがブロックされます**（デバッグ/実験用）

#### 非ブロッキング版（推奨）

メインスレッド（GUI/ゲームループ）をブロックしないバージョンです。

| 関数 | 説明 | 用途 |
|------|------|------|
| `loginf_try(variable)` | 即座にチェック、読めたら`true`を返す | ゲームループ、リアルタイム処理 |
| `loginf_timeout(variable, timeout)` | タイムアウト付き、時間内に読めなければ`false` | 長時間フリーズ防止 |
| `loginf_async<T>()` | 非同期で待機、`std::future<T>`を返す | バックグラウンド処理 |
| `loginf_async<T>(callback)` | 非同期で待機、コールバックを呼び出す | イベント駆動 |

**使用例:**

```cpp
// ノンブロッキング版: ゲームループで使用
int value = 0;
while (game_running) {
    if (loginf_try(value)) {
        std::cout << "Player input: " << value << "\n";
        break;
    }
    update_game();  // ブロックされない！
    render_frame();
}

// タイムアウト版: 5秒待ってタイムアウト
if (loginf_timeout(value, std::chrono::seconds(5))) {
    std::cout << "Got: " << value << "\n";
} else {
    std::cout << "Timeout!\n";
    value = 100; // デフォルト値
}

// 非同期版: バックグラウンドで待機
auto future = loginf_async<int>();
// メインスレッドで他の処理...
int value = future.get(); // 結果を取得

// 非同期版（コールバック）: イベント駆動
loginf_async<int>([](int value) {
    std::cout << "Received: " << value << "\n";
});
// メインスレッドは即座に続行...
```

詳細は [ASYNC_USAGE.md](ASYNC_USAGE.md) を参照してください。

---

## サンプルコード

### examples/example.cpp - 基本的な使用例

```cpp
#include "include/logfunc.h"

int main() {
    // 例1: ファイル出力とファイル入力（Modern C++版）
    logff("input number:\n");
    int num;
    loginf(num);
    logff("you input number is: ", num, "\n");
    logc("Console output: ", num, "\n");
    
    // 例2: 複数の値を一度に出力
    float f;
    logff("input float number:\n");
    loginc(f);
    logff("you input float number is: ", f, "\n");
    
    // 例3: ストリームスタイルでの出力
    int x = 10, y = 20;
    logff("x=", x, ", y=", y, ", sum=", x + y, "\n");
    
    return 0;
}
```

### examples/benchmark.cpp - パフォーマンステスト

```cpp
#include <iostream>
#include <chrono>
#include "../include/logfunc.h"

int main() {
    const int ITERATIONS = 10000;
    
    std::cout << "=== Performance Benchmark ===\n";
    std::cout << "Testing " << ITERATIONS << " log operations...\n\n";
    
    // 高速ログ出力テスト（Modern C++ 可変長テンプレート）
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++) {
        logff("Frame: ", i, ", X: ", i * 10, ", Y: ", i * 20, "\n");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "✓ Completed in " << duration.count() << " ms\n";
    std::cout << "✓ Throughput: " << (ITERATIONS / (duration.count() / 1000.0)) << " ops/sec\n";
    
    return 0;
}
```

**実行方法:**
```powershell
g++ -std=c++17 examples/benchmark.cpp -I include -o benchmark.exe
.\benchmark.exe
```

### in.txt の例

```
# このファイルに入力値を記述してください（1行に1つ）
42
3.14
```

### 実行結果

**log.txt:**
```
input number:
you input number is: 42
input float number:
you input float number is: 3.14
x=10, y=20, sum=30
```

**コンソール:**
```
[Waiting for input in in.txt...]
[Read value: 42]
Console output: 42
input float number:
3.14
```

---

## ファイル構成

```
.
├── CMakeLists.txt            # CMakeビルド設定
├── cmake/                    # CMake設定ファイル
│   ├── logfuncConfig.cmake.in
│   └── logfunc.pc.in
├── include/                  # ヘッダーファイル
│   ├── logfunc.h             # ヘッダーオンリー版（推奨）
│   └── logfunc_lib.h         # ライブラリ版ヘッダー
├── src/                      # ソースファイル
│   └── logfunc_lib.cpp       # ライブラリ版実装
├── examples/                 # サンプルプログラム
│   ├── example.cpp           # ヘッダーオンリー版サンプル
│   └── example_lib.cpp       # ライブラリ版サンプル
├── Test/                     # テストプログラム
│   └── test_async.cpp        # 非同期APIテスト
├── ASYNC_USAGE.md            # 非同期APIの詳細ドキュメント
├── LICENSE                   # ライセンスファイル
├── README.md                 # 英語版ドキュメント
└── README_JA.md              # 日本語版ドキュメント（このファイル）
```

---

## 重要な注意事項

### 関数名の使い分け

- **`logff()`**: デフォルトのログファイル（`log.txt`）に出力
- **`logto()`**: 特定のファイルに出力（Log To File）

この区別により、以下のような曖昧性が解決されています:

```cpp
// ✅ 正しく動作: logffで可変長引数を出力
logff("Status: ", "OK", "\n");  // log.txtに "Status: OK"

// ✅ 正しく動作: logtoで特定ファイルに出力
logto("custom.txt", "Status: ", "OK", "\n");  // custom.txtに "Status: OK"
```

以前のバージョンでは `logff(filepath, value)` という関数があり、
`logff("Status: %s", "OK")` が意図せず「Status: %s」という名前のファイルを作ろうとする
バグがありましたが、`logto`関数の導入により解決されています。

---

## パフォーマンス最適化

### イベント駆動型ファイル監視

v3.0以降、ファイル監視にOSネイティブAPIを使用するイベント駆動方式を採用しています。

**対応プラットフォーム:**
| OS | API | 特徴 |
|------|------|------|
| Windows | `ReadDirectoryChangesW` | 非同期I/O、低レイテンシ |
| Linux | `inotify` | カーネルレベルの監視、高効率 |
| macOS | `FSEvents` | ディスパッチキュー対応 |

**従来のポーリング方式の問題点:**
```cpp
// ❌ 従来: 100msごとにファイルシステムにアクセス
while (!value_read) {
    auto new_time = std::filesystem::last_write_time(path);  // 重い
    std::this_thread::sleep_for(100ms);  // レイテンシ
}
```

**イベント駆動方式のメリット:**
```cpp
// ✅ 新方式: OSからの通知を待機（CPU負荷ゼロ）
watcher->wait_for_change();  // ブロッキングだがCPU使用率0%
```

**比較:**
| 項目 | ポーリング方式 | イベント駆動方式 |
|------|---------------|-----------------|
| CPU使用率 | 高い（定期的なアクセス） | ほぼゼロ |
| レイテンシ | 最大100ms | ほぼリアルタイム |
| ファイルシステム負荷 | 高い | 低い |
| SSD寿命への影響 | あり | なし |

**モード切り替え:**
```cpp
// イベント駆動モードを無効化（ポーリングに切り替え）
log_set_event_driven_mode(false);

// 現在のモードを確認
if (log_is_event_driven_mode()) {
    std::cout << "Using event-driven file watching\n";
}

// OSネイティブAPIのサポート確認
if (log_has_native_file_watch_support()) {
    std::cout << "Native file watching is supported\n";
}
```

### ファイルハンドルキャッシュ

v2.0以降、ファイルハンドルをキャッシュすることで、毎回のopen/closeによるオーバーヘッドを削減しています。

**従来版（遅い）:**
```cpp
for (int i = 0; i < 10000; i++) {
    logff("frame: ", i, "\n");  // 毎回 fopen/fclose → 非常に重い
}
```

**最適化版（高速）:**
```cpp
for (int i = 0; i < 10000; i++) {
    logff("frame: ", i, "\n");  // std::ofstreamを再利用 → 高速
}
// プログラム終了時に自動的にクローズされる（RAII）
```

**特徴:**
- `std::ofstream`は最初のアクセス時に開かれ、`std::unique_ptr`で管理されます
- バッファリングを無効化（`pubsetbuf`）し、リアルタイム性を確保
- プログラム終了時にRAII原則により自動クローズ
- マルチスレッド環境でも安全（`std::mutex`でロック）

**手動制御が必要な場合:**
```cpp
// 明示的にフラッシュ（確実にディスクに書き込む）
log_flush();  // 全ファイル
log_flush("custom.txt");  // 特定ファイル

// 全ファイルを手動でクローズ（通常は不要）
log_close_all();
```

**実測パフォーマンス:**
```
10,000回のログ出力: 約45ms (222,222 ops/sec)
平均: 0.0045ms/操作
```
従来の毎回open/close方式と比較して、**約10〜50倍の高速化**を実現しています。

**Modern C++のメリット:**
- `std::unique_ptr`によるメモリリーク防止
- RAII原則による確実なリソース解放
- 例外安全性の向上

---

## トラブルシューティング

### ファイルが見つからない

ヘッダーファイルのパスを確認してください:
```cpp
#include "include/logfunc.h"  // プロジェクトルートから
```

### ファイル入力が動作しない

- `in.txt`が存在するか確認してください（自動作成されます）
- ファイルの読み取り権限を確認してください
- コメント行（`#`で始まる行）と空行は自動的にスキップされます

### ビルドエラー

MinGW/GCCの場合:
```powershell
g++ -std=c++17 your_program.cpp -I include -o program.exe
```

**C++17の機能要件:**
- `std::filesystem`: ファイルシステム操作
- `std::string_view`: 効率的な文字列参照
- 構造化束縛（Structured bindings）: `auto& [key, value]`
- テンプレートの畳み込み式: `(stream << ... << args)`

---

## リポジトリ

GitHub: [andogensi/CPP-LOGinout](https://github.com/andogensi/CPP-LOGinout)

---

## ライセンス

MIT License - 自由に使用・改変・配布できます
