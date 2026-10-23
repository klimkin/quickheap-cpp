#ifndef QUICKHEAP_BENCH_FIXTURES_HPP
#define QUICKHEAP_BENCH_FIXTURES_HPP

#include "workloads.hpp"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace quickheap::bench {

inline constexpr std::string_view fixture_protocol = "quickheap-cpp-v1";
inline constexpr std::size_t max_fixture_n = std::size_t{1} << 25;

struct FixtureShape {
    std::size_t setup_count;
    std::size_t timed_count;
    bool offsets;
};

inline FixtureShape fixture_shape(Workload workload, std::size_t n) {
    if (n == 0 || n > max_fixture_n)
        throw std::runtime_error("fixture n must be in [1, 2^25]");
    switch (workload) {
    case Workload::HeapSort: return {0, n, false};
    case Workload::MonotoneConstantSize: return {2 * (n + 50), 10 * n, true};
    case Workload::RandomConstantSize: return {2 * (n + 50), 10 * n, false};
    case Workload::MonotoneWiggle:
    case Workload::GeometricMonotoneWiggle: return {0, 3 * n, true};
    case Workload::RandomWiggle: return {0, 3 * n, false};
    case Workload::WorstCaseDescending: return {0, 9 * n, false};
    case Workload::MostlyPushDecreasing: return {0, 10 * n, false};
    }
    throw std::runtime_error("unknown fixture workload");
}

inline Workload fixture_workload(std::string_view name) {
    for (const auto &entry : workloads)
        if (entry.name == name) return entry.workload;
    throw std::runtime_error("unknown fixture workload: " + std::string(name));
}

inline std::string_view fixture_workload_name(Workload workload) {
    for (const auto &entry : workloads)
        if (entry.workload == workload) return entry.name;
    throw std::runtime_error("unknown fixture workload");
}

inline std::uint64_t fixture_max_bits(std::string_view elem) {
    if (elem == "i32" || elem == "u32") return UINT32_MAX;
    if (elem == "i64" || elem == "u64") return UINT64_MAX;
    throw std::runtime_error("unknown fixture elem: " + std::string(elem));
}

inline std::string fixture_digest(std::string_view bytes) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream result;
    result << "fnv1a64:" << std::hex << std::setfill('0') << std::setw(16) << hash;
    return result.str();
}

struct RawFixture {
    std::string elem;
    Workload workload;
    std::size_t n;
    std::vector<std::uint64_t> setup;
    std::vector<std::uint64_t> timed;
    std::string digest;
};

template <typename T> struct Fixture {
    Workload workload;
    std::size_t n;
    std::vector<T> setup_keys;
    std::vector<T> keys;
    std::vector<std::uint64_t> setup_offsets;
    std::vector<std::uint64_t> offsets;
    std::string digest;
};

class FixtureTokens {
  public:
    explicit FixtureTokens(std::string_view bytes) : remaining_(bytes) {}

    std::string_view next() {
        skip_space();
        if (remaining_.empty()) throw std::runtime_error("truncated fixture");
        const auto end = remaining_.find_first_of(" \t\n\r\f\v");
        const auto token = remaining_.substr(0, end);
        remaining_.remove_prefix(token.size());
        return token;
    }

    std::uint64_t number() {
        const auto token = next();
        std::uint64_t value = 0;
        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (error != std::errc{} || end != token.data() + token.size())
            throw std::runtime_error("fixture requires unsigned decimal integers");
        return value;
    }

    void expect(std::string_view label) {
        if (next() != label) throw std::runtime_error("fixture requires " + std::string(label));
    }

    bool empty() {
        skip_space();
        return remaining_.empty();
    }

    std::size_t bytes_left() const { return remaining_.size(); }

  private:
    void skip_space() {
        const auto start = remaining_.find_first_not_of(" \t\n\r\f\v");
        remaining_.remove_prefix(start == std::string_view::npos ? remaining_.size() : start);
    }

    std::string_view remaining_;
};

inline RawFixture parse_raw_fixture(std::string_view bytes) {
    FixtureTokens input(bytes);
    input.expect("QHB1");
    const std::string elem(input.next());
    const auto max_bits = fixture_max_bits(elem);
    const auto workload = fixture_workload(input.next());
    const auto raw_n = input.number();
    if (raw_n == 0 || raw_n > max_fixture_n)
        throw std::runtime_error("fixture n must be in [1, 2^25]");
    const auto n = static_cast<std::size_t>(raw_n);
    const auto shape = fixture_shape(workload, n);
    const auto region = [&](std::string_view label, std::size_t expected) {
        input.expect(label);
        if (input.number() != expected) throw std::runtime_error("wrong fixture region length");
        if (expected > input.bytes_left()) throw std::runtime_error("truncated fixture region");
        std::vector<std::uint64_t> values;
        values.reserve(expected);
        for (std::size_t i = 0; i < expected; ++i) {
            const auto value = input.number();
            if (!shape.offsets && value > max_bits)
                throw std::runtime_error("fixture value exceeds elem width");
            values.push_back(value);
        }
        return values;
    };
    auto setup = region("setup", shape.setup_count);
    auto timed = region("timed", shape.timed_count);
    if (!input.empty()) throw std::runtime_error("trailing fixture tokens");
    return {elem, workload, n, std::move(setup), std::move(timed), fixture_digest(bytes)};
}

inline std::string read_fixture_bytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open fixture: " + path.string());
    std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad()) throw std::runtime_error("cannot read fixture: " + path.string());
    return bytes;
}

inline RawFixture load_raw_fixture(const std::filesystem::path &path) {
    return parse_raw_fixture(read_fixture_bytes(path));
}

template <typename T> Fixture<T> typed_fixture(RawFixture raw) {
    static_assert(std::is_same_v<T, std::int32_t> || std::is_same_v<T, std::int64_t> ||
                  std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>);
    if (raw.elem != key_name<T>()) throw std::runtime_error("fixture elem does not match runner");
    Fixture<T> result{raw.workload, raw.n, {}, {}, {}, {}, std::move(raw.digest)};
    if (fixture_shape(raw.workload, raw.n).offsets) {
        result.setup_offsets = std::move(raw.setup);
        result.offsets = std::move(raw.timed);
    } else {
        const auto decode = [](const auto &values) {
            std::vector<T> keys;
            keys.reserve(values.size());
            for (auto value : values)
                keys.push_back(key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(value)));
            return keys;
        };
        result.setup_keys = decode(raw.setup);
        result.keys = decode(raw.timed);
    }
    return result;
}

template <typename T> Fixture<T> parse_fixture(std::string_view bytes) {
    return typed_fixture<T>(parse_raw_fixture(bytes));
}

template <typename T> Fixture<T> load_fixture(const std::filesystem::path &path) {
    return typed_fixture<T>(load_raw_fixture(path));
}

inline std::string encode_raw_fixture(const RawFixture &fixture) {
    const auto max_bits = fixture_max_bits(fixture.elem);
    const auto shape = fixture_shape(fixture.workload, fixture.n);
    if (fixture.setup.size() != shape.setup_count || fixture.timed.size() != shape.timed_count)
        throw std::runtime_error("wrong fixture region length");
    std::ostringstream output;
    output << "QHB1 " << fixture.elem << ' ' << fixture_workload_name(fixture.workload) << ' '
           << fixture.n << '\n';
    const auto region = [&](std::string_view label, const auto &values) {
        output << label << ' ' << values.size() << '\n';
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (!shape.offsets && values[i] > max_bits)
                throw std::runtime_error("fixture value exceeds elem width");
            if (i != 0) output << ' ';
            output << values[i];
        }
        output << '\n';
    };
    region("setup", fixture.setup);
    region("timed", fixture.timed);
    return output.str();
}

template <typename T> std::string encode_fixture(const Fixture<T> &fixture) {
    const auto shape = fixture_shape(fixture.workload, fixture.n);
    RawFixture raw{std::string(key_name<T>()), fixture.workload, fixture.n, {}, {}, {}};
    if (shape.offsets) {
        if (!fixture.setup_keys.empty() || !fixture.keys.empty())
            throw std::runtime_error("offset fixture contains keys");
        raw.setup = fixture.setup_offsets;
        raw.timed = fixture.offsets;
    } else {
        if (!fixture.setup_offsets.empty() || !fixture.offsets.empty())
            throw std::runtime_error("key fixture contains offsets");
        for (T key : fixture.setup_keys) raw.setup.push_back(static_cast<std::make_unsigned_t<T>>(key));
        for (T key : fixture.keys) raw.timed.push_back(static_cast<std::make_unsigned_t<T>>(key));
    }
    return encode_raw_fixture(raw);
}

class FixtureRng {
  public:
    explicit FixtureRng(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next() {
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }

    std::uint64_t bounded(std::uint64_t bound) {
        const auto threshold = (std::uint64_t{0} - bound) % bound;
        auto value = next();
        while (value < threshold) value = next();
        return value % bound;
    }

  private:
    std::uint64_t state_;
};

inline constexpr std::uint64_t default_fixture_seed = 0x51484c42ULL;

template <typename T> Fixture<T> generate_fixture(Workload workload, std::size_t n,
                                                 std::uint64_t seed = default_fixture_seed) {
    const auto shape = fixture_shape(workload, n);
    FixtureRng rng(seed ^ (static_cast<std::uint64_t>(workload) << 56) ^
                   (static_cast<std::uint64_t>(sizeof(T)) << 48) ^ n);
    RawFixture raw{std::string(key_name<T>()), workload, n, {}, {}, {}};
    const auto next = [&]() -> std::uint64_t {
        if (shape.offsets) {
            if (workload == Workload::GeometricMonotoneWiggle) {
                const long double uniform = static_cast<long double>((rng.next() >> 11) + 1) /
                                            9007199254740993.0L;
                const long double value = std::floor(std::log(uniform) /
                    std::log1p(-1.0L / static_cast<long double>(stride<T>())));
                return value >= static_cast<long double>(UINT64_MAX)
                           ? UINT64_MAX : static_cast<std::uint64_t>(value);
            }
            return rng.bounded(stride<T>());
        }
        if (workload == Workload::RandomConstantSize) return rng.bounded(std::uint64_t{1} << 15);
        if (workload == Workload::RandomWiggle || workload == Workload::MostlyPushDecreasing)
            return rng.bounded(stride<T>());
        T key = key_from_bits<T>(static_cast<std::make_unsigned_t<T>>(rng.next()));
        if (key == std::numeric_limits<T>::min()) ++key;
        if (key == std::numeric_limits<T>::max()) --key;
        return static_cast<std::make_unsigned_t<T>>(key);
    };
    raw.setup.reserve(shape.setup_count);
    raw.timed.reserve(shape.timed_count);
    for (std::size_t i = 0; i < shape.setup_count; ++i) raw.setup.push_back(next());
    for (std::size_t i = 0; i < shape.timed_count; ++i) {
        if (workload == Workload::WorstCaseDescending && (rng.next() & 1) != 0)
            raw.timed.push_back(shape.timed_count - i - 1);
        else raw.timed.push_back(next());
    }
    if (workload == Workload::MostlyPushDecreasing)
        std::sort(raw.timed.begin(), raw.timed.end(), std::greater<std::uint64_t>());
    raw.digest = fixture_digest(encode_raw_fixture(raw));
    return typed_fixture<T>(std::move(raw));
}

} // namespace quickheap::bench

#endif
