#include "quickheap.hpp"

#include <charconv>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>

namespace {

template <typename T> int run_trace() {
    quickheap::Heap<T> heap;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(std::cin, line)) {
        ++line_number;
        std::istringstream input(line);
        std::string operation;
        input >> operation;
        if (operation.empty()) {
            continue;
        }

        std::string argument;
        std::string extra;
        if (operation == "push") {
            if (!(input >> argument) || (input >> extra)) {
                std::cerr << "invalid push at line " << line_number << '\n';
                return 2;
            }
            T value{};
            const auto [end, error] =
                std::from_chars(argument.data(), argument.data() + argument.size(), value);
            if (error != std::errc{} || end != argument.data() + argument.size()) {
                std::cerr << "invalid integer at line " << line_number << '\n';
                return 2;
            }
            heap.push(value);
        } else if (operation == "pop" && !(input >> extra)) {
            const auto value = heap.try_pop();
            if (value) {
                std::cout << "pop " << *value << '\n';
            } else {
                std::cout << "pop none\n";
            }
        } else if (operation == "len" && !(input >> extra)) {
            std::cout << "len " << heap.size() << '\n';
        } else if (operation == "empty" && !(input >> extra)) {
            std::cout << "empty " << (heap.empty() ? 1 : 0) << '\n';
        } else {
            std::cerr << "invalid command at line " << line_number << '\n';
            return 2;
        }
    }
    return 0;
}

}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: quickheap_trace {i32|u32|i64|u64}\n";
        return 2;
    }
    const std::string type = argv[1];
    if (type == "i32") return run_trace<std::int32_t>();
    if (type == "u32") return run_trace<std::uint32_t>();
    if (type == "i64") return run_trace<std::int64_t>();
    if (type == "u64") return run_trace<std::uint64_t>();
    std::cerr << "unknown type: " << type << '\n';
    return 2;
}
