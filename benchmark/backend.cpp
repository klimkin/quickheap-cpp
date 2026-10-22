#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "backend.cpp"
#include "hwy/foreach_target.h"
#include "hwy/highway.h"
#include "hwy/targets.h"

#include "backend.hpp"

HWY_BEFORE_NAMESPACE();
namespace quickheap::bench::HWY_NAMESPACE {

std::int64_t BackendTarget() { return HWY_TARGET; }

}
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace quickheap::bench {

HWY_EXPORT(BackendTarget);

const char *simd_backend() {
    return hwy::TargetName(HWY_DYNAMIC_DISPATCH(BackendTarget)());
}

}
#endif
