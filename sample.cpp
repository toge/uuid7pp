#include <iostream>
#include <format>
#include "uuid7pp.hpp"

// 注: generate()/generate_at() はカウンタ飽和時にも常に値を返します (飽和時はタイムスタンプを 1ms 進めます)。

auto main() -> int {
  auto const id{uuid7pp::generator::generate()};

  // std::format による出力 (C++20/23)
  std::cout << std::format("Default:    {}\n", id);
  std::cout << std::format("Uppercase:  {:X}\n", id);
  std::cout << std::format("No-hyphen:  {:n}\n", id);
  std::cout << std::format("No-hyphen-U:{:N}\n", id);

  return 0;
}
