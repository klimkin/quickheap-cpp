#include "backend.hpp"
#include "simd.hpp"
#include "workloads.hpp"

#include <catch2/catch_test_macros.hpp>

namespace quickheap::bench {

TEST_CASE("Explicit SIMD targets select compiled supported kernels", "[backend]") {
    const std::string original = simd_backend();
    for (const auto &backend : backend_options()) {
        CAPTURE(backend.name);
        if (!backend.compiled || !backend.host_supported) {
            REQUIRE_THROWS(select_backend(backend.name));
            continue;
        }
        select_backend(backend.name);
        CHECK(std::string(simd_backend()) == backend.target);
        std::array<std::int32_t, 32> pivots{};
        for (std::size_t i = 0; i < pivots.size(); ++i) pivots[i] = 63 - static_cast<std::int32_t>(2 * i);
        CHECK(push_position(pivots.data(), pivots.size(), 12) == 26);
        KeyQuickHeap<std::int32_t> heap;
        std::vector<std::int32_t> keys;
        for (int i = 0; i < 257; ++i) {
            keys.push_back((i * 31) % 73);
            heap.push(keys.back());
        }
        std::sort(keys.begin(), keys.end());
        for (const auto key : keys) REQUIRE(heap.try_pop() == key);
        REQUIRE_FALSE(heap.try_pop());
    }
    select_backend("native");
    CHECK(std::string(simd_backend()) == original);
    REQUIRE_THROWS(select_backend("unknown"));
}

}
