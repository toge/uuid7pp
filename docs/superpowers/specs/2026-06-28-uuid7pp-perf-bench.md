# uuid7pp ベンチ結果ログ

計測環境: alcor (Linux 7.0.12-201.fc44.x86_64), GCC 16.1.1 20260515, `-O3 -march=native -flto`
ベンチ方法: `./build/test/all_test "[benchmark]" --benchmark-samples=10 --benchmark-warmup-time=2ms`

各セクションの数値は中央値 ns (Catch2 の mean)。Speedup = baseline / improved (1.0 倍未満なら REJECTED)。

---

## Phase 1: NTTP テンプレ化 (機能不変リファクタ)

ベースライン commit: `df7e00a` (計画書追加直後)

| 計測項目 | Baseline (HEAD~) | Improved (HEAD) | Speedup |
|----------|------------------|-----------------|---------|
| uuid7pp::generate | 26.99 | TBD | TBD |
| uuid7pp::to_string | 9.79 | TBD | TBD |
| uuid7pp::to_chars (hyphen) | 2.65 | TBD | TBD |
| uuid7pp::to_chars (no-hyphen) | 8.02 | TBD | TBD |
| uuid7pp::from_chars (hyphen) | 8.83 | TBD | TBD |
| uuid7pp::from_chars (plain) | 2.74 | TBD | TBD |
| uuid7pp::extract_timestamp | 0.71 | TBD | TBD |

判定: TBD

---

## Phase 2: to_chars/from_chars SIMD 強化

| 計測項目 | Baseline (Phase1 HEAD) | Improved (Phase2 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| (Phase 2 終了時に記入) | | | |

判定: TBD

---

## Phase 3: extract_timestamp SIMD 化

| 計測項目 | Baseline (Phase2 HEAD) | Improved (Phase3 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| (Phase 3 終了時に記入) | | | |

判定: TBD

---

## Phase 4: generate_batch と generate() ホットパス仕上げ

| 計測項目 | Baseline (Phase3 HEAD) | Improved (Phase4 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| (Phase 4 終了時に記入) | | | |

判定: TBD

---

## Phase 5: unsafe API 公開とベンチ更新

| 計測項目 | Baseline (Phase4 HEAD) | Improved (Phase5 HEAD) | Speedup |
|----------|------------------------|------------------------|---------|
| (Phase 5 終了時に記入) | | | |

判定: TBD
