import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class QuickHeapConan(ConanFile):
    name = "quickheap"
    version = "0.2.0"
    license = "MIT"
    description = "Portable SIMD bucket min-priority queue"
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_tests": [True, False],
        "with_benchmarks": [True, False],
        "scalar": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_tests": True,
        "with_benchmarks": False,
        "scalar": False,
    }
    exports_sources = (
        "CMakeLists.txt",
        "include/*",
        "src/*",
        "test/*",
        "benchmark/*",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)
        self.cpp.build.libdirs = [os.path.join("package", "lib")]
        self.cpp.build.includedirs = [os.path.join("package", "include")]
        self.cpp.source.includedirs = []

    def requirements(self):
        self.requires("highway/1.4.0", transitive_headers=False)

    def build_requirements(self):
        self.tool_requires("cmake/[>=4.0]")
        if self.options.with_tests:
            self.test_requires("gtest/[^1.15]")
            if self.options.with_benchmarks:
                self.test_requires("benchmark/[^1.9]")

    def validate(self):
        check_min_cppstd(self, 20)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["BUILD_TESTING"] = bool(self.options.with_tests)
        toolchain.cache_variables["QUICKHEAP_BUILD_BENCHMARKS"] = bool(
            self.options.with_benchmarks
        )
        toolchain.cache_variables["QUICKHEAP_SCALAR"] = bool(self.options.scalar)
        toolchain.cache_variables["CMAKE_INSTALL_PREFIX"] = os.path.join(
            self.build_folder, "package"
        )
        toolchain.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.build(target="install")
        cmake.ctest()

    def package(self):
        CMake(self).install()

    def package_id(self):
        del self.info.options.with_tests
        del self.info.options.with_benchmarks

    def package_info(self):
        self.cpp_info.libs = ["quickheap"]
        if not self.options.shared:
            self.cpp_info.defines = ["QUICKHEAP_STATIC_DEFINE"]
