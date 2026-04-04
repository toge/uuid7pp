#include <set>
#include <unordered_set>
#include <regex>
#include <format>

#include "catch2/catch_all.hpp"

#include "uuid7pp.hpp"

TEST_CASE("UUID v7 Generation", "[uuid]") {
    SECTION("Basic generation") {
        auto const u1 = uuid7pp::generator::generate();
        auto const u2 = uuid7pp::generator::generate();
        CHECK(u1.data != u2.data);
    }

    SECTION("Monotonicity") {
        auto const u1 = uuid7pp::generator::generate();
        auto const u2 = uuid7pp::generator::generate();
        CHECK(u1.data < u2.data);
    }

    SECTION("Generate at specific time") {
        uint64_t const ts = 1704067200000;
        auto const u = uuid7pp::generator::generate_at(ts);
        
        uint64_t extracted_ts = 0;
        for (int i = 0; i < 6; ++i) {
            extracted_ts = (extracted_ts << 8) | u.data[i];
        }
        CHECK(extracted_ts == ts);

        auto const u2 = uuid7pp::generator::generate_at(ts);
        CHECK(u.data < u2.data);
    }
}

TEST_CASE("UUID Conversion and Formatting", "[uuid]") {
    auto const u = uuid7pp::generator::generate();

    SECTION("Default to_string") {
        auto const s = uuid7pp::to_string(u);
        CHECK(s.length() == 36);
        CHECK(s[8] == '-');
    }

    SECTION("No hyphen") {
        auto const s = uuid7pp::to_string(u, false);
        CHECK(s.length() == 32);
        CHECK(s.find('-') == std::string::npos);
        
        auto const p = uuid7pp::from_chars(s);
        REQUIRE(p.has_value());
        CHECK(*p == u);
    }

    SECTION("Uppercase") {
        auto const s = uuid7pp::to_string(u, true, true);
        std::regex const re("^[0-9A-F]{8}-[0-9A-F]{4}-7[0-9A-F]{3}-[89AB][0-9A-F]{3}-[0-9A-F]{12}$");
        CHECK(std::regex_match(s, re));
        
        auto const p = uuid7pp::from_chars(s);
        REQUIRE(p.has_value());
        CHECK(*p == u);
    }

    SECTION("std::format integration") {
        CHECK(std::format("{}", u) == uuid7pp::to_string(u));
        CHECK(std::format("{:X}", u) == uuid7pp::to_string(u, true, true));
        CHECK(std::format("{:n}", u) == uuid7pp::to_string(u, false, false));
        CHECK(std::format("{:N}", u) == uuid7pp::to_string(u, false, true));
    }
}

TEST_CASE("UUID Container Integration", "[uuid]") {
    SECTION("std::unordered_set") {
        std::unordered_set<uuid7pp::uuid> set;
        auto const u1 = uuid7pp::generator::generate();
        auto const u2 = uuid7pp::generator::generate();
        
        set.insert(u1);
        set.insert(u2);
        
        CHECK(set.size() == 2);
        CHECK(set.contains(u1));
    }
}
