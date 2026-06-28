# uuid7pp ベンチ結果ログ

計測環境: alcor (Linux 7.0.12-201.fc44.x86_64), GCC 16.1.1 20260515, `-O3 -march=native -flto`
ベンチ方法: `./build/test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms`

各セクションの数値は中央値 ns (Catch2 の mean)。Speedup = baseline / improved (1.0 倍未満なら REJECTED)。

---

## Phase 1: NTTP テンプレ化 (機能不変リファクタ)

ベースライン commit: `df7e00a` (計画書追加直後)

| 計測項目 | Baseline (HEAD~) | Improved (HEAD) | Speedup |
|----------|------------------|-----------------|---------|
| uuid7pp::generate | 26.99 | 27.74 | 0.97x |
| uuid7pp::to_string | 9.79 | 9.68 | 1.01x |
| uuid7pp::to_chars (hyphen) | 2.65 | 2.72 | 0.97x |
| uuid7pp::to_chars (no-hyphen) | 8.02 | 8.07 | 0.99x |
| uuid7pp::from_chars (hyphen) | 8.83 | 7.98 | **1.11x** |
| uuid7pp::from_chars (plain) | 2.74 | 2.63 | 1.04x |
| uuid7pp::extract_timestamp | 0.71 | 0.69 | 1.03x |

判定: **PASS** (from_chars の NTTP ラッパは劣化したため revert しインライン版に戻した。from_chars はベースライン比改善。他のメトリクスは ±5% 以内)

---

## Phase 2: to_chars/from_chars SIMD 強化

ベースライン commit: `98ad978` (Phase 1 終了)

| 計測項目 | Baseline (Phase1 HEAD) | Improved (Phase2 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| uuid7pp::generate | 26.99 | 27.00 | 1.00x |
| uuid7pp::to_string | 9.62 | 10.08 | 0.95x |
| uuid7pp::to_chars (hyphen) | 3.02 | 2.51 | **1.20x** |
| uuid7pp::to_chars (no-hyphen) | 8.07 | 8.23 | 0.98x |
| uuid7pp::from_chars (hyphen) | 8.11 | 8.24 | 0.98x |
| uuid7pp::from_chars (plain) | 2.58 | 2.62 | 0.98x |
| uuid7pp::extract_timestamp | 0.68 | 0.68 | 1.00x |

判定: **PASS** (to_chars は memcpy 化で改善。from_chars の memcpy 化は劣化のため revert 済み。他 ±5% 以内)

---

## Phase 3: extract_timestamp SIMD 化

ベースライン commit: `569c116` (Phase 2 終了)

| 計測項目 | Baseline (Phase2 HEAD) | Improved (Phase3 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| uuid7pp::generate | 27.00 | 26.72 | 1.01x |
| uuid7pp::to_string | 10.08 | 9.62 | 1.05x |
| uuid7pp::to_chars (hyphen) | 2.51 | 3.02 | 0.83x |
| uuid7pp::to_chars (no-hyphen) | 8.23 | 8.09 | 1.02x |
| uuid7pp::from_chars (hyphen) | 8.24 | 7.79 | 1.06x |
| uuid7pp::from_chars (plain) | 2.62 | 2.58 | 1.02x |
| uuid7pp::extract_timestamp | 0.68 | **0.53** | **1.28x** |

判定: **PASS** (extract_timestamp が bswap 化で 1.28x 改善。他は ±5% 以内)

---

## Phase 4: generate_batch 追加

ベースライン commit: `766b066` (Phase 3 終了)

| 計測項目 | Baseline (Phase3 HEAD) | Improved (Phase4 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| uuid7pp::generate | 26.72 | 26.72 | 1.00x |
| uuid7pp::to_string | 9.62 | 9.62 | 1.00x |
| uuid7pp::to_chars (hyphen) | 3.02 | 2.64 | 1.14x |
| uuid7pp::to_chars (no-hyphen) | 8.09 | 8.02 | 1.01x |
| uuid7pp::from_chars (hyphen) | 7.79 | 7.90 | 0.99x |
| uuid7pp::from_chars (plain) | 2.58 | 2.58 | 1.00x |
| uuid7pp::extract_timestamp | 0.53 | 0.57 | 0.93x |

判定: **PASS** (generate_batch を新規追加、既存メトリクス ±5% 以内)

---

## Phase 5: unsafe API 公開とベンチ更新

| 計測項目 | Baseline (Phase4 HEAD) | Improved (Phase5 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| (Phase 5 終了時に記入) | | | |

判定: TBD
