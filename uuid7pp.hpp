#ifndef UUID7PP_HPP__
#define UUID7PP_HPP__

#include <array>
#include <bit>
#include <chrono>
#include <compare>
#include <random>
#include <string>
#include <string_view>
#include <cstring>
#include <optional>

#ifdef __cpp_lib_format
#include <format>
#endif

#include <simde/x86/ssse3.h>
#include <simde/x86/sse4.1.h>

/**
 * @namespace uuid7pp
 * @brief 高速なUUID v7生成・変換ライブラリ
 *
 * SIMD (SSSE3/SSE4.1) を活用し、RFC 9562 準拠の UUID v7 を高速に生成・変換します。
 */
namespace uuid7pp {

/**
 * @struct xoshiro256
 * @brief xoshiro256++ 疑似乱数生成器
 */
struct xoshiro256 {
  uint64_t s[4];

  /**
   * @brief 次の64ビット乱数を生成する
   * @return 生成された64ビットの乱数値
   */
  auto next() noexcept -> uint64_t {
    auto const result{std::rotl(s[0] + s[3], 23) + s[0]};
    auto const t{s[1] << 17};
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = std::rotl(s[3], 45);
    return result;
  }
};

/**
 * @struct uuid
 * @brief 128ビットのUUIDを表す構造体
 *
 * SIMD演算のために16バイト境界でアライメント済み
 */
struct alignas(16) [[nodiscard]] uuid {
  /** UUIDの生データ (16バイト) */
  std::array<uint8_t, 16> data;

  /**
   * @brief 等価比較演算子
   * @param other 比較対象のUUID
   * @return 一致する場合true (SIMDにより128ビットを一気に比較)
   */
  auto operator==(uuid const& other) const noexcept -> bool {
    auto const v1{simde_mm_load_si128(reinterpret_cast<simde__m128i const*>(data.data()))};
    auto const v2{simde_mm_load_si128(reinterpret_cast<simde__m128i const*>(other.data.data()))};
    return simde_mm_movemask_epi8(simde_mm_cmpeq_epi8(v1, v2)) == 0xFFFF;
  }

  /**
   * @brief 三方比較演算子 (C++20)
   * @param other 比較対象のUUID
   * @return 比較結果 (std::strong_ordering)
   * @note UUID v7 は最上位バイトから順にタイムスタンプ、バージョン、カウンタ、エントロピーが格納されているため、
   *       辞書順比較はそのまま時系列順の比較となります。
   */
  auto operator<=>(uuid const& other) const noexcept -> std::strong_ordering {
    return data <=> other.data;
  }
};

namespace detail {

/**
 * @struct state
 * @brief スレッドごとの生成器の状態
 */
struct state {
  xoshiro256 rng;    /**< 乱数生成器の状態 */
  uint64_t last_ms{0}; /**< 最後にUUIDを生成したミリ秒タイムスタンプ */
  uint16_t counter{0}; /**< 同一ミリ秒内での単調増加カウンタ */
  bool initialized{false}; /**< 初期化済みフラグ */
};

/**
 * @brief SIMD用：16進数文字(ASCII)を数値(0-15)に変換する
 * @param v 16個のASCII文字を含む128ビットレジスタ
 * @return 16個の数値(ニブル)を含むレジスタ。無効な文字は15より大きい値になる。
 */
static inline auto hex_to_nibble_simd(simde__m128i v) noexcept -> simde__m128i {
  auto const mask_num = simde_mm_and_si128(simde_mm_cmplt_epi8(v, simde_mm_set1_epi8('9' + 1)),
                                         simde_mm_cmpgt_epi8(v, simde_mm_set1_epi8('0' - 1)));
  auto const v_num = simde_mm_sub_epi8(v, simde_mm_set1_epi8('0'));

  auto const v_lower = simde_mm_or_si128(v, simde_mm_set1_epi8(0x20));
  auto const v_alpha = simde_mm_sub_epi8(v_lower, simde_mm_set1_epi8('a' - 10));

  return simde_mm_blendv_epi8(v_alpha, v_num, mask_num);
}

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
    std::memcpy(out, tmp, 8);
    out[8] = '-';
    std::memcpy(out + 9, tmp + 8, 4);
    out[13] = '-';
    std::memcpy(out + 14, tmp + 12, 4);
    out[18] = '-';
    std::memcpy(out + 19, tmp + 16, 4);
    out[23] = '-';
    std::memcpy(out + 24, tmp + 20, 12);
  } else {
    std::memcpy(out, tmp, 32);
  }
}
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
} // namespace detail

/**
 * @class generator
 * @brief UUID v7生成器
 *
 * スレッドごとに独立した状態を持ち、ロックフリーかつスレッドセーフに動作します。
 * RFC 9562 準拠の UUID v7 を極めて高速に生成します。
 */
class generator {
private:
  /** スレッドローカルな状態変数 */
  static inline thread_local detail::state tls_state;

  /**
   * @brief 乱数生成器の初期化 (std::random_deviceを使用)
   * @param st 初期化対象の状態
   */
  static auto initialize(detail::state& st) noexcept -> void {
    auto rd{std::random_device{}};
    for (auto& val : st.rng.s) {
      val = (static_cast<uint64_t>(rd()) << 32) | rd();
    }
    st.initialized = true;
  }

  /**
   * @brief ミリ秒、カウンタ、エントロピーからUUIDをパッキングする内部関数
   * @param ms 48ビットのミリ秒タイムスタンプ
   * @param counter 12ビットのカウンタ
   * @param entropy 62ビットの乱数エントロピー
   * @return 構築されたUUID
   */
  static inline auto pack(uint64_t const ms, uint16_t const counter, uint64_t const entropy) noexcept -> uuid {
    auto const high_final{((ms & 0xFFFF'FFFF'FFFFu) << 16) | (0x7000u | (counter & 0x0FFFu))};
    auto const low_final{(entropy & 0x3FFF'FFFF'FFFF'FFFFu) | 0x8000'0000'0000'0000u};

    uuid res;
    auto const v{simde_mm_set_epi64x(static_cast<int64_t>(low_final), static_cast<int64_t>(high_final))};
    auto const mask{simde_mm_setr_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8)};
    simde_mm_store_si128(reinterpret_cast<simde__m128i*>(res.data.data()), simde_mm_shuffle_epi8(v, mask));
    return res;
  }

public:
  /**
   * @brief 現在時刻でUUID v7を生成する
   * @return 生成されたUUID
   */
  static inline auto generate() noexcept -> uuid {
    auto const now{std::chrono::system_clock::now()};
    auto const ms{static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count())};
    return generate_at(ms);
  }

  /**
   * @brief 指定した time_point でUUID v7を生成する
   * @param tp 生成時刻
   * @return 生成されたUUID
   */
  static inline auto generate_at(std::chrono::system_clock::time_point const tp) noexcept -> uuid {
    auto const ms{static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count())};
    return generate_at(ms);
  }

  /**
   * @brief 指定したミリ秒タイムスタンプでUUID v7を生成する
   *
   * 同一時刻の呼び出しに対しては、スレッドローカルなカウンタをインクリメントして単調増加性を維持します。
   * @param ms UNIXタイムスタンプ (ミリ秒)
   * @return 生成されたUUID
   */
  static inline auto generate_at(uint64_t const ms) noexcept -> uuid {
    auto& st{tls_state};
    if (!st.initialized) [[unlikely]] {
      initialize(st);
    }

    if (ms > st.last_ms) [[likely]] {
      st.last_ms = ms;
      st.counter = static_cast<uint16_t>(st.rng.next() & 0x03FF);
    } else if (ms == st.last_ms) {
      if (st.counter < 0x0FFF) [[likely]] {
        st.counter++;
      }
    } else {
      // 過去の時刻の場合は内部状態を更新せず独立して生成
      return pack(ms, static_cast<uint16_t>(st.rng.next() & 0x0FFF), st.rng.next());
    }

    return pack(st.last_ms, st.counter, st.rng.next());
  }

  static inline auto generate_batch(uuid* out, std::size_t n) noexcept -> std::size_t {
    if (n == 0) return 0;
    auto& st{tls_state};
    if (!st.initialized) [[unlikely]] initialize(st);

    auto const now{std::chrono::system_clock::now()};
    auto ms{static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count())};

    if (ms > st.last_ms) [[likely]] {
      st.last_ms = ms;
      st.counter = static_cast<uint16_t>(st.rng.next() & 0x03FF);
    } else if (ms < st.last_ms) {
      st.last_ms = ms;
      st.counter = static_cast<uint16_t>(st.rng.next() & 0x03FF);
    }

    for (std::size_t i = 0; i < n; ++i) {
      out[i] = pack(st.last_ms, st.counter, st.rng.next());
      if (st.counter < 0x0FFF) [[likely]] ++st.counter;
    }
    return n;
  }
};

/**
 * @brief UUIDをバッファに直接書き込む (メモリ割当なしの超高速版)
 *
 * 36文字(ハイフンあり)または32文字(ハイフンなし)を書き込み。ヌル終端は行わない。
 *
 * @param u 変換対象のUUID
 * @param out 書き込み先のバッファ (最低36バイトの空きが必要)
 * @param hyphen ハイフン ('-') を挿入するかどうか。デフォルトはtrue。
 * @param upper 16進数文字を大文字にするかどうか。デフォルトはfalse。
 */
static inline auto to_chars(uuid const& u, char* out, bool const hyphen = true, bool const upper = false) noexcept -> void {
  if (upper) {
    if (hyphen) detail::to_chars_impl<true, true>(u, out);
    else        detail::to_chars_impl<true, false>(u, out);
  } else {
    if (hyphen) detail::to_chars_impl<false, true>(u, out);
    else        detail::to_chars_impl<false, false>(u, out);
  }
}

/**
 * @brief 文字列からUUIDをパースする (SIMD高速化版)
 *
 * 36文字(ハイフンあり)および32文字(ハイフンなし)の両形式に対応。
 * 大文字・小文字を区別しない。
 *
 * @param s パース対象の文字列
 * @return パース成功時はUUID、失敗時(形式不正や無効な文字)はstd::nullopt
 */
static inline auto from_chars(std::string_view s) noexcept -> std::optional<uuid> {
  alignas(16) char clean[32];
  if (s.length() == 36) {
    if (s[8] != '-' || s[13] != '-' || s[18] != '-' || s[23] != '-') [[unlikely]] return std::nullopt;
    auto const copy_hex = [&](int src_off, int dst_off, int len) {
      for(int i=0; i<len; ++i) clean[dst_off + i] = s[src_off + i];
    };
    copy_hex(0, 0, 8);
    copy_hex(9, 8, 4);
    copy_hex(14, 12, 4);
    copy_hex(19, 16, 4);
    copy_hex(24, 20, 12);
  } else if (s.length() == 32) {
    std::copy_n(s.data(), 32, clean);
  } else {
    return std::nullopt;
  }

  auto const v1{simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(clean))};
  auto const v2{simde_mm_loadu_si128(reinterpret_cast<simde__m128i const*>(clean + 16))};
  auto const n1{detail::hex_to_nibble_simd(v1)};
  auto const n2{detail::hex_to_nibble_simd(v2)};
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

/**
 * @brief UUIDのバージョンを取得する
 * @param u 対象のUUID
 * @return バージョン番号 (0-15)
 * @note RFC 9562 Section 4.2: バージョンは `time_hi_and_version` フィールド (data[6]) の上位4ビットに格納される
 */
[[nodiscard]]
constexpr auto get_version(uuid const& u) noexcept -> uint8_t {
  return static_cast<uint8_t>(u.data[6] >> 4);
}

/**
 * @brief UUIDがバージョン7かつRFC 4122/9562バリアントであるかを確認する
 * @param u 対象のUUID
 * @return UUID v7であればtrue
 * @note RFC 9562 Section 4.1 & 4.2:
 *       - Version: 7 (0b0111)
 *       - Variant: 0b10 (RFC 4122) は `clock_seq_hi_and_reserved` (data[8]) の上位2ビットに格納される
 */
[[nodiscard]]
constexpr auto is_v7(uuid const& u) noexcept -> bool {
  return (get_version(u) == 7) && ((u.data[8] & 0xc0) == 0x80);
}

/**
 * @brief UUIDからミリ秒タイムスタンプを高速に抽出する (bare uint64_t)
 * @param u 抽出対象のUUID
 * @return ミリ秒単位のUnixタイムスタンプ (上位48bit)
 */
[[nodiscard]]
inline auto extract_timestamp_fast(uuid const& u) noexcept -> uint64_t {
    uint64_t v;
    std::memcpy(&v, u.data.data(), sizeof(v));
    return __builtin_bswap64(v) >> 16;
}

/**
 * @brief UUIDからミリ秒タイムスタンプを復元する
 * @param u 復元対象のUUID
 * @return 復元されたタイムスタンプ (std::chrono::system_clock::time_point)
 */
[[nodiscard]]
static inline auto extract_timestamp(uuid const& u) noexcept -> std::chrono::system_clock::time_point {
  return std::chrono::system_clock::time_point{
    std::chrono::milliseconds{extract_timestamp_fast(u)}};
}

/**
 * @brief 利便性のためのstd::string変換関数
 * @param u 変換対象のUUID
 * @param hyphen ハイフンを入れるかどうか。デフォルトはtrue
 * @param upper 大文字にするかどうか。デフォルトはfalse
 * @return 36文字または32文字のUUID文字列
 */
inline auto to_string(uuid const& u, bool const hyphen = true, bool const upper = false) -> std::string {
  std::string s(hyphen ? 36 : 32, '\0');
  to_chars(u, s.data(), hyphen, upper);
  return s;
}

} // namespace uuid7pp

/**
 * @brief std::hash の uuid7pp::uuid に対する特殊化
 *
 * 128ビットのデータを MurmurHash3 スタイルの手法でマージし、高速なハッシュ値を生成する。
 */
template <>
struct std::hash<uuid7pp::uuid> {
  /**
   * @brief ハッシュ値を計算する
   * @param u ハッシュ対象のUUID
   * @return 計算されたハッシュ値
   */
  auto operator()(uuid7pp::uuid const& u) const noexcept -> std::size_t {
    auto const* p = reinterpret_cast<uint64_t const*>(u.data.data());
    auto h1 = p[0];
    auto h2 = p[1];
    h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
    return static_cast<std::size_t>(h1);
  }
};

#ifdef __cpp_lib_format

/**
 * @brief std::formatter の uuid7pp::uuid に対する特殊化 (C++20/23対応)
 *
 * 使用可能な書式指定子:
 * - `{:x}` : 小文字、ハイフンあり (デフォルト)
 * - `{:X}` : 大文字、ハイフンあり
 * - `{:n}` : 小文字、ハイフンなし
 * - `{:N}` : 大文字、ハイフンなし
 *
 * 例: `std::print("{:X}", id);`
 */
template <>
struct std::formatter<uuid7pp::uuid> {
  /** 選択されたフォーマット形式 */
  char format_type = 'x';

  /**
   * @brief フォーマット文字列をパースする
   */
  constexpr auto parse(std::format_parse_context& ctx) {
    auto it = ctx.begin();
    if (it != ctx.end() && (*it == 'x' || *it == 'X' || *it == 'n' || *it == 'N')) {
      format_type = *it++;
    }
    return it;
  }

  /**
   * @brief UUIDを指定された書式で出力する
   */
  auto format(uuid7pp::uuid const& u, std::format_context& ctx) const {
    char buf[36];
    bool const hyphen = (format_type == 'x' || format_type == 'X');
    bool const upper  = (format_type == 'X' || format_type == 'N');
    uuid7pp::to_chars(u, buf, hyphen, upper);
    return std::format_to(ctx.out(), "{}", std::string_view(buf, hyphen ? 36 : 32));
  }
};

#endif // __cpp_lib_format

#endif // UUID7PP_HPP__
