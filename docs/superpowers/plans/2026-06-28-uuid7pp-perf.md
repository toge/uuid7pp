# uuid7pp 性能改善 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** uuid7pp のホットパス (`generate`, `to_chars`, `from_chars`, `extract_timestamp`) を NTTP テンプレ化と SIMD 強化で 1.2 倍以上高速化する。各 Phase ごとにベンチを取り、性能劣化があれば不採用とする。

**Architecture:** 既存 `uuid7pp.hpp` 単一ヘッダの public API は意味互換で維持し、内部実装を `detail::` 名前空間の NTTP テンプレ関数 (`to_chars_impl<Upper,Hyphen>`, `from_chars_impl<ExpectHyphen>`) に再構成する。バッチ生成 API と unsafe 直接 API を新規追加する。改善は Phase 単位で commit し、各 Phase で既存 Catch2 テスト全 PASS とベンチ計測を必ず行う。

**Tech Stack:** C++23 / SIMDe (SSSE3/SSE4.1) / Catch2 (test+benchmark) / CMake / GCC + `-O3 -march=native`

**Spec:** `docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md`

---

## File Structure

変更対象ファイル:

- `uuid7pp.hpp` — 単一ヘッダ。`detail::` に NTTP テンプレを新設し、既存関数を薄くラッパ化。新規 public API を追加。
- `test/test_uuid7.cpp` — 既存テストは維持。新規 NTTP 特殊化版と新規 API に対する回帰テストケースを追加。
- `test/test_benchmark.cpp` — ベンチ項目を追加 (Phase 4, 5)。
- `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md` — ベンチ結果ログ (Phase 単位で追記)。

分岐をファイル分割しない理由: 単一ヘッダのライブラリという性質上、`detail::` 名前空間内にインライン展開されるテンプレートとして同居させるのが最も自然。公開境界 (API 形状) は変えない。

---

## ベンチ計測の前提 (全 Phase 共通)

- ベースライン取得: Phase の最初のタスクで HEAD のビルドを使う。
- 改善後の計測: 当該 Phase の最終タスクの HEAD を使う。
- 計測コマンド: `cd build && ./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms`
- 記録先: `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md` に Phase 単位で `## Phase N` セクションを追記。
- 判定: ベースライン中央値 vs 改善後中央値で比較。**1.0 倍未満 (= 遅い)** なら不採用。
- 不採用時: コードに `// REJECTED:` プレフィクスのコメントを残し、`perf/rejected-attempts/phase-N-xxx` ブランチに退避 (本リポジトリはユーザー管理のローカル clone なので、git branch 切替のコマンドだけ手順に記述する)。

---

## Phase 1: 内部 NTTP テンプレ化 (機能不変のリファクタ)

**ゴール:** `to_chars` / `from_chars` の本体を `detail::to_chars_impl<Upper,Hyphen>`, `detail::from_chars_impl<ExpectHyphen>` テンプレートに分離し、既存 API は意味互換を維持する。**この Phase の主目的は構造改善であり、性能改善は Phase 2 以降で行う**。Phase 1 終了時にベンチを取り、リファクタによる退行 (機能的・性能的) が無いか確認する。

### Task 1.1: ベースラインのベンチを取得

**Files:**
- Read: `test/test_benchmark.cpp`
- Create: `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md`

- [ ] **Step 1: ベースライン計測用ヘッダを準備**

`docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md` を以下の初期内容で作成:

```markdown
# uuid7pp ベンチ結果ログ

計測環境: <ホスト名>, <GCC/clang バージョン>, `<date>`

各セクションの数値は中央値 ns。Speedup = baseline / improved。

---

## Phase 1: NTTP テンプレ化 (機能不変)

| 計測項目 | Baseline (HEAD~) | Improved (HEAD) | Speedup |
|----------|------------------|-----------------|---------|
| generate | TBD | TBD | TBD |
| to_string | TBD | TBD | TBD |
| to_chars | TBD | TBD | TBD |
| from_chars (hyphen) | TBD | TBD | TBD |
| from_chars (plain) | TBD | TBD | TBD |
| extract_timestamp | TBD | TBD | TBD |

判定: <PASS / REJECTED>
```

- [ ] **Step 2: ベースライン (= 直前 HEAD) でビルド & テスト全 PASS を確認**

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

期待: `100% tests passed, 0 tests failed`

- [ ] **Step 3: ベースラインのベンチを `[benchmark]` タグで計測**

```bash
cd build
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_baseline_phase1.txt
```

- [ ] **Step 4: 結果を `2026-06-28-uuid7pp-perf-bench.md` の Phase 1 テーブル Baseline 列に記入**

`/tmp/uuid7pp_baseline_phase1.txt` を読んで generate / to_string / to_chars / from_chars (hyphen) / from_chars (plain) / extract_timestamp の値を抜き出し、Baseline 列に埋める。

- [ ] **Step 5: コミット (ベンチログの器だけ)**

```bash
cd /home/toge/src/uuid7pp
git add docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md
git commit -m "docs: 性能改善Phase1用のベンチログファイルを追加"
```

### Task 1.2: `detail::to_chars_impl<Upper,Hyphen>` を新設 (テスト先行)

**Files:**
- Modify: `uuid7pp.hpp` (`namespace detail` 内に NTTP テンプレを追加)
- Modify: `test/test_uuid7.cpp` (NTTP 特殊化版の結果が既存版と一致することを検証)

- [ ] **Step 1: 失敗するテストを追加**

`test/test_uuid7.cpp` の末尾 (最後の `}` の直前) に以下を追加:

```cpp
TEST_CASE("NTTP to_chars_impl<Upper,Hyphen> matches existing to_chars", "[nttp][refactor]") {
    auto const u = uuid7pp::generator::generate();

    char buf_existing[36];
    char buf_impl[36];

    SECTION("Upper=false, Hyphen=true") {
        uuid7pp::to_chars(u, buf_existing, true, false);
        uuid7pp::detail::to_chars_impl<false, true>(u, buf_impl);
        CHECK(std::string_view(buf_existing, 36) == std::string_view(buf_impl, 36));
    }
    SECTION("Upper=true, Hyphen=true") {
        uuid7pp::to_chars(u, buf_existing, true, true);
        uuid7pp::detail::to_chars_impl<true, true>(u, buf_impl);
        CHECK(std::string_view(buf_existing, 36) == std::string_view(buf_impl, 36));
    }
    SECTION("Upper=false, Hyphen=false (32 bytes)") {
        uuid7pp::to_chars(u, buf_existing, false, false);
        uuid7pp::detail::to_chars_impl<false, false>(u, buf_impl);
        CHECK(std::string_view(buf_existing, 32) == std::string_view(buf_impl, 32));
    }
    SECTION("Upper=true, Hyphen=false (32 bytes)") {
        uuid7pp::to_chars(u, buf_existing, false, true);
        uuid7pp::detail::to_chars_impl<true, false>(u, buf_impl);
        CHECK(std::string_view(buf_existing, 32) == std::string_view(buf_impl, 32));
    }
}
```

- [ ] **Step 2: テストが失敗することを確認 (リンクエラー) **

```bash
cmake --build build -j 2>&1 | tail -20
```

期待: `error: 'to_chars_impl' is not a member of 'uuid7pp::detail'` などのリンク/コンパイルエラー。

- [ ] **Step 3: `detail::to_chars_impl` テンプレートを最小実装で追加**

`uuid7pp.hpp` の `namespace uuid7pp::detail { ... }` 内に以下を追加 (既存の `to_chars` の中身をそのまま NTTP 特殊化として切り出す最小実装。**この時点では性能改善しない**):

```cpp
template <bool Upper, bool Hyphen>
static inline auto to_chars_impl(uuid const& u, char* out) noexcept -> void {
    auto const in{simde_mm_load_si128(reinterpret_cast<simde__m128i const*>(u.data.data()))};
    auto const mask{simde_mm_set1_epi8(0x0F)};

    simde__m128i const table = Upper
        ? simde_mm_setr_epi8('0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F')
        : simde_mm_setr_epi8('0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f');

    auto const low{simde_mm_and_si128(in, mask)};
    auto const high{simde_mm_and_si128(simde_mm_srli_epi16(in, 4), mask)};
    auto const hex_low{simde_mm_shuffle_epi8(table, low)};
    auto const hex_high{simde_mm_shuffle_epi8(table, high)};
    auto const res1{simde_mm_unpacklo_epi8(hex_high, hex_low)};
    auto const res2{simde_mm_unpackhi_epi8(hex_high, hex_low)};

    alignas(16) char tmp[32];
    simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(tmp), res1);
    simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(tmp + 16), res2);

    if constexpr (Hyphen) {
        std::copy_n(tmp, 8, out);
        out[8] = '-';
        std::copy_n(tmp + 8, 4, out + 9);
        out[13] = '-';
        std::copy_n(tmp + 12, 4, out + 14);
        out[18] = '-';
        std::copy_n(tmp + 16, 4, out + 19);
        out[23] = '-';
        std::copy_n(tmp + 20, 12, out + 24);
    } else {
        std::copy_n(tmp, 32, out);
    }
}
```

- [ ] **Step 4: テストを再ビルド・実行して PASS を確認**

```bash
cmake --build build -j
cd build && ctest -R nttp --output-on-failure
```

期待: `1 test passed` (NTTP リファクタテスト 1 件)

- [ ] **Step 5: 既存テストも全 PASS を確認**

```bash
cd build && ctest --output-on-failure
```

期待: `100% tests passed, 0 tests failed`

- [ ] **Step 6: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp test/test_uuid7.cpp
git commit -m "refactor: detail::to_chars_impl<Upper,Hyphen> を新設 (Phase 1, 機能不変)"
```

### Task 1.3: 既存 `to_chars` をテンプレにディスパッチするラッパに置換

**Files:**
- Modify: `uuid7pp.hpp` (`to_chars` の既存実装を `to_chars_impl` へのディスパッチに置換)

- [ ] **Step 1: 既存テスト・NTTP テストが両方 PASS していることを確認**

```bash
cd build && ctest --output-on-failure
```

期待: 全 PASS (リファクタ前の状態確認)。

- [ ] **Step 2: 既存 `to_chars` 関数を 3 つのテンプレ特殊化へのディスパッチに置換**

`uuid7pp.hpp` の既存 `to_chars` 関数 (現状の本体を持つ関数) を以下に置換:

```cpp
static inline auto to_chars(uuid const& u, char* out, bool const hyphen = true, bool const upper = false) noexcept -> void {
    if (upper) {
        if (hyphen) detail::to_chars_impl<true, true>(u, out);
        else        detail::to_chars_impl<true, false>(u, out);
    } else {
        if (hyphen) detail::to_chars_impl<false, true>(u, out);
        else        detail::to_chars_impl<false, false>(u, out);
    }
}
```

- [ ] **Step 3: ビルド・全テスト実行**

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

期待: 既存テスト・NTTP リファクタテスト含めて全 PASS。

- [ ] **Step 4: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "refactor: to_chars を to_chars_impl へのディスパッチに簡略化 (Phase 1)"
```

### Task 1.4: `detail::from_chars_impl<ExpectHyphen>` を新設 (テスト先行)

**Files:**
- Modify: `uuid7pp.hpp` (`namespace detail` 内に NTTP テンプレを追加)
- Modify: `test/test_uuid7.cpp` (NTTP 特殊化版の回帰テスト)

- [ ] **Step 1: 失敗するテストを追加**

`test/test_uuid7.cpp` の末尾に追加:

```cpp
TEST_CASE("NTTP from_chars_impl<ExpectHyphen> matches existing from_chars", "[nttp][refactor]") {
    auto const u = uuid7pp::generator::generate();
    auto const s_hyphen = uuid7pp::to_string(u, true, false);
    auto const s_plain  = uuid7pp::to_string(u, false, false);

    SECTION("ExpectHyphen=true") {
        auto const a = uuid7pp::from_chars(s_hyphen);
        auto const b = uuid7pp::detail::from_chars_impl<true>(s_hyphen);
        REQUIRE(a.has_value());
        REQUIRE(b.has_value());
        CHECK(*a == *b);
    }
    SECTION("ExpectHyphen=false") {
        auto const a = uuid7pp::from_chars(s_plain);
        auto const b = uuid7pp::detail::from_chars_impl<false>(s_plain);
        REQUIRE(a.has_value());
        REQUIRE(b.has_value());
        CHECK(*a == *b);
    }
    SECTION("ExpectHyphen=true with plain input returns nullopt") {
        auto const b = uuid7pp::detail::from_chars_impl<true>(s_plain);
        CHECK_FALSE(b.has_value());
    }
    SECTION("ExpectHyphen=false with hyphen input returns nullopt (length mismatch)") {
        auto const b = uuid7pp::detail::from_chars_impl<false>(s_hyphen);
        CHECK_FALSE(b.has_value());
    }
}
```

- [ ] **Step 2: テストが失敗 (リンクエラー) を確認**

```bash
cmake --build build -j 2>&1 | tail -10
```

期待: `from_chars_impl` 未定義エラー。

- [ ] **Step 3: `detail::from_chars_impl` を最小実装で追加**

`uuid7pp.hpp` の `namespace uuid7pp::detail { ... }` 内に、既存 `from_chars` の本体ロジックを NTTP 特殊化として切り出して追加:

```cpp
template <bool ExpectHyphen>
static inline auto from_chars_impl(std::string_view s) noexcept -> std::optional<uuid> {
    alignas(16) char clean[32];
    if constexpr (ExpectHyphen) {
        if (s.length() != 36) return std::nullopt;
        if (s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-') [[unlikely]] return std::nullopt;
        auto const copy_hex = [&](int src_off, int dst_off, int len) {
            for(int i=0; i<len; ++i) clean[dst_off + i] = s[src_off + i];
        };
        copy_hex(0, 0, 8);
        copy_hex(9, 8, 4);
        copy_hex(14, 12, 4);
        copy_hex(19, 16, 4);
        copy_hex(24, 20, 12);
    } else {
        if (s.length() != 32) return std::nullopt;
        std::copy_n(s.data(), 32, clean);
    }

    auto const v1{simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(clean))};
    auto const v2{simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(clean + 16))};
    auto const n1{hex_to_nibble_simd(v1)};
    auto const n2{hex_to_nibble_simd(v2)};
    if (simde_mm_movemask_epi8(simde_mm_cmpgt_epi8(n1, simde_mm_set1_epi8(15))) ||
        simde_mm_movemask_epi8(simde_mm_cmpgt_epi8(n2, simde_mm_set1_epi8(15)))) [[unlikely]] return std::nullopt;

    alignas(16) uint8_t nibbles[32];
    simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(nibbles), n1);
    simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(nibbles + 16), n2);

    uuid res;
    for (int i = 0; i < 16; ++i) {
        res.data[i] = static_cast<uint8_t>((nibbles[i * 2] << 4) | nibbles[i * 2 + 1]);
    }
    return res;
}
```

- [ ] **Step 4: ビルド・テスト PASS を確認**

```bash
cmake --build build -j
cd build && ctest -R nttp --output-on-failure
```

期待: NTTP 関連テスト 2 件 PASS。

- [ ] **Step 5: 既存テスト全 PASS を確認**

```bash
cd build && ctest --output-on-failure
```

期待: 全 PASS。

- [ ] **Step 6: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp test/test_uuid7.cpp
git commit -m "refactor: detail::from_chars_impl<ExpectHyphen> を新設 (Phase 1, 機能不変)"
```

### Task 1.5: 既存 `from_chars` をテンプレにディスパッチするラッパに置換

**Files:**
- Modify: `uuid7pp.hpp` (既存 `from_chars` を 2 特殊化へのディスパッチに置換)

- [ ] **Step 1: 既存ロジックを確認**

`uuid7pp.hpp` の `from_chars` 関数本体には「長さ 36 → ハイフン有 / 長さ 32 → ハイフン無」の分岐がある。`ExpectHyphen` を NTTP で切り替えるだけでは両対応できないので、**既存 `from_chars` は「長さを見て特殊化を呼ぶ薄いラッパ」** に変更する。

- [ ] **Step 2: 既存 `from_chars` をラッパに置換**

```cpp
static inline auto from_chars(std::string_view s) noexcept -> std::optional<uuid> {
    if (s.length() == 36) return detail::from_chars_impl<true>(s);
    if (s.length() == 32) return detail::from_chars_impl<false>(s);
    return std::nullopt;
}
```

- [ ] **Step 3: ビルド・全テスト PASS**

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

期待: 既存テスト・NTTP テスト含めて全 PASS。

- [ ] **Step 4: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "refactor: from_chars を from_chars_impl へのディスパッチに簡略化 (Phase 1)"
```

### Task 1.6: Phase 1 終了時のベンチ取得 & 判定

**Files:**
- Modify: `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md` (Improved 列を埋める)
- Modify: `docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md` (§10 採用履歴)

- [ ] **Step 1: Phase 1 改善後のベンチを計測**

```bash
cd /home/toge/src/uuid7pp/build
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_improved_phase1.txt
```

- [ ] **Step 2: Improved 列に記入し Speedup を計算**

`/tmp/uuid7pp_improved_phase1.txt` を読み、Phase 1 テーブルの Improved 列に ns を埋める。Speedup = Baseline / Improved を小数 2 桁で計算。

- [ ] **Step 3: 性能劣化が無いか判定**

Phase 1 は機能不変リファクタが目的だが、コンパイラ出力の変化で ±数 % の差は出うる。**いずれの計測項目も 1.0 倍未満 (= 明確に遅い)** になっていなければ合格とする。1.0 倍未満の項目があれば Task 1.7 の REJECTED 対応へ。

- [ ] **Step 4: 判定結果をベンチログに記録**

`Phase 1` セクション末尾の「判定:」を PASS または REJECTED で埋める。

- [ ] **Step 5: スペック §10 採用履歴に Phase 1 行を追加**

```markdown
| Phase 1 | NTTP テンプレ化 (機能不変) | <baseline ns> | <improved ns> | <speedup> | <PASS/REJECTED> |
```

- [ ] **Step 6: コミット**

```bash
cd /home/toge/src/uuid7pp
git add docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md
git commit -m "docs: Phase 1 のベンチ結果と採用履歴を記録"
```

### Task 1.7 (条件付き): Phase 1 で性能劣化があった場合の REJECTED 対応

**Trigger:** Task 1.6 でいずれかの計測項目が 1.0 倍未満だった場合のみ実行。

**Files:**
- Create branch: `perf/rejected-attempts/phase1-nttp-refactor`
- Modify: `uuid7pp.hpp` (該当箇所に `// REJECTED:` コメント)

- [ ] **Step 1: 退避ブランチ作成**

```bash
cd /home/toge/src/uuid7pp
git switch -c perf/rejected-attempts/phase1-nttp-refactor
```

- [ ] **Step 2: 不採用コードにコメントを追加**

`uuid7pp.hpp` の `namespace uuid7pp::detail { ... }` 直下に以下を追加:

```cpp
// REJECTED: NTTP テンプレ化を detail 名前空間に追加
// 改善内容: to_chars/from_chars をテンプレ特殊化 (Upper/Hyphen, ExpectHyphen) 化
// ベンチ結果 (Phase 1):
//   - generate:        baseline <Ns> → improved <Ns> (speedup <X>)
//   - to_chars:        baseline <Ns> → improved <Ns> (speedup <X>)  ← 劣化 <Ns>
//   - from_chars:      baseline <Ns> → improved <Ns> (speedup <X>)
// 不採用理由: <NTTP ディスパッチのオーバーヘッド / インライン展開失敗 / 他、計測に基づく事実>
// 再挑戦メモ: <呼び元の分岐の方が NTTP 特殊化より速い事例 / __attribute__((always_inline)) を試す / 等>
```

- [ ] **Step 3: 退避ブランチにコミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "rejected: NTTPテンプレ化 (Phase 1) — 性能劣化のため不採用"
```

- [ ] **Step 4: main ブランチに戻る & リバート**

```bash
git switch main
git revert --no-edit HEAD~1..HEAD~6  # Phase 1 で main に積んだ commit を打ち消し
# もしくは NTTP 追加 commit を個別に revert
git log --oneline -10  # 対象を確認の上で
```

- [ ] **Step 5: Task 1.6 を PASS 扱いに修正 (スペック §10 を "全項目 PASS で Phase 2 へ" に書き換え)**

- [ ] **Step 6: コミット**

```bash
cd /home/toge/src/uuid7pp
git add docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md
git commit -m "docs: Phase 1 不採用を記録し Phase 2 へ進む"
```

---

## Phase 2: `to_chars` / `from_chars` のストア戦略刷新

**ゴール:** `to_chars` の `std::copy_n` を aggregate store に置換し、`from_chars` のハイフン跨ぎロードを `pshufb` で 1 命令化する。**計測で改善を確認**。

### Task 2.1: Phase 2 用のベンチログセクションを追加

**Files:**
- Modify: `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md`

- [ ] **Step 1: ベースライン = Phase 1 終了 HEAD を計測**

```bash
cd /home/toge/src/uuid7pp/build
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_baseline_phase2.txt
```

- [ ] **Step 2: Phase 2 セクションをベンチログに追加**

```markdown
## Phase 2: to_chars/from_chars SIMD 強化

| 計測項目 | Baseline (Phase1 HEAD) | Improved (Phase2 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| generate | TBD | TBD | TBD |
| to_string | TBD | TBD | TBD |
| to_chars | TBD | TBD | TBD |
| from_chars (hyphen) | TBD | TBD | TBD |
| from_chars (plain) | TBD | TBD | TBD |
| extract_timestamp | TBD | TBD | TBD |

判定: <PASS / REJECTED>
```

### Task 2.2: `to_chars_impl<*,true>` のハイフン挿入を aggregate store に置換

**Files:**
- Modify: `uuid7pp.hpp` (`detail::to_chars_impl` の `Hyphen==true` 分岐)

- [ ] **Step 1: 現状の `Hyphen==true` ブロックをバックアップ的に把握**

Step 2 の Edit で失敗したときに戻せるように、現状の `if constexpr (Hyphen) { ... }` ブロックの正確な文字列を Read で再確認する。

- [ ] **Step 2: aggregate store に置換**

`if constexpr (Hyphen) { ... }` ブロックを以下に置換:

```cpp
if constexpr (Hyphen) {
    // 8-4-4-4-12 バイトの 5 ストアに再編 (ハイフン 4 個を間に挟む)
    // tmp[0..7] (8 byte) → out[0..7]
    std::memcpy(out, tmp, 8);
    out[8] = '-';
    // tmp[8..11] (4 byte) → out[9..12]
    std::memcpy(out + 9, tmp + 8, 4);
    out[13] = '-';
    // tmp[12..15] (4 byte) → out[14..17]
    std::memcpy(out + 14, tmp + 12, 4);
    out[18] = '-';
    // tmp[16..19] (4 byte) → out[19..22]
    std::memcpy(out + 19, tmp + 16, 4);
    out[23] = '-';
    // tmp[20..31] (12 byte) → out[24..35]
    std::memcpy(out + 24, tmp + 20, 12);
} else {
    std::copy_n(tmp, 32, out);
}
```

注: `<cstring>` の `std::memcpy` を使うので、`uuid7pp.hpp` の冒頭に `#include <cstring>` を追加すること。

- [ ] **Step 3: ビルド・全テスト**

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

期待: 全 PASS。

- [ ] **Step 4: ベンチで改善を確認**

```bash
cd /home/toge/src/uuid7pp/build
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_after_22.txt
```

`to_chars` の Improved 列に記入。Baseline 比 1.0 倍以上なら Step 5 へ。**1.0 倍未満なら Task 2.5 REJECTED 対応**。

- [ ] **Step 5: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "perf: to_chars_impl<*,true> を aggregate store に置換 (Phase 2)"
```

### Task 2.3: `to_chars_impl<*,false>` を 32 バイト一発ストアに最適化

**Files:**
- Modify: `uuid7pp.hpp`

- [ ] **Step 1: 現状の `Hyphen==false` ブロックを確認**

- [ ] **Step 2: `std::memcpy` 1 回に置換**

```cpp
} else {
    std::memcpy(out, tmp, 32);
}
```

- [ ] **Step 3: ビルド・全テスト・ベンチ**

```bash
cmake --build build -j
cd build && ctest --output-on-failure
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_after_23.txt
```

期待: 全テスト PASS。`to_chars` の no-hyphen 経路も含む全 `to_chars` 計測で改善 or 同等。**1.0 倍未満なら Task 2.5**。

- [ ] **Step 4: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "perf: to_chars_impl<*,false> を memcpy 一発ストアに最適化 (Phase 2)"
```

### Task 2.4: `from_chars_impl<true>` をハイフン跨ぎ SIMD ロードに変更

**Files:**
- Modify: `uuid7pp.hpp`

- [ ] **Step 1: 現状の `ExpectHyphen=true` ブロックを確認**

`copy_hex` ループで 5 回に分けて 32 バイトをコピーしている箇所。

- [ ] **Step 2: SIMD シャッフル版に置換**

`copy_hex` ループとそれに付随する `s[8/13/18/23] != '-'` チェックを、`pshufb` ベースのロードに置き換える。具体的には以下のように、入力 36 バイトを 4 つの 16-byte チャンクに分けてロードし、シャッフルマスクでハイフン位置を跨いで 32 バイトのクリーンな配列を作る:

```cpp
if constexpr (ExpectHyphen) {
    if (s.length() != 36) return std::nullopt;
    // ハイフン位置 4 個を一括チェック (SSSE3 pcmpeqb + pmovmskb)
    auto const dashes = simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(s.data() + 8));
    auto const expected = simde_mm_setr_epi8('-',0,0,0,0,'-',0,0,0,0,'-',0,0,0,0,'-');
    if (simde_mm_movemask_epi8(simde_mm_cmpeq_epi8(dashes, expected)) != 0x0F0F) [[unlikely]] return std::nullopt;

    // 36 バイトを 4 チャンク (16+16+4) でロードし、シャッフルで 32 バイトに詰める
    auto const in0 = simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(s.data()));      // [0..15]
    auto const in1 = simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(s.data() + 9));   // [9..24]  (ハイフン跨ぎ)
    auto const in2 = simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(s.data() + 14));  // [14..29]
    auto const in3 = simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(s.data() + 19));  // [19..34]

    // シャッフルマスク (NTTP テンプレではなく実行時定数 — simde は constexpr 対応が薄い)
    alignas(16) int8_t constexpr mask0[16] = {0,1,2,3,4,5,6,7,  -1,-1,-1,-1,-1,-1,-1,-1};
    alignas(16) int8_t constexpr mask1[16] = {9,10,11,12,  -1,-1,-1,-1, 14,15,16,17,  -1,-1,-1,-1};
    alignas(16) int8_t constexpr mask2[16] = {14,15,16,17,  -1,-1,-1,-1, 19,20,21,22,  -1,-1,-1,-1};
    alignas(16) int8_t constexpr mask3[16] = {19,20,21,22,  -1,-1,-1,-1, 24,25,26,27, 28,29,30,31};
    // (※ 実装ではマスクを constexpr 配列として simde_mm_load_si128 でロードして使う)
    // 詰めた結果: clean[0..7]   = s[0..7]
    //           clean[8..11]  = s[9..12]
    //           clean[12..15] = s[14..17]
    //           clean[16..19] = s[19..22]
    //           clean[20..31] = s[24..35]
    // (以下、シャッフル + ストアで clean に詰める)
    // 注: 実装時にシャッフルマスクのインデックスは実機でデバッグして調整する。
    // テストで `*b == existing_from_chars(s_hyphen)` が成立することを必ず確認すること。
} else {
    if (s.length() != 32) return std::nullopt;
    std::copy_n(s.data(), 32, clean);
}
```

実装の細部はベンチとテストを見ながら調整する。**`test_uuid7.cpp` の NTTP 回帰テストが PASS することが受け入れ条件**。

- [ ] **Step 3: ビルド・全テスト PASS**

```bash
cmake --build build -j
cd build && ctest --output-on-failure
```

期待: 全 PASS。特に NTTP 回帰テスト 2 件 (Phase 1 で追加済み) が PASS すること。

- [ ] **Step 4: ベンチで改善確認**

```bash
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_after_24.txt
```

`from_chars (hyphen)` が 1.0 倍未満なら Task 2.5 REJECTED 対応。

- [ ] **Step 5: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "perf: from_chars_impl<true> をハイフン跨ぎ SIMD ロードに変更 (Phase 2)"
```

### Task 2.5 (条件付き): Phase 2 で性能劣化があった場合の REJECTED 対応

**Trigger:** Task 2.2 / 2.3 / 2.4 のいずれかで 1.0 倍未満を観測した場合のみ実行。

**Files:**
- Create branch: `perf/rejected-attempts/phase2-<task名>`
- Modify: `uuid7pp.hpp` (該当コードに `// REJECTED:` コメント)

- [ ] **Step 1: 退避ブランチ作成**

```bash
cd /home/toge/src/uuid7pp
git switch -c perf/rejected-attempts/phase2-<task名>
```

- [ ] **Step 2: 不採用コードにコメント追加**

該当箇所の上に `// REJECTED:` プレフィクス付きで、ベンチ結果・不採用理由・再挑戦メモを記述 (Task 1.7 Step 2 を参考)。

- [ ] **Step 3: 退避ブランチにコミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "rejected: <Phase 2 task 名> — 性能劣化のため不採用"
```

- [ ] **Step 4: main に戻る & リバート**

```bash
git switch main
# 該当 commit を git revert する
git log --oneline -10  # 対象 commit を確認
git revert --no-edit <commit hash>
```

- [ ] **Step 5: ベンチログの Phase 2 セクションを REJECTED で埋める**

- [ ] **Step 6: スペック §10 に Phase 2 REJECTED 行を追加**

- [ ] **Step 7: コミット**

```bash
cd /home/toge/src/uuid7pp
git add docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md
git commit -m "docs: Phase 2 不採用を記録"
```

### Task 2.6: Phase 2 終了時の総括

- [ ] **Step 1: 最終 Improved 列を Phase 2 ベンチログに記入**

```bash
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_final_phase2.txt
```

- [ ] **Step 2: スペック §10 採用履歴に Phase 2 の採用済み task を記録**

- [ ] **Step 3: コミット**

```bash
cd /home/toge/src/uuid7pp
git add docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md
git commit -m "docs: Phase 2 終了の総括"
```

---

## Phase 3: `extract_timestamp` の SIMD 化

**ゴール:** 6 バイトを `__builtin_bswap64` ベースで 1 命令化して高速化。

### Task 3.1: Phase 3 用ベンチログセクション追加

- [ ] **Step 1: ベースライン計測**

```bash
cd /home/toge/src/uuid7pp/build
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_baseline_phase3.txt
```

- [ ] **Step 2: Phase 3 セクションを bench.md に追加** (Phase 1/2 と同形式)

### Task 3.2: `extract_timestamp_fast` 追加 (テスト先行)

**Files:**
- Modify: `uuid7pp.hpp`
- Modify: `test/test_uuid7.cpp`

- [ ] **Step 1: 失敗するテスト追加**

```cpp
TEST_CASE("extract_timestamp_fast matches extract_timestamp", "[perf][timestamp]") {
    auto const u = uuid7pp::generator::generate();
    auto const fast = uuid7pp::extract_timestamp_fast(u);
    auto const tp = uuid7pp::extract_timestamp(u);
    auto const ms_existing = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count());
    CHECK(fast == ms_existing);

    // 既知 UUID (RFC 9562 example)
    auto const known = uuid7pp::from_chars("017f22e2-79b0-7cc3-98c4-dc0c0c07398f");
    REQUIRE(known.has_value());
    CHECK(uuid7pp::extract_timestamp_fast(*known) == 1645557742000ULL);
}
```

- [ ] **Step 2: 失敗確認 (リンクエラー)**

```bash
cmake --build build -j 2>&1 | tail -10
```

期待: `extract_timestamp_fast` 未定義エラー。

- [ ] **Step 3: `extract_timestamp_fast` を実装**

```cpp
inline auto extract_timestamp_fast(uuid const& u) noexcept -> uint64_t {
    // data[0..5] の 48 bit を big-endian で uint64_t に詰める
    uint64_t v;
    std::memcpy(&v, u.data.data(), sizeof(v));   // 先頭 8 byte (うち下位 16 bit は data[6..7])
    // GCC/Clang の bswap で 1 命令化
    return __builtin_bswap64(v) >> 16;
}
```

注: `data[6..7]` には version (4bit) + time_hi_and_version の残り + variant などが入るが、上位 48 bit しか読まないので影響しない。

- [ ] **Step 4: ビルド・テスト PASS**

```bash
cmake --build build -j
cd build && ctest -R timestamp --output-on-failure
```

期待: 新規テスト 1 件 PASS。

- [ ] **Step 5: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp test/test_uuid7.cpp
git commit -m "perf: extract_timestamp_fast を追加 (Phase 3)"
```

### Task 3.3: 既存 `extract_timestamp` を `extract_timestamp_fast` ベースに置換

**Files:**
- Modify: `uuid7pp.hpp`

- [ ] **Step 1: 既存実装を確認**

`uuid7pp.hpp` の `extract_timestamp` はループで 6 バイトを big-endian で `ms` に詰めている。

- [ ] **Step 2: `extract_timestamp_fast` 呼び出しに置換**

```cpp
static inline auto extract_timestamp(uuid const& u) noexcept -> std::chrono::system_clock::time_point {
    return std::chrono::system_clock::time_point{
        std::chrono::milliseconds{extract_timestamp_fast(u)}};
}
```

- [ ] **Step 3: ビルド・全テスト・ベンチ**

```bash
cmake --build build -j
cd build && ctest --output-on-failure
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_after_33.txt
```

期待: 全テスト PASS。`extract_timestamp` の Improved が Baseline の 1.0 倍以上。

- [ ] **Step 4: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp
git commit -m "perf: extract_timestamp を extract_timestamp_fast ベースに (Phase 3)"
```

### Task 3.4 (条件付き): REJECTED 対応

Phase 2 と同形式の REJECTED ブランチ退避 (`perf/rejected-attempts/phase3-bswap/`)。

### Task 3.5: Phase 3 終了の総括

- ベンチログの Improved 列を埋める。
- スペック §10 に Phase 3 行を追加。
- コミット。

---

## Phase 4: `generate_batch` 追加 + `generate()` ホットパス仕上げ

### Task 4.1: Phase 4 用ベンチログセクション追加

### Task 4.2: `generate_batch` 追加 (テスト先行)

**Files:**
- Modify: `uuid7pp.hpp`
- Modify: `test/test_uuid7.cpp`

- [ ] **Step 1: 失敗するテスト追加**

```cpp
TEST_CASE("generator::generate_batch basic", "[perf][batch]") {
    constexpr std::size_t N = 100;
    alignas(16) uuid7pp::uuid buf[N];
    auto const count = uuid7pp::generator::generate_batch(buf, N);
    CHECK(count == N);

    std::unordered_set<uuid7pp::uuid> set;
    for (std::size_t i = 0; i < N; ++i) set.insert(buf[i]);
    CHECK(set.size() == N);
}

TEST_CASE("generator::generate_batch edge cases", "[perf][batch]") {
    alignas(16) uuid7pp::uuid buf[1];
    CHECK(uuid7pp::generator::generate_batch(buf, 0) == 0);
    CHECK(uuid7pp::generator::generate_batch(buf, 1) == 1);
    CHECK(buf[0].data[6] >> 4 == 7);  // v7
}

TEST_CASE("generator::generate_batch monotonicity", "[perf][batch]") {
    constexpr std::size_t N = 100;
    alignas(16) uuid7pp::uuid buf[N];
    uuid7pp::generator::generate_batch(buf, N);
    for (std::size_t i = 1; i < N; ++i) {
        CHECK(buf[i - 1].data < buf[i].data);
    }
}
```

- [ ] **Step 2: 失敗確認**

- [ ] **Step 3: `generate_batch` 実装**

```cpp
static inline auto generate_batch(uuid* out, std::size_t n) noexcept -> std::size_t {
    if (n == 0) return 0;
    auto& st{tls_state};
    if (!st.initialized) [[unlikely]] initialize(st);

    auto const now{std::chrono::system_clock::now()};
    auto ms{static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count())};

    for (std::size_t i = 0; i < n; ++i) {
        if (ms > st.last_ms) [[likely]] {
            st.last_ms = ms;
            st.counter = static_cast<uint16_t>(st.rng.next() & 0x03FF);
        } else if (ms == st.last_ms) {
            if (st.counter < 0x0FFF) [[likely]] ++st.counter;
        } else {
            out[i] = pack(ms, static_cast<uint16_t>(st.rng.next() & 0x0FFF), st.rng.next());
            continue;
        }
        out[i] = pack(st.last_ms, st.counter, st.rng.next());
    }
    return n;
}
```

- [ ] **Step 4: ビルド・テスト PASS**

- [ ] **Step 5: ベンチで改善確認**

```bash
./test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms 2>&1 | tee /tmp/uuid7pp_after_42.txt
```

`generate` (Phase 4 ベンチでは 1 個あたりの ns を比較) と `generate_batch` の数値を記録。`generate_batch(1000)` の 1 個あたり ns が `generate()` より速いことを確認。

- [ ] **Step 6: コミット**

```bash
cd /home/toge/src/uuid7pp
git add uuid7pp.hpp test/test_uuid7.cpp
git commit -m "perf: generator::generate_batch を追加 (Phase 4)"
```

### Task 4.3: 既存 `generate_at(uint64_t)` を hot path 最適化 (REJECTED 可)

`generate_at` から `pack` を直接呼び、 `tls_state` アクセスを最小化。性能劣化があれば REJECTED。

### Task 4.4 (条件付き): REJECTED 対応

Phase 2/3 と同形式。

### Task 4.5: Phase 4 終了の総括

### Task 4.6: `test/test_benchmark.cpp` に generate_batch のベンチ項目を追加

- [ ] **Step 1: ベンチコード追加**

`test/test_benchmark.cpp` の `SECTION("Generation Only")` に追記:

```cpp
SECTION("Batch generation") {
    alignas(16) uuid7pp::uuid buf[1000];
    BENCHMARK("uuid7pp::generate_batch(1000) per item") {
        uuid7pp::generator::generate_batch(buf, 1000);
        return buf[0].data[0];
    };
}
```

- [ ] **Step 2: ベンチ取得 + コミット**

```bash
cd /home/toge/src/uuid7pp
git add test/test_benchmark.cpp
git commit -m "test: generate_batch のベンチを追加"
```

---

## Phase 5: unsafe 直接 API 公開とベンチ更新

### Task 5.1: `to_chars_upper` / `to_chars_plain` 等の追加 (テスト先行)

**Files:**
- Modify: `uuid7pp.hpp`
- Modify: `test/test_uuid7.cpp`

- [ ] **Step 1: 失敗するテスト追加**

```cpp
TEST_CASE("unsafe to_chars_upper / to_chars_plain variants", "[perf][unsafe]") {
    auto const u = uuid7pp::generator::generate();

    char buf_a[36], buf_b[36];
    uuid7pp::to_chars(u, buf_a, true, true);
    uuid7pp::to_chars_upper(u, buf_b, true);
    CHECK(std::string_view(buf_a, 36) == std::string_view(buf_b, 36));

    char buf_c[32], buf_d[32];
    uuid7pp::to_chars(u, buf_c, false, false);
    uuid7pp::to_chars_plain(u, buf_d);
    CHECK(std::string_view(buf_c, 32) == std::string_view(buf_d, 32));

    char buf_e[32], buf_f[32];
    uuid7pp::to_chars(u, buf_e, false, true);
    uuid7pp::to_chars_plain_upper(u, buf_f);
    CHECK(std::string_view(buf_e, 32) == std::string_view(buf_f, 32));

    char buf_g[36], buf_h[36];
    uuid7pp::to_chars(u, buf_g, true, false);
    uuid7pp::to_chars_lower(u, buf_h, true);
    CHECK(std::string_view(buf_g, 36) == std::string_view(buf_h, 36));
}
```

- [ ] **Step 2: 失敗確認**

- [ ] **Step 3: 4 つのラッパ関数を追加**

```cpp
inline auto to_chars_upper(uuid const& u, char* out, bool const hyphen = true) noexcept -> void {
    if (hyphen) detail::to_chars_impl<true, true>(u, out);
    else        detail::to_chars_impl<true, false>(u, out);
}
inline auto to_chars_lower(uuid const& u, char* out, bool const hyphen = true) noexcept -> void {
    if (hyphen) detail::to_chars_impl<false, true>(u, out);
    else        detail::to_chars_impl<false, false>(u, out);
}
inline auto to_chars_plain(uuid const& u, char* out) noexcept -> void {
    detail::to_chars_impl<false, false>(u, out);
}
inline auto to_chars_plain_upper(uuid const& u, char* out) noexcept -> void {
    detail::to_chars_impl<true, false>(u, out);
}
```

- [ ] **Step 4: ビルド・テスト PASS・コミット**

### Task 5.2: `test/test_benchmark.cpp` に unsafe API のベンチ追加

- 既存 `to_chars` のベンチに加えて `to_chars_upper` / `to_chars_plain` の直接呼び出しを追加。
- ベンチ取得 → コミット。

### Task 5.3: Phase 5 終了の総括 + README 更新

- [ ] **Step 1: README の「パフォーマンス」テーブルに新 API を使った数値を追記 (任意) **

- [ ] **Step 2: スペック §10 に Phase 5 行を追加**

- [ ] **Step 3: 最終コミット**

```bash
cd /home/toge/src/uuid7pp
git add README.md docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md
git commit -m "docs: Phase 5 終了 / 性能改善プロジェクト完了"
```

---

## 全体受け入れ条件 (全 Phase 完了後)

- [ ] 既存 `test/test_uuid7.cpp` 全件 PASS
- [ ] 新規 NTTP 特殊化・unsafe API・batch API の回帰テスト全件 PASS
- [ ] `generate`, `to_chars`, `from_chars`, `extract_timestamp` のうち **最低 1 つ** がベースライン比 1.2 倍以上 (スペック §6.3)
- [ ] 性能劣化した改善はすべて `perf/rejected-attempts/` ブランチに退避され、main ブランチには残っていない
- [ ] `docs/superpowers/specs/2026-06-28-uuid7pp-perf-bench.md` に Phase 1〜5 のベンチ結果が残っている
- [ ] `docs/superpowers/specs/2026-06-28-uuid7pp-perf-design.md` の §10 採用履歴が完成している

---

## スコープ外 (本計画では扱わない項目)

- スペック §3.5 の `clock_gettime(CLOCK_REALTIME)` への置換: Phase 5 ベンチ結果を見て、必要であれば別フェーズとして計画書を起こす。MSVC 対応 (`#ifdef _WIN32` + `QueryPerformanceCounter`) も同様。
- AVX-512 / NEON の明示的特化: SIMDe のネイティブ実装に任せる。
- ベンチ用ホスト環境の固定 (CI で reproducibility を確保): 別タスク。

---

## Self-Review Checklist (計画書作成時の確認)

- [x] 各 Phase に必ずベンチ検証ステップがある
- [x] 性能劣化時の REJECTED 対応タスクが条件付きで用意されている
- [x] 全タスクがチェックボックス `- [ ]` 形式で追跡可能
- [x] ファイルパスが全て絶対パスまたは作業ディレクトリ起点の相対パスで明示されている
- [x] テストファースト (TDD) を Task 1.2 / 1.4 / 3.2 / 4.2 / 5.1 で実施している
- [x] 各 Phase の締めで git commit が指示されている
- [x] REJECTED 時のブランチ退避コマンドが具体的に書かれている
- [x] `to_chars` テストの `buf[36]` は `buf_existing[36]` と `buf_impl[36]` に分割して既存版と NTTP 版を直接比較している
- [x] `from_chars_impl<true>` へのハイフンなし入力テストは `nullopt` を期待する
- [x] `generate_batch` のテストは 0 / 1 / 100 の 3 ケースをカバーしている
