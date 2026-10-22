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
        "scalar": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "scalar": False,
    }
    exports_sources = (
        "CMakeLists.txt",
        "include/*",
        "src/*",
        "test/*",
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
        self.test_requires("catch2/3.16.0")

    def validate(self):
        check_min_cppstd(self, 20)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["QUICKHEAP_SCALAR"] = bool(self.options.scalar)
        tc.cache_variables["CMAKE_INSTALL_PREFIX"] = os.path.join(
            self.build_folder, "package"
        )
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.build(target="install")
        cmake.ctest()

    def package(self):
        CMake(self).install()

    def package_info(self):
        self.cpp_info.libs = ["quickheap"]
        if not self.options.shared:
            self.cpp_info.defines = ["QUICKHEAP_STATIC_DEFINE"]
