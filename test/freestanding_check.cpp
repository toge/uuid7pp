// freestanding モード検証。Catch2 を使わず -ffreestanding -fno-exceptions -fno-rtti
// -nostdlib++ (libstdc++ リンクなし) でビルド・実行できることを確認する。
// OS 依存の generate() / generate_batch() / generate_at(time_point) /
// extract_timestamp(time_point) が無効化され、seed() + generate_at(uint64_t) の
// みの構成で完結することを確認する。動的確保に逆戻りしたら
// コンパイルエラーまたはリンクエラー (operator new 等) で失敗する。

// 依存の simde は未使用でも <cmath> を取り込むため、GCC 16 のように <cmath> を
// hosted 専用とする実装では -ffreestanding で失敗する。HUGE_VAL を先に定義して
// simde の「<cmath> 済み」検出ブランチへ誘導し、<cmath> の取り込み自体を回避する。
// hosted 環境 (wasm 含む) では本物の <cmath> を使わせるため無効化する。
// SIMDE_NO_NATIVE でネイティブ x86 ヘッダ (mm_malloc.h の malloc 参照) の取り込みも抑止する。
#if defined(__STDC_HOSTED__) && !__STDC_HOSTED__
#define HUGE_VAL (__builtin_huge_val())
#endif
#define SIMDE_NO_NATIVE 1

#include <cstdio>

#include "uuid7pp.hpp"

// wasm32-unknown-unknown ではヘッダ側で自動有効化される。それ以外は明示が必要。
#ifndef UUID7PP_FREESTANDING
#error "UUID7PP_FREESTANDING is not defined (build with -DENABLE_FREESTANDING=ON)"
#endif

static int failed = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failed;                                                   \
        }                                                               \
    } while (0)

int main() {
    using namespace uuid7pp;

    // 1. 明示シード + generate_at(ms) での生成 (FREESTANDING で必須の手順)
    generator::seed(0x0123'4567'89AB'CDEFull);
    auto const u1 = generator::generate_at(1'700'000'000'000ull);
    CHECK(is_v7(u1));
    CHECK(get_version(u1) == 7);
    CHECK(extract_timestamp_fast(u1) == 1'700'000'000'000ull);

    // 2. 同一 ms での単調増加 (カウンタ) と辞書順比較
    auto const u2 = generator::generate_at(1'700'000'000'000ull);
    CHECK(u1 != u2);
    CHECK(u1 < u2);

    // 3. reset() 後も seed() + generate_at が動く
    generator::reset();
    generator::seed(42);
    auto const u3 = generator::generate_at(1'700'000'000'001ull);
    CHECK(is_v7(u3));
    CHECK(extract_timestamp_fast(u3) == 1'700'000'000'001ull);

    // 4. to_chars / from_chars ラウンドトリップ (動的確保なし)
    char buf[36];
    to_chars(u3, buf);
    auto const parsed = from_chars(std::string_view{buf, 36});
    CHECK(parsed.has_value());
    CHECK(*parsed == u3);
    // ハイフンなし 32 文字形式
    char buf32[32];
    to_chars_plain(u3, buf32);
    auto const parsed32 = from_chars(std::string_view{buf32, 32});
    CHECK(parsed32.has_value());
    CHECK(*parsed32 == u3);

    if (failed == 0) std::printf("freestanding_check: all ok\n");
    return failed == 0 ? 0 : 1;
}
