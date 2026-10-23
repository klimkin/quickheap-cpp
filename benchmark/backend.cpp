#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backend.cpp"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/targets.h"

#include "backend.hpp"

#include <array>
#include <stdexcept>
#include <string>

HWY_BEFORE_NAMESPACE();
namespace quickheap::bench::HWY_NAMESPACE {

std::int64_t BackendTarget() { return HWY_TARGET; }

}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace quickheap::bench {

HWY_EXPORT(BackendTarget);

namespace {

struct NamedTarget {
    std::string_view name;
    std::int64_t target;
};

constexpr std::array named_targets{
    NamedTarget{"scalar", HWY_SCALAR},
    NamedTarget{"avx2", HWY_AVX2},
    NamedTarget{"avx512", HWY_AVX3},
};

std::int64_t host_targets() {
    static const auto targets = hwy::SupportedTargets();
    return targets;
}

}

std::vector<BackendOption> backend_options() {
    const auto supported = host_targets();
    std::vector<BackendOption> options;
    options.reserve(named_targets.size());
    for (const auto &[name, target] : named_targets) {
        options.push_back({name, hwy::TargetName(target), (HWY_TARGETS & target) != 0,
                           target == HWY_SCALAR || (supported & target) != 0});
    }
    return options;
}

void select_backend(std::string_view name) {
    const auto supported = host_targets();
    if (name == "native") {
        hwy::SetSupportedTargetsForTest(0);
        return;
    }
    for (const auto &[candidate, target] : named_targets) {
        if (candidate != name) continue;
        if ((HWY_TARGETS & target) == 0)
            throw std::runtime_error("SIMD backend was not compiled: " + std::string(name));
        if (target != HWY_SCALAR && (supported & target) == 0)
            throw std::runtime_error("SIMD backend is unsupported by this CPU: " + std::string(name));
        hwy::SetSupportedTargetsForTest(target);
        if (HWY_DYNAMIC_DISPATCH(BackendTarget)() != target)
            throw std::runtime_error("SIMD backend dispatch did not select " + std::string(name));
        return;
    }
    throw std::runtime_error("unknown SIMD backend: " + std::string(name));
}

const char *simd_backend() {
    return hwy::TargetName(HWY_DYNAMIC_DISPATCH(BackendTarget)());
}

}
#endif
