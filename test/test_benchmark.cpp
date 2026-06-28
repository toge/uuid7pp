#include "catch2/catch_all.hpp"
#include "uuid7pp.hpp"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

static auto const bench_uuid = uuid7pp::generator::generate();
static auto const bench_str_hyphen = uuid7pp::to_string(bench_uuid);
static auto const bench_str_plain = uuid7pp::to_string(bench_uuid, false);

TEST_CASE("UUID v7 Benchmark", "[benchmark]") {
    SECTION("Generation Only") {
        BENCHMARK("uuid7pp::generate") {
            return uuid7pp::generator::generate();
        };

        boost::uuids::time_generator_v7 boost_gen;
        BENCHMARK("boost::generate") {
            return boost_gen();
        };

        alignas(16) uuid7pp::uuid buf[1000];
        BENCHMARK("uuid7pp::generate_batch(1000) per item") {
            uuid7pp::generator::generate_batch(buf, 1000);
            return buf[0].data[0];
        };
    }

    SECTION("String Conversion (std::string)") {
        auto const u = uuid7pp::generator::generate();

        BENCHMARK("uuid7pp::to_string") {
            return uuid7pp::to_string(u);
        };

        boost::uuids::time_generator_v7 boost_gen;
        auto const bu = boost_gen();
        BENCHMARK("boost::to_string") {
            return boost::uuids::to_string(bu);
        };
    }

    SECTION("Low-level string conversion (to_chars / etc)") {
        auto const u = uuid7pp::generator::generate();
        char buf[36];

        BENCHMARK("uuid7pp::to_chars (zero-alloc)") {
            uuid7pp::to_chars(u, buf);
            return buf[0];
        };

        BENCHMARK("uuid7pp::to_chars (no-hyphen, zero-alloc)") {
            uuid7pp::to_chars(u, buf, false);
            return buf[0];
        };
    }

    SECTION("Parse from chars") {
        BENCHMARK("uuid7pp::from_chars (hyphen)") {
            return uuid7pp::from_chars(bench_str_hyphen);
        };

        BENCHMARK("uuid7pp::from_chars (plain)") {
            return uuid7pp::from_chars(bench_str_plain);
        };
    }

    SECTION("Timestamp extraction") {
        BENCHMARK("uuid7pp::extract_timestamp") {
            return uuid7pp::extract_timestamp(bench_uuid);
        };
    }
}
