#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_root=${QUICKHEAP_VERIFY_DIR:-"${root_dir}/build/verify"}
build_profile=${QUICKHEAP_BUILD_PROFILE:-gcc-native-build}
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

axes=(
    'gcc-release-static gcc-native Release False False True'
    'gcc-release-shared gcc-native Release True False True'
    'gcc-debug-scalar gcc-native Debug False True False'
    'gcc-asan gcc-native-asan Debug False True False'
    'gcc-ubsan gcc-native-ubsan Debug False False False'
    'gcc-tsan gcc-native-tsan Debug False False False'
    'gcc-asan-shared gcc-native-asan Debug True False False'
    'gcc-ubsan-shared gcc-native-ubsan Debug True False False'
    'gcc-tsan-shared gcc-native-tsan Debug True False False'
    'gcc-pinned gcc17 Release False False True'
)

if [[ -n ${CLANGXX:-} ]]; then
    clang_available=$(command -v -- "${CLANGXX}" || true)
else
    clang_available=$(command -v clang++ || true)
fi

if [[ -n ${clang_available} ]]; then
    axes+=(
        'clang-release-static clang-native Release False False True'
        'clang-release-shared clang-native Release True False True'
        'clang-debug-scalar clang-native Debug False True False'
        'clang-asan clang-native-asan Debug False True False'
        'clang-ubsan clang-native-ubsan Debug False False False'
        'clang-tsan clang-native-tsan Debug False False False'
        'clang-asan-shared clang-native-asan Debug True False False'
        'clang-ubsan-shared clang-native-ubsan Debug True False False'
        'clang-tsan-shared clang-native-tsan Debug True False False'
        'clang-pinned clang24 Release False False True'
    )
else
    printf 'Clang unavailable. Activate the compiler before verification.\n' >&2
    if (($# == 0)); then
        exit 2
    fi
fi

selected=("$@")
seen_axes=()
for requested in "${selected[@]}"; do
    for seen in "${seen_axes[@]}"; do
        if [[ ${seen} == "${requested}" ]]; then
            printf 'Duplicate axis requested: %s\n' "${requested}" >&2
            exit 2
        fi
    done
    seen_axes+=("${requested}")
    matches=0
    for axis in "${axes[@]}"; do
        if [[ ${axis%% *} == "${requested}" ]]; then
            matches=$((matches + 1))
        fi
    done
    if ((matches != 1)); then
        printf 'Unknown axis requested: %s\n' "${requested}" >&2
        exit 2
    fi
done

editable_refs=$(conan editable list)
if [[ $'\n'${editable_refs}$'\n' == *$'\nquickheap/0.2.0\n'* ]]; then
    printf 'quickheap/0.2.0 already registered as editable; remove it before package verification.\n' >&2
    exit 2
fi

mkdir -p -- "${build_root}"

run_axis() {
    local name=$1 profile=$2 build_type=$3 shared=$4 scalar=$5 benchmarks=$6
    local axis_dir="${build_root}/${name}"
    local host_profile="${profile}"
    if [[ ${profile} == gcc17 || ${profile} == clang24 ]]; then
        host_profile="${root_dir}/profiles/${profile}"
    fi

    mkdir -p -- "${axis_dir}"
    printf 'Running %s with host profile %s and build profile %s\n' \
        "${name}" "${host_profile}" "${build_profile}"

    local args=(
        -pr:h "${host_profile}"
        -pr:b "${build_profile}"
        -s:h "build_type=${build_type}"
        -o:h "quickheap/*:shared=${shared}"
        -o:h "quickheap/*:scalar=${scalar}"
        -o:h "quickheap/*:with_benchmarks=${benchmarks}"
        --build=missing
        --lockfile=''
    )

    local local_dir
    local_dir=$(mktemp -d "${axis_dir}/local.XXXXXX")
    conan build "${root_dir}" -of "${local_dir}" "${args[@]}"
    local package_dir
    package_dir=$(mktemp -d "${axis_dir}/package.XXXXXX")
    cleanup_paths+=("${package_dir}")
    mkdir -p "${package_dir}/test_package"
    cp -- "${root_dir}/test_package/"{conanfile.py,CMakeLists.txt,test_c.c,test_cpp.cpp} \
        "${package_dir}/test_package/"
    conan create "${root_dir}" "${args[@]}" '--build=quickheap/*' \
        --test-folder "${package_dir}/test_package"
}

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
    -pr:h gcc-native -pr:b "${build_profile}" \
    -o:h 'quickheap/*:with_benchmarks=True' \
    --lockfile='' --lockfile-out "${build_root}/conan.lock"

editable_dir="${build_root}/editable"
mkdir -p -- "${editable_dir}"
editable_local=$(mktemp -d "${editable_dir}/local.XXXXXX")
conan editable add "${root_dir}" --name quickheap --version 0.2.0 \
    -of "${editable_local}"
editable_registered=1

conan build "${root_dir}" -of "${editable_local}" \
    -pr:h gcc-native -pr:b "${build_profile}" \
    --build=missing --lockfile=''
consumer_dir=$(mktemp -d "${editable_dir}/consumer.XXXXXX")
cleanup_paths+=("${consumer_dir}")
cp -- "${root_dir}/test_package/"{conanfile.py,CMakeLists.txt,test_c.c,test_cpp.cpp} \
    "${consumer_dir}/"
conan test "${consumer_dir}" quickheap/0.2.0 \
    -pr:h gcc-native -pr:b "${build_profile}" \
    --build=missing --lockfile=''
