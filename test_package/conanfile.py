from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout


class QuickHeapTestPackage(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires(self.tested_reference_str, headers=True, libs=True, run=True)

    def build_requirements(self):
        self.tool_requires("cmake/[>=4.0]")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        CMake(self).ctest()
