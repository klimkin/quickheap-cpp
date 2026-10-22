import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualRunEnv


class QuickHeapBenchmarkConan(ConanFile):
    name = "quickheap-bench"
    version = "0.2.0"
    license = "MIT"
    description = "Quickheap workloads, competitors, and configuration experiments"
    package_type = "application"
    settings = "os", "arch", "compiler", "build_type"
    options = {"boost": [True, False], "external": [True, False]}
    default_options = {"boost": False, "external": False}
    exports_sources = (
        "CMakeLists.txt",
        "*.cpp",
        "*.hpp",
        "README.md",
        "run-all.sh",
        "data/*.gr",
        "evals/*.py",
        "external/*",
    )

    def layout(self):
        cmake_layout(self)
        self.cpp.build.bindirs = [os.path.join("package", "bin")]
        self.layouts.build.buildenv_info.prepend_path(
            "PATH", os.path.join("package", "bin")
        )
        self.layouts.build.runenv_info.prepend_path(
            "PATH", os.path.join("package", "bin")
        )

    def requirements(self):
        self.requires("quickheap/[^0.2]", headers=True, libs=True, run=True)
        self.requires("highway/[^1.4]", transitive_headers=False)
        if self.options.boost:
            self.requires("boost/[^1.90]", transitive_headers=False)

    def build_requirements(self):
        self.tool_requires("cmake/[>=4.0]")
        self.test_requires("catch2/[^3.16]")

    def validate(self):
        check_min_cppstd(self, 20)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["QUICKHEAP_SCALAR"] = bool(
            self.dependencies["quickheap"].options.scalar
        )
        toolchain.cache_variables["QUICKHEAP_BENCH_BOOST"] = bool(self.options.boost)
        toolchain.cache_variables["QUICKHEAP_BENCH_EXTERNAL"] = bool(self.options.external)
        toolchain.cache_variables["CMAKE_INSTALL_PREFIX"] = os.path.join(
            self.build_folder, "package"
        )
        toolchain.generate()
        CMakeDeps(self).generate()
        VirtualRunEnv(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.build(target="install")
        with VirtualRunEnv(self).vars().apply():
            cmake.ctest()

    def package(self):
        CMake(self).install()

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
