#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace quickheap::bench {

struct BackendOption {
    std::string_view name;
    const char *target;
    bool compiled;
    bool host_supported;
};

std::vector<BackendOption> backend_options();
void select_backend(std::string_view name);
const char *simd_backend();

}
