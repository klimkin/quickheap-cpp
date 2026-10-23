#include "fixtures.hpp"

#include <iostream>

namespace {
using namespace quickheap::bench;

struct Options {
    std::string workload;
    std::string elem;
    std::size_t n = 0;
    std::filesystem::path output;
    std::filesystem::path inspect;
    bool dump = false;
    bool help = false;
};

Options parse(int argc, char **argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag(argv[i]);
        const auto value = [&]() -> std::string_view {
            if (++i >= argc) throw std::runtime_error("missing value for " + std::string(flag));
            return argv[i];
        };
        if (flag == "--generate") options.workload = value();
        else if (flag == "--elem") options.elem = value();
        else if (flag == "--n") {
            const auto token = value();
            const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), options.n);
            if (error != std::errc{} || end != token.data() + token.size())
                throw std::runtime_error("--n requires an unsigned decimal integer");
        } else if (flag == "--output") options.output = value();
        else if (flag == "--inspect") options.inspect = value();
        else if (flag == "--dump") options.dump = true;
        else if (flag == "--help" || flag == "-h") options.help = true;
        else throw std::runtime_error("unsupported option: " + std::string(flag));
    }
    if (options.help) return options;
    if (!options.inspect.empty()) {
        if (!options.workload.empty() || !options.elem.empty() || options.n != 0 || !options.output.empty())
            throw std::runtime_error("--inspect cannot combine with generation options");
    } else if (options.workload.empty() || options.elem.empty() || options.n == 0 ||
               options.output.empty() || options.dump) {
        throw std::runtime_error("generation requires --generate, --elem, --n, and --output");
    }
    return options;
}

template <typename T> void generate(const Options &options) {
    const auto fixture = generate_fixture<T>(fixture_workload(options.workload), options.n);
    const auto bytes = encode_fixture(fixture);
    std::ofstream output(options.output, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot write fixture: " + options.output.string());
    output << bytes;
    output.close();
    if (!output) throw std::runtime_error("cannot write fixture: " + options.output.string());
}

void inspect(const Options &options) {
    const auto fixture = load_raw_fixture(options.inspect);
    std::cout << "protocol,elem,workload,n,fixture_digest,setup_count,timed_count\n"
              << fixture_protocol << ',' << fixture.elem << ',' << fixture_workload_name(fixture.workload)
              << ',' << fixture.n << ',' << fixture.digest << ',' << fixture.setup.size() << ','
              << fixture.timed.size() << '\n';
    if (options.dump) {
        const auto region = [](std::string_view label, const auto &values) {
            std::cout << label;
            for (const auto value : values) std::cout << ',' << value;
            std::cout << '\n';
        };
        region("setup", fixture.setup);
        region("timed", fixture.timed);
    }
}
} // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse(argc, argv);
        if (options.help) {
            std::cout << "Usage: quickheap_fixture --generate WORKLOAD --elem TYPE --n N --output FILE\n"
                         "       quickheap_fixture --inspect FILE [--dump]\n"
                         "Types: i32, i64, u32, u64. Size: 1 through 2^25.\n"
                         "--dump emits decoded unsigned key bits or offsets.\n";
        } else if (!options.inspect.empty()) inspect(options);
        else if (options.elem == "i32") generate<std::int32_t>(options);
        else if (options.elem == "i64") generate<std::int64_t>(options);
        else if (options.elem == "u32") generate<std::uint32_t>(options);
        else if (options.elem == "u64") generate<std::uint64_t>(options);
        else throw std::runtime_error("unknown fixture elem: " + options.elem);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
