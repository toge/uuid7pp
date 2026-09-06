/**
 * @file test/smoke_no_exceptions.cpp
 * @brief 例外なしの検証。
 *
 * -fno-exceptions 付きでビルドされる（Catch2 は例外を要するため素の main を使用）。
 * 全モジュールが例外なしでコンパイル・評価・実行できることを確認する。
 */
#include "uuid7pp.hpp"

#include <cassert>
#ifdef __cpp_lib_format
#include <format>
#endif
#include <string_view>

int main() {
  using namespace uuid7pp;

  generator::seed(0x0123'4567'89AB'CDEFull);
  auto const u1 = generator::generate_at(1'700'000'000'000ull);
  assert(is_v7(u1));
  assert(get_version(u1) == 7);
  assert(extract_timestamp_fast(u1) == 1'700'000'000'000ull);

  auto const u2 = generator::generate_at(1'700'000'000'000ull);
  assert(u1 != u2);
  assert(u1 < u2);

  generator::reset();
  generator::seed(42);
  auto const u3 = generator::generate_at(1'700'000'000'001ull);
  assert(is_v7(u3));

  char buf[36];
  to_chars(u3, buf);
  auto const parsed = from_chars(std::string_view{buf, 36});
  assert(parsed.has_value());
  assert(*parsed == u3);

  char buf32[32];
  to_chars_plain(u3, buf32);
  auto const parsed32 = from_chars(std::string_view{buf32, 32});
  assert(parsed32.has_value());
  assert(*parsed32 == u3);

  auto const s = to_string(u3);
  assert(s.size() == 36);
  assert(from_chars(s)->data == u3.data);

#ifdef __cpp_lib_format
  auto const fmt = std::format("{}", u3);
  assert(fmt.size() == 36);
#endif

#if !defined(UUID7PP_WASIP1)
  // generate_batch nullptr 安全性（例外を出さず 0 を返す）
  assert(generator::generate_batch(nullptr, 10) == 0);
#endif

  return 0;
}
