#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>

#if __has_include(<sys/random.h>)
#include <sys/random.h>
#endif

namespace quickheap::detail {

// Mirrors `rand::rng()` from rand 0.10: a thread-local `BlockRng` over ChaCha12 (legacy
// 64-bit counter, zero nonce) producing 4 blocks per refill and reseeding from the OS
// after 1024 blocks.
class ThreadRng {
    static constexpr std::size_t block_words = 16;
    static constexpr std::size_t buf_blocks = 4;
    static constexpr std::size_t buffer_size = block_words * buf_blocks;
    static constexpr std::uint64_t reseed_block_threshold = 1024;

    using u32x4 = std::uint32_t __attribute__((vector_size(16)));

    // results_[0] doubles as the read index, as in rand_core 0.10 `BlockRng`.
    alignas(64) std::uint32_t results_[buffer_size];
    std::uint32_t key_[8];
    std::uint64_t block_pos_ = 0;

    static void os_seed(std::uint32_t (&key)[8]) {
#if __has_include(<sys/random.h>)
        auto* bytes = reinterpret_cast<unsigned char*>(key);
        std::size_t filled = 0;
        while (filled < sizeof(key)) {
            const auto got = getrandom(bytes + filled, sizeof(key) - filled, 0);
            if (got > 0) {
                filled += static_cast<std::size_t>(got);
            }
        }
#else
        std::random_device device;
        for (auto& word : key) {
            word = device();
        }
#endif
    }

    static u32x4 rotl(u32x4 value, int bits) noexcept {
        return (value << bits) | (value >> (32 - bits));
    }

    static void quarter_round(u32x4& a, u32x4& b, u32x4& c, u32x4& d) noexcept {
        a += b; d = rotl(d ^ a, 16);
        c += d; b = rotl(b ^ c, 12);
        a += b; d = rotl(d ^ a, 8);
        c += d; b = rotl(b ^ c, 7);
    }

    [[gnu::noinline]] void reseed() {
        os_seed(key_);
        block_pos_ = 0;
    }

    void generate() noexcept {
        if (block_pos_ >= reseed_block_threshold) [[unlikely]] {
            reseed();
        }
        u32x4 x[block_words];
        const std::uint32_t constants[4] = {0x61707865, 0x3320646e, 0x79622d32, 0x6b206574};
        for (std::size_t word = 0; word < 4; ++word) {
            x[word] = u32x4{} + constants[word];
        }
        for (std::size_t word = 0; word < 8; ++word) {
            x[4 + word] = u32x4{} + key_[word];
        }
        u32x4 counter_lo;
        u32x4 counter_hi;
        for (std::size_t block = 0; block < buf_blocks; ++block) {
            const auto counter = block_pos_ + block;
            counter_lo[block] = static_cast<std::uint32_t>(counter);
            counter_hi[block] = static_cast<std::uint32_t>(counter >> 32);
        }
        x[12] = counter_lo;
        x[13] = counter_hi;
        x[14] = u32x4{};
        x[15] = u32x4{};
        u32x4 state[block_words];
        std::memcpy(state, x, sizeof(x));
        for (int round = 0; round < 12; round += 2) {
            quarter_round(x[0], x[4], x[8], x[12]);
            quarter_round(x[1], x[5], x[9], x[13]);
            quarter_round(x[2], x[6], x[10], x[14]);
            quarter_round(x[3], x[7], x[11], x[15]);
            quarter_round(x[0], x[5], x[10], x[15]);
            quarter_round(x[1], x[6], x[11], x[12]);
            quarter_round(x[2], x[7], x[8], x[13]);
            quarter_round(x[3], x[4], x[9], x[14]);
        }
        for (std::size_t word = 0; word < block_words; ++word) {
            const auto out = x[word] + state[word];
            for (std::size_t block = 0; block < buf_blocks; ++block) {
                results_[block * block_words + word] = out[block];
            }
        }
        block_pos_ += buf_blocks;
    }

    ThreadRng() {
        os_seed(key_);
        results_[0] = buffer_size;
    }

public:
    static ThreadRng& get() {
        thread_local ThreadRng rng;
        return rng;
    }

    std::uint32_t next_u32() noexcept {
        std::size_t index = results_[0];
        if (index >= buffer_size) {
            generate();
            index = 0;
        }
        const auto value = results_[index];
        results_[0] = static_cast<std::uint32_t>(index + 1);
        return value;
    }

    std::uint64_t next_u64() noexcept {
        const std::uint64_t lo = next_u32();
        return lo | (static_cast<std::uint64_t>(next_u32()) << 32);
    }
};

// Mirrors `rand::random_range(0..high)` for `usize`: Canon's method on `u32` when the
// range fits, `u64` otherwise.
inline std::size_t random_range(std::size_t high) noexcept {
    auto& rng = ThreadRng::get();
    if (high > UINT32_MAX) {
        const auto range = static_cast<unsigned __int128>(high);
        const auto wide = static_cast<unsigned __int128>(rng.next_u64()) * range;
        auto result = static_cast<std::uint64_t>(wide >> 64);
        const auto lo = static_cast<std::uint64_t>(wide);
        if (lo > static_cast<std::uint64_t>(0) - static_cast<std::uint64_t>(high)) {
            const auto new_hi = static_cast<std::uint64_t>(
                (static_cast<unsigned __int128>(rng.next_u64()) * range) >> 64);
            result += lo + new_hi < lo;
        }
        return static_cast<std::size_t>(result);
    }
    const auto range = static_cast<std::uint32_t>(high);
    const auto wide = static_cast<std::uint64_t>(rng.next_u32()) * range;
    auto result = static_cast<std::uint32_t>(wide >> 32);
    const auto lo = static_cast<std::uint32_t>(wide);
    if (lo > static_cast<std::uint32_t>(0u - range)) {
        const auto new_hi = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(rng.next_u32()) * range) >> 32);
        result += static_cast<std::uint32_t>(lo + new_hi) < lo;
    }
    return result;
}

}
