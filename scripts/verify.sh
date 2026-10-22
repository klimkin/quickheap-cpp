#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_root=${QUICKHEAP_VERIFY_DIR:-"${root_dir}/build/verify"}
profile_dir="${root_dir}/profiles"
build_profile="${profile_dir}/gcc"
cleanup_paths=()
editable_registered=0

cleanup() {
    if ((editable_registered)); then
        conan editable remove --refs quickheap/0.2.0
    fi
    for path in "${cleanup_paths[@]}"; do
        rm -rf -- "${path}"
    done
}
trap cleanup EXIT

command -v conan >/dev/null
command -v cmake >/dev/null

mkdir -p -- "${build_root}"

axes=(
    'gcc-release-static gcc Release False False True'
    'gcc-release-shared gcc Release True False True'
    'gcc-debug-scalar gcc Debug False True False'
    'gcc-asan gcc-asan Debug False True False'
    'gcc-ubsan gcc-ubsan Debug False False False'
    'gcc-tsan gcc-tsan Debug False False False'
)

if [[ -n ${CLANGXX:-} ]]; then
    clang_available=$(command -v -- "${CLANGXX}" || true)
else
    clang_available=$(command -v clang++ || true)
fi

if [[ -n ${clang_available} ]]; then
    axes+=(
        'clang-release-static clang Release False False True'
        'clang-release-shared clang Release True False True'
        'clang-debug-scalar clang Debug False True False'
        'clang-asan clang-asan Debug False True False'
        'clang-ubsan clang-ubsan Debug False False False'
        'clang-tsan clang-tsan Debug False False False'
    )
else
    printf 'Clang unavailable. Set CLANG and CLANGXX to enable Clang axes.\n' >&2
fi

run_axis() {
    local name=$1 profile=$2 build_type=$3 shared=$4 scalar=$5 benchmarks=$6
    local axis_dir="${build_root}/${name}"
    local host_profile="${profile_dir}/${profile}"

    mkdir -p -- "${axis_dir}"
    printf 'Running %s with host profile %s and build profile %s\n' \
        "${name}" "${host_profile}" "${build_profile}"

    local args=(
        -pr:h "${host_profile}"
        -pr:b "${build_profile}"
        -s:h "build_type=${build_type}"
        -o:h "quickheap/*:shared=${shared}"
        -o:h "quickheap/*:scalar=${scalar}"
        -o:h 'quickheap/*:with_tests=True'
        -o:h "quickheap/*:with_benchmarks=${benchmarks}"
        --build=missing
        --lockfile=''
    )

    conan build "${root_dir}" -of "${axis_dir}/local" "${args[@]}"
    local package_dir
    package_dir=$(mktemp -d "${axis_dir}/package.XXXXXX")
    cleanup_paths+=("${package_dir}")
    mkdir -p "${package_dir}/test_package"
    cp -- "${root_dir}/test_package/"{conanfile.py,CMakeLists.txt,test_c.c,test_cpp.cpp} \
        "${package_dir}/test_package/"
    conan create "${root_dir}" "${args[@]}" '--build=quickheap/*' \
        --test-folder "${package_dir}/test_package"
}

selected=("$@")
matched=0
for axis in "${axes[@]}"; do
    read -r name profile build_type shared scalar benchmarks <<<"${axis}"
    if ((${#selected[@]})); then
        match=0
        for requested in "${selected[@]}"; do
            if [[ ${requested} == "${name}" ]]; then
                match=1
            fi
        done
        if ((match == 0)); then
            continue
        fi
    fi
    matched=$((matched + 1))
    run_axis "${name}" "${profile}" "${build_type}" "${shared}" \
        "${scalar}" "${benchmarks}"
done

if ((${#selected[@]})); then
    if ((matched != ${#selected[@]})); then
        printf 'Unknown or duplicate axis requested.\n' >&2
        exit 2
    fi
    exit 0
fi

conan lock create "${root_dir}" \
    -pr:h "${profile_dir}/gcc" -pr:b "${build_profile}" \
    --lockfile='' --lockfile-out "${build_root}/conan.lock"

if conan editable list | rg -q '^quickheap/0\.2\.0'; then
    printf 'quickheap/0.2.0 already registered as editable; leave existing registration unchanged.\n' >&2
    exit 1
fi

editable_dir="${build_root}/editable"
mkdir -p -- "${editable_dir}"
conan editable add "${root_dir}" --name quickheap --version 0.2.0 \
    -of "${editable_dir}/local"
editable_registered=1

conan build "${root_dir}" -of "${editable_dir}/local" \
    -pr:h "${profile_dir}/gcc" -pr:b "${build_profile}" \
    -o:h 'quickheap/*:with_tests=True' --build=missing --lockfile=''
consumer_dir=$(mktemp -d "${editable_dir}/consumer.XXXXXX")
cleanup_paths+=("${consumer_dir}")
cp -- "${root_dir}/test_package/"{conanfile.py,CMakeLists.txt,test_c.c,test_cpp.cpp} \
    "${consumer_dir}/"
conan test "${consumer_dir}" quickheap/0.2.0 \
    -pr:h "${profile_dir}/gcc" -pr:b "${build_profile}" \
    -o:h 'quickheap/*:with_tests=True' --build=missing --lockfile=''
