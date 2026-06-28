#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <random>
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
        uint64_t const ts = 2000000000000ULL; // 2033-05-18
        auto const u = uuid7pp::generator::generate_at(ts);
        
        uint64_t extracted_ts = 0;
        for (int i = 0; i < 6; ++i) {
            extracted_ts = (extracted_ts << 8) | u.data[i];
        }
        CHECK(extracted_ts == ts);

        auto const u2 = uuid7pp::generator::generate_at(ts);
        CHECK(u.data < u2.data);
    }

    SECTION("Extract timestamp") {
        auto const now{std::chrono::system_clock::now()};
        auto const u{uuid7pp::generator::generate()};
        auto const extracted{uuid7pp::extract_timestamp(u)};
        
        // 1. generate() で生成した UUID から extract_timestamp() した結果が、
        //    生成時刻と 2ms 以内の誤差に収まること
        auto const diff{std::chrono::abs(std::chrono::duration_cast<std::chrono::milliseconds>(extracted - now))};
        CHECK(diff.count() <= 2);

        // 2. generate_at(tp) で特定時刻を指定した UUID から復元した時刻が tp と完全一致すること
        // (ミリ秒精度での比較)
        auto const tp{std::chrono::system_clock::time_point{std::chrono::milliseconds{1234567890123}}};
        auto const u_at{uuid7pp::generator::generate_at(tp)};
        CHECK(uuid7pp::extract_timestamp(u_at) == tp);

        // 3. from_chars でパースした既知の UUID 文字列からタイムスタンプが正しく復元されること
        // RFC 9562 example: 017f22e2-79b0-7cc3-98c4-dc0c0c07398f
        // unix_ts_ms: 0x017F22E279B0 (1645557742000)
        auto const s_known{"017f22e2-79b0-7cc3-98c4-dc0c0c07398f"};
        auto const u_known{uuid7pp::from_chars(s_known)};
        REQUIRE(u_known.has_value());
        auto const extracted_known{uuid7pp::extract_timestamp(*u_known)};
        auto const ms_known{std::chrono::duration_cast<std::chrono::milliseconds>(extracted_known.time_since_epoch()).count()};
        CHECK(ms_known == 1645557742000ULL);
    }

    SECTION("Version and Variant") {
        auto const u = uuid7pp::generator::generate();
        CHECK(uuid7pp::get_version(u) == 7);
        CHECK(uuid7pp::is_v7(u) == true);

        // v4 UUID: 550e8400-e29b-41d4-a716-446655440000
        auto const u4 = uuid7pp::from_chars("550e8400-e29b-41d4-a716-446655440000");
        REQUIRE(u4.has_value());
        CHECK(uuid7pp::get_version(*u4) == 4);
        CHECK(uuid7pp::is_v7(*u4) == false);

        // Nil UUID
        uuid7pp::uuid const nil_u{.data = {0}};
        CHECK(uuid7pp::get_version(nil_u) == 0);
        CHECK(uuid7pp::is_v7(nil_u) == false);

        // constexpr evaluation
        constexpr uuid7pp::uuid const c_u{.data = {0x01,0x02,0x03,0x04,0x05,0x06,0x71,0x08,0x89,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10}};
        static_assert(uuid7pp::get_version(c_u) == 7);
        static_assert(uuid7pp::is_v7(c_u) == true);
        
        constexpr uuid7pp::uuid const c_nil{.data = {0}};
        static_assert(uuid7pp::get_version(c_nil) == 0);
        static_assert(uuid7pp::is_v7(c_nil) == false);
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

TEST_CASE("UUID Comparison and Ordering", "[uuid]") {
    SECTION("Three-way comparison and strong ordering") {
        auto const u1 = uuid7pp::from_chars("0185966b-4e6a-7000-8000-000000000000").value();
        auto const u2 = uuid7pp::from_chars("0185966b-4e6a-7000-8000-000000000001").value();
        auto const u1_again = u1;

        CHECK((u1 <=> u2) == std::strong_ordering::less);
        CHECK((u2 <=> u1) == std::strong_ordering::greater);
        CHECK((u1 <=> u1_again) == std::strong_ordering::equal);
        
        // Relational operators derived from <=>
        CHECK(u1 < u2);
        CHECK(u1 <= u2);
        CHECK(u2 > u1);
        CHECK(u2 >= u1);
        CHECK(!(u1 > u2));
        CHECK(u1 <= u1_again);
        CHECK(u1 >= u1_again);
    }

    SECTION("Time-sequential sorting") {
        std::vector<uuid7pp::uuid> uuids;
        // Generate UUIDs with explicit increasing timestamps
        uuids.push_back(uuid7pp::generator::generate_at(1000));
        uuids.push_back(uuid7pp::generator::generate_at(2000));
        uuids.push_back(uuid7pp::generator::generate_at(3000));

        auto original = uuids;
        std::shuffle(uuids.begin(), uuids.end(), std::mt19937{std::random_device{}()});
        std::sort(uuids.begin(), uuids.end());

        CHECK(uuids == original);
    }

    SECTION("std::map as key") {
        std::map<uuid7pp::uuid, int> m;
        auto const u1 = uuid7pp::generator::generate_at(1000);
        auto const u2 = uuid7pp::generator::generate_at(2000);
        
        m[u1] = 1;
        m[u2] = 2;
        
        CHECK(m.size() == 2);
        CHECK(m[u1] == 1);
        CHECK(m[u2] == 2);
    }

    SECTION("Binary search with std::lower_bound") {
        std::vector<uuid7pp::uuid> uuids;
        for (uint64_t ts = 1000; ts <= 5000; ts += 1000) {
            uuids.push_back(uuid7pp::generator::generate_at(ts));
        }

        // Search for UUID at or after 2500ms
        auto const target_ts = 2500ULL;
        // Create a search key (Nil UUID with specific timestamp)
        uuid7pp::uuid search_key{.data = {}};
        for (int i = 0; i < 6; ++i) {
            search_key.data[i] = static_cast<uint8_t>((target_ts >> (8 * (5 - i))) & 0xFF);
        }

        auto it = std::lower_bound(uuids.begin(), uuids.end(), search_key);
        REQUIRE(it != uuids.end());
        CHECK(uuid7pp::extract_timestamp(*it) == std::chrono::system_clock::time_point{std::chrono::milliseconds{3000}});
    }
}

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
