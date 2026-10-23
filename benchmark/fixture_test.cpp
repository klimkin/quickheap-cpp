#include "fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

namespace {
using namespace quickheap::bench;

void require(bool condition, std::string_view message) {
    INFO(message);
    REQUIRE(condition);
}

void rejects(std::string_view bytes) {
    bool rejected = false;
    try { static_cast<void>(parse_raw_fixture(bytes)); }
    catch (const std::runtime_error &) { rejected = true; }
    require(rejected, "malformed fixture accepted");
}

template <typename T> void roundtrips() {
    for (const auto &entry : workloads) {
        const auto fixture = generate_fixture<T>(entry.workload, 8);
        const auto bytes = encode_fixture(fixture);
        const auto decoded = parse_fixture<T>(bytes);
        require(encode_fixture(decoded) == bytes, "fixture changed during decode");
        require(decoded.digest == fixture_digest(bytes), "fixture digest differs");
        require(fixture.digest == decoded.digest, "generator digest differs");
        require(encode_fixture(generate_fixture<T>(entry.workload, 8)) == bytes,
                "generator is not deterministic");
        const auto shape = fixture_shape(entry.workload, 8);
        require(decoded.setup_keys.size() + decoded.setup_offsets.size() == shape.setup_count,
                "wrong decoded setup length");
        require(decoded.keys.size() + decoded.offsets.size() == shape.timed_count,
                "wrong decoded timed length");
        require(shape.offsets ? decoded.keys.empty() : decoded.offsets.empty(),
                "fixture representation differs from workload");
    }
}
} // namespace

TEST_CASE("Fixture parser and generator follow shared protocol", "[fixtures]") {
        roundtrips<std::int32_t>();
        roundtrips<std::int64_t>();
        roundtrips<std::uint32_t>();
        roundtrips<std::uint64_t>();
        const auto signed32 = parse_fixture<std::int32_t>(
            "QHB1 i32 HeapSort 4\nsetup 0\ntimed 4\n0 4294967295 2147483648 2147483647\n");
        require(signed32.keys == std::vector<std::int32_t>{0, -1, INT32_MIN, INT32_MAX},
                "i32 bit decoding differs");
        const auto signed64 = parse_fixture<std::int64_t>(
            "QHB1 i64 HeapSort 4\nsetup 0\ntimed 4\n0 18446744073709551615 "
            "9223372036854775808 9223372036854775807\n");
        require(signed64.keys == std::vector<std::int64_t>{0, -1, INT64_MIN, INT64_MAX},
                "i64 bit decoding differs");
        require(fixture_digest("") == "fnv1a64:cbf29ce484222325", "FNV empty vector differs");
        require(fixture_digest("a") == "fnv1a64:af63dc4c8601ec8c", "FNV ASCII vector differs");
        const std::string valid = "QHB1 u32 HeapSort 1\nsetup 0\ntimed 1\n0\n";
        require(parse_raw_fixture(valid).digest != parse_raw_fixture(valid + "\n").digest,
                "digest ignores original bytes");
        require(parse_fixture<std::uint32_t>(
                    "QHB1 u32 MonotoneWiggle 1 setup 0 timed 3 0 1 18446744073709551615")
                    .offsets.back() == UINT64_MAX,
                "offset parser truncates u64 values");
        for (const auto bad : {
            "", "QHB2 u32 HeapSort 1 setup 0 timed 1 0",
            "QHB1 f32 HeapSort 1 setup 0 timed 1 0",
            "QHB1 u32 Unknown 1 setup 0 timed 1 0",
            "QHB1 u32 HeapSort 0 setup 0 timed 0",
            "QHB1 u32 HeapSort 33554433 setup 0 timed 1 0",
            "QHB1 u32 HeapSort -1 setup 0 timed 1 0",
            "QHB1 u32 HeapSort +1 setup 0 timed 1 0",
            "QHB1 u32 HeapSort 1 setup 1 0 timed 1 0",
            "QHB1 u32 HeapSort 1 setup 0 timed 18446744073709551615",
            "QHB1 u32 HeapSort 1 setup 0 timed 1",
            "QHB1 u32 HeapSort 1 setup 0 timed 1 -1",
            "QHB1 u32 HeapSort 1 setup 0 timed 1 +1",
            "QHB1 u32 HeapSort 1 setup 0 timed 1 0x1",
            "QHB1 u32 HeapSort 1 setup 0 timed 1 4294967296",
            "QHB1 u64 HeapSort 1 setup 0 timed 1 18446744073709551616",
            "QHB1 u32 HeapSort 1 setup 0 timed 1 0 trailing",
            "QHB1 u32 MonotoneWiggle 1 setup 0 timed 3 0 1",
            "QHB1 u32 MonotoneWiggle 1 setup 0 timed 3 0 1 18446744073709551616"
        }) rejects(bad);
        bool type_rejected = false;
        try { static_cast<void>(parse_fixture<std::int64_t>(valid)); }
        catch (const std::runtime_error &) { type_rejected = true; }
        require(type_rejected, "runner accepted wrong fixture type");
}
