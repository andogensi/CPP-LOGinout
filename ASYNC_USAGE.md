# 非同期入力 API ドキュメント

`logfunc`ライブラリの非同期入力機能の詳細ドキュメントです。

## 概要

従来の `loginf()` 関数はファイルが更新されるまでメインスレッドをブロックしてしまいます。
これはゲームループやGUIアプリケーションでは問題となることがあります。

非同期入力APIは、この問題を解決するために以下のバリエーションを提供します：

| 関数 | 特徴 | 用途 |
|------|------|------|
| `loginf_try()` | 即座にチェック、ノンブロッキング | ゲームループ、リアルタイム処理 |
| `loginf_timeout()` | タイムアウト付きの待機 | 長時間フリーズの防止 |
| `loginf_async<T>()` | `std::future`を返す非同期版 | バックグラウンド処理 |
| `loginf_async<T>(callback)` | コールバック呼び出し | イベント駆動型プログラミング |

---

## 1. loginf_try - ノンブロッキング即座チェック

最も軽量な非同期APIです。呼び出し時に値が利用可能かどうかを即座にチェックし、結果を返します。

### シグネチャ

```cpp
template<typename T>
bool loginf_try(T& value);
```

### 対応型

- `int`
- `float`
- `double`

### 使用例

```cpp
#include "logfunc.h"

int main() {
    int value = 0;
    
    // ゲームループでの使用例
    while (game_running) {
        // ノンブロッキングで入力をチェック
        if (loginf_try(value)) {
            std::cout << "入力を受け取りました: " << value << "\n";
            process_input(value);
        }
        
        // ゲームの更新（ブロックされない！）
        update_game();
        render_frame();
        
        // 60 FPS
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    return 0;
}
```

### パフォーマンス

`loginf_try()` は以下の最適化を行っています：

1. **ファイル更新日時のキャッシュ**: 10ms以内の連続呼び出しでは、ファイルを開かずにキャッシュから判定
2. **軽量なファイルシステムチェック**: `std::filesystem::last_write_time()` を使用
3. **条件付きファイル読み取り**: 更新が検出された場合のみファイルを開く

---

## 2. loginf_timeout - タイムアウト付き待機

指定した時間内に入力が得られなければ `false` を返します。

### シグネチャ

```cpp
template<typename T>
bool loginf_timeout(T& value, std::chrono::milliseconds timeout);
```

### 使用例

```cpp
#include "logfunc.h"
#include <chrono>

int main() {
    int value = 0;
    
    // 5秒間待機
    if (loginf_timeout(value, std::chrono::seconds(5))) {
        std::cout << "値を受け取りました: " << value << "\n";
    } else {
        std::cout << "タイムアウト！デフォルト値を使用します\n";
        value = 100; // デフォルト値
    }
    
    return 0;
}
```

### 注意事項

- 内部で100msごとにポーリングを行います
- タイムアウト時間は厳密ではなく、最大100msの誤差があります

---

## 3. loginf_async<T>() - Future版

`std::future<T>` を返す非同期版です。バックグラウンドスレッドで入力を待機します。

### シグネチャ

```cpp
template<typename T>
std::future<T> loginf_async();
```

### 使用例

```cpp
#include "logfunc.h"
#include <future>

int main() {
    // 非同期で入力を待機開始
    auto future = loginf_async<int>();
    
    // メインスレッドは他の処理を続行
    std::cout << "バックグラウンドで入力を待機中...\n";
    
    // 定期的に結果をチェック
    while (future.wait_for(std::chrono::milliseconds(100)) != std::future_status::ready) {
        std::cout << "処理中...\n";
        do_some_work();
    }
    
    // 結果を取得
    int value = future.get();
    std::cout << "受信: " << value << "\n";
    
    return 0;
}
```

### ライブラリ版での使用

ライブラリ版では、型ごとに専用の関数が提供されています：

```cpp
#include "logfunc_lib.h"

std::future<int> loginf_async_int();
std::future<float> loginf_async_float();
std::future<double> loginf_async_double();
```

---

## 4. loginf_async<T>(callback) - コールバック版

コールバック関数を指定して、入力を受け取った時に呼び出されます。

### シグネチャ

```cpp
template<typename T, typename Callback>
void loginf_async(Callback callback);
```

### 使用例

```cpp
#include "logfunc.h"
#include <atomic>

int main() {
    std::atomic<bool> done{false};
    
    // コールバックを設定して非同期待機を開始
    loginf_async<int>([&done](int value) {
        std::cout << "コールバックで値を受信: " << value << "\n";
        done = true;
    });
    
    // メインスレッドは即座に続行
    std::cout << "メインスレッドは続行中...\n";
    
    // doneがtrueになるまで他の処理
    while (!done) {
        do_some_work();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
}
```

### 注意事項

- コールバックは別スレッドで呼び出されます
- スレッドは `detach()` されるため、明示的な終了待機が必要な場合は `std::atomic` などを使用してください
- コールバック内から共有リソースにアクセスする場合は適切な同期が必要です

---

## in.txt ファイルの形式

すべての `loginf*` 関数は以下の形式の入力ファイルを期待します：

```
# コメント行（#で始まる行は無視されます）
42
3.14

# 空行も無視されます
100
```

- 1行に1つの値
- `#` で始まる行はコメント
- 空行や空白のみの行は無視
- 値は行の先頭から読み取られます

---

## 使用シナリオ

### ゲーム開発

```cpp
// ゲームループ
void game_loop() {
    int player_command = 0;
    
    while (!should_quit) {
        // ノンブロッキングで入力チェック
        if (loginf_try(player_command)) {
            handle_player_input(player_command);
        }
        
        update_game_state();
        render_frame();
        wait_for_next_frame();
    }
}
```

### バッチ処理

```cpp
// タイムアウト付きでパラメータを待機
void batch_process() {
    double threshold = 0.0;
    
    std::cout << "閾値を入力してください（10秒以内）...\n";
    
    if (loginf_timeout(threshold, std::chrono::seconds(10))) {
        process_with_threshold(threshold);
    } else {
        std::cout << "デフォルト閾値 0.5 を使用します\n";
        process_with_threshold(0.5);
    }
}
```

### イベント駆動プログラミング

```cpp
// イベントループ
void event_loop() {
    // 非同期で入力を監視
    loginf_async<int>([](int event_code) {
        switch (event_code) {
            case 1: handle_start(); break;
            case 2: handle_stop(); break;
            case 0: handle_quit(); break;
        }
    });
    
    // メインイベントループ
    run_event_loop();
}
```

---

## トラブルシューティング

### 入力が認識されない

1. `in.txt` が正しいディレクトリに存在するか確認
2. ファイルが保存されているか確認（エディタが保存していない場合がある）
3. ファイルの形式が正しいか確認（BOMなしUTF-8推奨）

### loginf_try() が常に false を返す

- 入力ファイルに有効な値が含まれているか確認
- コメント行や空行だけではないか確認
- 値の型が正しいか確認（整数を期待しているのに小数が入力されている等）

### コールバックが呼ばれない

- `loginf_async()` はスレッドを `detach()` するため、メインスレッドが先に終了するとコールバックが呼ばれません
- `std::atomic<bool>` などで待機するか、十分な時間を確保してください

---

## 参照

- [README.md](README.md) - メインドキュメント
- [logfunc.h](include/logfunc.h) - ヘッダーオンリー版ソースコード
