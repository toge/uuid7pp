# uuid7pp

[![CI](https://github.com/toge/uuid7pp/actions/workflows/ci.yml/badge.svg)](https://github.com/toge/uuid7pp/actions/workflows/ci.yml)

高速に動作する C++26 向けの UUID v7 生成・変換ライブラリです。SIMD (SSSE3/SSE4.1) を活用し、ロックフリーかつスレッドセーフな設計により、128ビット UUID の生成と文字列変換を高速に行います。

## 特徴

- **超高速生成**: `xoshiro256++` PRNGとSIMD命令を活用し、レジスタ内でUUID構造を構築します。
- **SIMD 最適化**: 文字列変換(Hex変換)、パース、UUID比較にSIMD命令を使用しています。
- **ゼロアロケーション**: `to_chars` 関数を使用することで、ヒープメモリ割当なしで文字列化が可能。
- **UUID v7 準拠**: RFC 9562 に基づく48bitタイムスタンプ、バージョン7、および単調増加性をサポート。
- **時刻指定生成**: 過去や未来の特定の時刻を指定したUUIDv7の生成。
- **エコシステム統合**: `std::hash` および `std::format` (`std::formatter`) をサポート。
- **スレッドセーフ**: `thread_local` を利用した状態管理により、ロックフリーで動作。
- **モダン C++**: C++26 の機能を活用（C++23でも動作可能）。
- **ポータブル SIMD**: [SIMDe](https://github.com/simd-everywhere/simde) を利用しているため、x86 以外（ARM NEON 等）への移植性も考慮されています。

## パフォーマンス (ベンチマーク)

Catch2 を使用した、`boost::uuid` (v1.90.0) との比較結果です。
(環境: Linux, GCC 15.2, x86_64 native)

| 操作                         | uuid7pp      | boost::uuid | 比較           |
| :--------------------------- | :----------- | :---------- | :------------- |
| **生成 (generate)**          | **~27.4 ns** | ~37.9 ns    | **1.38x 高速** |
| **文字列変換 (std::string)** | **~9.6 ns**  | ~13.0 ns    | **1.35x 高速** |
| **文字列変換 (to_chars)**    | **~2.5 ns**  | N/A         | N/A            |

*注: `to_chars` はヒープ割当を行わないため、圧倒的なパフォーマンスを発揮します。*

## 使い方

### 基本的な生成と文字列変換

```cpp
#include "uuid7pp.hpp"
#include <iostream>
#include <format>

int main() {
    // 現在時刻で UUID v7 の生成
    auto const id = uuid7pp::generator::generate();

    // std::format による出力 (C++20/23)
    // {:x} 小文字・ハイフン (デフォルト)
    // {:X} 大文字・ハイフン
    // {:n} 小文字・ハイフンなし
    // {:N} 大文字・ハイフンなし
    std::cout << std::format("ID: {:X}\n", id);

    // 文字列からのパース (ハイフンの有無を問いません)
    auto const parsed = uuid7pp::from_chars("018E8C8A-EF80-7A00-BF9A-3F1F3A2C4D5E");
    if (parsed) {
        // ...
    }

    return 0;
}
```

### コンテナでの使用

`std::hash` に対応しているため、そのまま `unordered_set` 等で使用できます。

```cpp
std::unordered_set<uuid7pp::uuid> set;
set.insert(uuid7pp::generator::generate());
```

### パフォーマンス重視 (ゼロアロケーション)

```cpp
char buf[36];
auto const id = uuid7pp::generator::generate();
uuid7pp::to_chars(id, buf, true, false); // ハイフンあり、小文字
```

## インストール

### 依存関係
- C++23 以上のコンパイラ
- [SIMDe](https://github.com/simd-everywhere/simde)

### CMake による組み込み
```cmake
find_package(uuid7pp REQUIRED)
target_link_libraries(your_target PRIVATE uuid7pp::uuid7pp)
```

## ライセンス

MIT License
