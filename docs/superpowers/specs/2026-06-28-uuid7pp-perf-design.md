# uuid7pp 性能改善 — Design Spec

- 日付: 2026-06-28
- スコープ: uuid7pp ヘッダオンリーライブラリのホットパス性能改善
- ステータス: デザイン承認済み → 実装計画フェーズへ

## 1. ゴール & 非ゴール

### ゴール

- `generate()`, `to_chars()`, `from_chars()`, `extract_timestamp()` のホットパスを、ベースライン比で **1.2 倍以上** 高速化する
- 既存 public API は **意味互換** を維持しつつ、内部実装を NTTP テンプレ関数へ再構成する
- 新たに **バッチ生成 API** (例: `generate_batch(out, n)`) と **unsafe 直接 API** (例: `to_chars_upper`) を追加する
- Catch2 ベンチでベースラインと改善後を並べてレポートする

### 非ゴール

- マルチスレッド性能の大改造 (既存の `thread_local` を維持)
- xoshiro256++ のアルゴリズム変更
- v1/v4/v5 など他の UUID バージョン対応
- ライブラリ構造の分割 (引き続き単一ヘッダ)
- AVX-512 / NEON への明示的特化 (SIMDe のネイティブ実装に任せる)

## 2. アーキテクチャ概要

```
                    ┌─────────────────────────────────────┐
                    │ 既存 public API (意味互換を維持)    │
                    │   generate() / generate_at(...)     │
                    │   to_string() / to_chars()          │
                    │   from_chars()                      │
                    │   extract_timestamp()               │
                    │   get_version() / is_v7()           │
                    └─────────────┬───────────────────────┘
                                  │ (内部で detail:: に委譲)
                                  ▼
                    ┌─────────────────────────────────────┐
                    │ detail:: NTTP テンプレ特殊化         │
                    │   to_chars_impl<Upper, Hyphen>      │
                    │   from_chars_impl<ExpectHyphen>     │
                    │   pack_impl<...>                    │
                    └─────────────┬───────────────────────┘
                                  │
                                  ▼
                    ┌─────────────────────────────────────┐
                    │ 新規 public API                      │
                    │   generate_batch(out, n)             │
                    │   to_chars_upper / to_chars_plain   │
                    │   extract_timestamp_fast(u)          │
                    └─────────────────────────────────────┘
```

- 既存 API は薄いラッパとして残し、内部で NTTP 特殊化版へ委譲する
- 新規 API は「熱いコールサイトのペナルティ (分岐・タグディスパッチ・コピー) をゼロにしたいユーザ」向け
- SIMDe 依存と最適化レベル (`-O3 -march=native`) は維持

## 3. API 詳細

### 3.1 NTTP テンプレ化された内部関数

```cpp
namespace uuid7pp::detail {

template <bool Upper, bool Hyphen>
inline auto to_chars_impl(uuid const& u, char* out) noexcept -> void;

template <bool ExpectHyphen>
inline auto from_chars_impl(std::string_view s) noexcept -> std::optional<uuid>;

} // namespace uuid7pp::detail
```

- `Upper` / `Hyphen` / `ExpectHyphen` をコンパイル時定数にすることで、三項演算子と分岐が消える
- 既存 `to_chars(u, out, hyphen, upper)` は上記テンプレをディスパッチする薄いラッパに変更 (API は維持、内部で分岐)

### 3.2 バッチ生成 API

```cpp
namespace uuid7pp {
struct generator {
  // 戻り値: 生成した個数 (== n)
  static inline auto generate_batch(uuid* out, std::size_t n) noexcept -> std::size_t;
};
}
```

- 内部で `tls_state` を 1 回ロード → ループ内で `rng.next()` のみを回す
- 各イテレーションは `pack()` の SIMD 命令列 1 セット (ms/cnt/entropy 取得 + shuffle + store)
- 同一 ms でカウンタが飽和したら自動でエントロピーだけ進める分岐は維持 (RFC 9562 仕様)
- ホットパスから `system_clock::now()` の呼び出しを **1 回** に減らす

### 3.3 unsafe 直接 API

```cpp
namespace uuid7pp {

// 大文字・ハイフン有無をテンプレ化 (コンパイル時特殊化)
inline auto to_chars_upper(uuid const& u, char* out, bool hyphen = true) noexcept -> void;
inline auto to_chars_lower(uuid const& u, char* out, bool hyphen = true) noexcept -> void;
inline auto to_chars_plain(uuid const& u, char* out) noexcept -> void;            // 小文字・ハイフンなし
inline auto to_chars_plain_upper(uuid const& u, char* out) noexcept -> void;      // 大文字・ハイフンなし

} // namespace uuid7pp
```

- ランタイムの `upper ? ... : ...` 三項を完全排除
- `to_string(u, hyphen, upper)` の引数ディスパッチオーバーヘッドを排除

### 3.4 タイムスタンプ高速化

```cpp
namespace uuid7pp {

// 戻り値: ミリ秒整数 (48bit)
inline auto extract_timestamp_fast(uuid const& u) noexcept -> uint64_t;

} // namespace uuid7pp
```

- 内部は data[0..5] の 6 バイトを SIMD でロードし、`__builtin_bswap64` + マスクで 1 命令化
- 既存の `extract_timestamp()` はこの上に被せて `system_clock::time_point` 構築のみ残す

### 3.5 時刻取得の最適化

- `generate()` の `std::chrono::system_clock::now()` + `duration_cast` は **現状維持** (Phase 1〜4)
- ただし、`generate_batch(out, n)` はループ外で 1 回だけ取得して使い回す
- ベンチ結果次第で、後段 (Phase 5 以降) で `clock_gettime(CLOCK_REALTIME)` への置換をオプション化する (POSIX / MSVC で `#ifdef` 分岐)

## 4. データフロー

### 4.1 `generate()` (現状 → 改善後)

```
[現在] generate()
       → system_clock::now()
       → duration_cast<milliseconds>
       → generate_at(ms)
       → tls_state ロード
       → initialized 分岐
       → ms 比較
       → pack(ms, cnt, rng.next())
       → set_epi64x + shuffle + store_si128

[改善] generate()
       → system_clock::now()        (Phase 1〜4 は変更なし)
       → duration_cast<milliseconds>
       → generate_at(ms)
       → tls_state ロード (キャッシュライン整列は現状維持)
       → detail::pack_fast(ms, cnt, entropy) に分離
       → shuffle マスクを constexpr テーブル化
```

### 4.2 `to_chars` (現状 → 改善後)

```
[現在] to_chars(u, out, hyphen, upper)
       → load_si128
       → and / srli
       → shuffle(upper ? A : a)
       → unpacklo / unpackhi
       → 32byte tmp にストア
       → std::copy_n × 5 (ハイフン 4 個挿入)

[改善] to_chars_impl<Upper, Hyphen>
       → Hyphen == true:
         16+4+4+4+4+4 バイトのストアを
         8+8+4+4+12 バイトの aggregate store に再編
       → Hyphen == false:
         32 バイト一発ストア
       → std::copy_n を aggregate store で置換
```

### 4.3 `from_chars` (現状 → 改善後)

```
[現在] from_chars(s)
       → 長さ分岐 (36 / 32)
       → ハイフン位置 4 個をスカラ比較
       → copy_hex ループ (合計 32 byte コピー)
       → load_si128 × 2
       → hex_to_nibble × 2
       → mask チェック
       → storeu_si128 × 2
       → スカラで 16 バイト pack

[改善] from_chars_impl<ExpectHyphen>
       → 長さ分岐を NTTP で消す (呼び分けは呼び元で解決)
       → ハイフン位置を pcmpeqb + pmovmskb で一括チェック (ExpectHyphen=true 時)
       → 16 byte + 16 byte ロードの後、pshufb でハイフン跨ぎの
         クリーン配列を 1 命令で構成
       → nibbles → 16 byte pack を pshufb で一発化
```

## 5. エラーハンドリング

- 既存 `from_chars()` は長さ不正・ハイフン位置不正・非 HEX 文字を `std::nullopt` で返す動作を維持する
- NTTP 化しても分岐位置が変わるだけで、振る舞いは完全に同じ
- 新規 `generate_batch(out, n)` は `n == 0` を no-op とし、`out == nullptr && n > 0` は契約違反として `std::abort`
- 新規 `extract_timestamp_fast()` は UUID 構造体に依存しないので、不正値でもそのまま 48bit を返すだけ (呼び出し側責任)

## 6. テスト計画

### 6.1 機能回帰テスト

- 既存の `test/test_uuid7.cpp` は変更なしで全 PASS すること
- 追加で、NTTP 特殊化版 (`to_chars_impl<true, true>` 等) を直接テストして、結果が従来版と一致することを確認する

### 6.2 ベンチマーク

- 既存の `test/test_benchmark.cpp` をベースに、以下を追加:
  - `generate_batch(1000)` の 1 個あたり ns
  - `to_chars_upper` / `to_chars_plain` / `to_chars_impl<true, true>` 直接呼び出し
  - `from_chars_impl<true>` / `from_chars_impl<false>` 直接呼び出し
  - `extract_timestamp_fast` vs 既存 `extract_timestamp`
- ベースライン (改善前ビルド) と改善後で同じ Catch2 バイナリを使い、`--benchmark-samples=10` 程度のレポートを生成
- 結果は `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md` に保存

### 6.3 受け入れ条件

- 改善後の `generate()`, `to_chars()`, `from_chars()`, `extract_timestamp()` の **いずれか** がベースライン比で **1.2 倍以上** の改善
- ただし `to_chars` は現在 2.5 ns と極めて低いため、絶対下限を考慮して「1.2 倍以上」基準は **生成・from_chars・extract_timestamp のうち最低 1 つ** が達成すれば OK とする
- `test_uuid7.cpp` 全件 PASS
- ASan / UBSan クリーン (手元 or CI で 1 回)

## 7. 段階的リリース計画

1. **Phase 1**: 内部 NTTP テンプレ化 (`to_chars_impl`, `from_chars_impl`) — API 不変、内部リファクタのみ
2. **Phase 2**: `to_chars` のストア戦略刷新 (`std::copy_n` 除去) と `from_chars` の SIMD 強化
3. **Phase 3**: `extract_timestamp_fast` 追加 + `extract_timestamp` の高速化
4. **Phase 4**: `generate_batch` 追加 + `generate()` のホットパス仕上げ
5. **Phase 5**: unsafe API (`to_chars_upper` 等) の公開とベンチマーク更新

各 Phase 完了時に既存テスト全件 PASS を確認し、次フェーズへ進む。

## 8. 段階的ベンチ検証 & 不採用フィードバックルール

各 Phase (および Phase 内の個別改善ステップ) は以下を遵守する:

### 8.1 ベンチ検証の義務化

- 各改善ステップ完了時に **必ず Catch2 ベンチマーク** (`./build/test/uuid7pp_tests "[benchmark]"`) を実行する
- ベースライン (Phase 開始前の HEAD) と改善後を直接比較する
- ベンチは `--benchmark-samples=10` 以上で行い、誤差の小さい中央値を採用する
- 計測結果は `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md` に Phase 単位で追記する

### 8.2 性能劣化時の不採用判定

- 改善対象としたホットパスの **いずれか** でもベースラインより遅くなった場合、その改善は **不採用** とする
- 不採用の閾値: 中央値がベースライン比で **1.0 倍未満** (= 明確に遅い)
- 「同等だが僅かに遅い (±2% 以内)」も不採用 (保守的に判断)

### 8.3 不採用時の対応

- 改善コードは **`// REJECTED:` プレフィクスのコメントを付けて残す**
- コメントには以下を必ず記述する:
  - 適用した改善内容の概要
  - ベンチマーク結果 (ベースライン ns vs 改善後 ns)
  - 不採用と判定した理由 (推測ではなく計測に基づく事実)
  - 再挑戦時の参考情報 (例: 「分岐予測ミスが支配的」「レジスタ spill が発生」)
- 機能としては退行していなければ、コードはファイルに残して OK (機能テストは PASS しているはず)
- ただし、最終コミットには含めず、別ブランチ (`perf/rejected-attempts/`) に退避する

### 8.4 採用の記録

- 採用された改善は spec の §9 採用履歴に Phase 単位で記録する
- ベンチ数値 (中央値 ns / speedup 倍率) を残し、後続の改善の参考にする

## 9. リスクと対策

| リスク | 影響 | 対策 |
|--------|------|------|
| SIMDe の NTTP ディスパッチでコードサイズ肥大 | バイナリ肥大 | 既存 API ラッパは 1 関数に保ち、テンプレ特殊化は `[[gnu::always_inline]]` で抑制 |
| `thread_local` 周りの分岐が消せず予測ミスが残存 | generate 改善が頭打ち | `[[unlikely]]` ヒントと分岐順序の見直しでフォールスルー側を高速化 |
| 既存ユーザが `to_chars(u, out, hyphen, upper)` のシグネチャに依存 | API 破壊 | ラッパを残し意味互換を維持。テンプレ化は内部のみ |
| xoshiro256 の `std::rotl` が SIMD レジスタ跨ぎで遅くなる | 軽微 | ベンチで実測し、必要なら `__builtin_rotateleft64` に置換 |
| MSVC での `clock_gettime` 非対応 | Phase 5 遅延 | Phase 5 で `#ifdef _WIN32` の `QueryPerformanceCounter` 経路を別途用意 |
| 改善が機能テスト PASS でも性能劣化 | Phase 停滞 | §8.2 の不採用判定に従い、`// REJECTED:` コメントで残し退避 |

## 10. 採用履歴 (Phase 単位)

| Phase | 対象 | 代表ベンチ項目 | ベースライン ns | 改善後 ns | 倍率 | 採用/不採用 | 備考 |
|-------|------|--------------|----------------|-----------|------|------------|------|
| 1 | NTTP テンプレ化 (機能不変) | from_chars(hyphen) | 8.83 | 7.98 | 1.11x | 採用 | from_chars のみインライン版に戻した (NTTP ラッパが劣化したため)。detail:: テンプレートは残し、public API は元のインライン実装を維持。 |
| 2-5 | (後続 Phase) | | | | | | |

## 11. 参考リンク

- RFC 9562 — UUID v7 仕様
- SIMDe — https://github.com/simd-everywhere/simde
- Catch2 benchmark — `test/test_benchmark.cpp`
- ベンチ結果ログ — `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md`
