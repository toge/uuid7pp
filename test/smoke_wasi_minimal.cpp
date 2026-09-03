/**
 * @file test/smoke_wasi_minimal.cpp
 * @brief UUID7PP_WASI_MINIMAL モードの検証。
 *
 * -fno-exceptions 付きでビルドされる。WASI Minimal では OS 依存 API
 * (generate / generate_batch / generate_at(time_point) /
 * extract_timestamp(time_point)) が無効化されるが、to_string / formatter は
 * wasip1 hosted のため有効。seed() + generate_at(uint64_t) / to_chars /
 * from_chars / extract_timestamp_fast / to_string の構成を確認する。
 */
#include "uuid7pp.hpp"

#include <cassert>
#ifdef __cpp_lib_format
#include <format>
#endif
#include <string_view>

int main() {
  using namespace uuid7pp;

#ifndef UUID7PP_WASI_MINIMAL
  static_assert(false, "UUID7PP_WASI_MINIMAL must be defined for this smoke test");
#endif

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

  // to_string / formatter は wasip1 hosted のため WASI Minimal でも有効
  auto const s = to_string(u3);
  assert(s.size() == 36);
  assert(from_chars(s)->data == u3.data);
#ifdef __cpp_lib_format
  auto const fmt = std::format("{}", u3);
  assert(fmt.size() == 36);
#endif

  return 0;
}
