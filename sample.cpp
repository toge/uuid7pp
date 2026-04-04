#include <iostream>
#include <format>
#include "uuidv7pp.hpp"

auto main() -> int {
  try {
    auto const id{uuid7pp::generator::generate()};
    
    // std::format による出力 (C++20/23)
    std::cout << std::format("Default:    {}\n", id);
    std::cout << std::format("Uppercase:  {:X}\n", id);
    std::cout << std::format("No-hyphen:  {:n}\n", id);
    std::cout << std::format("No-hyphen-U:{:N}\n", id);
    
  } catch (std::exception const& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
  return 0;
}
