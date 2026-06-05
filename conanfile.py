import os

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps
from conan.tools.files import copy


class PiperPackage(ConanFile):
    name = "piper"
    description = "Visual designer for control-system pipelines (core + engine libraries)"
    license = "CECILL-C"
    settings = "os", "compiler", "build_type", "arch"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    def export_sources(self):
        root = self.recipe_folder
        copy(self, "CMakeLists.txt", root, self.export_sources_folder)
        copy(self, "cmake/*",        root, self.export_sources_folder)
        copy(self, "core/*",         root, self.export_sources_folder,
             excludes=["**/tests/**", "**/examples/**", "**/py_bindings/**"])
        copy(self, "engine/*",       root, self.export_sources_folder,
             excludes=["**/tests/**", "**/examples/**", "**/py_bindings/**"])

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_APP"]           = False
        tc.cache_variables["BUILD_PY_BINDINGS"]   = False
        tc.cache_variables["BUILD_TESTS"]         = False
        tc.cache_variables["BUILD_EXAMPLES"]      = False
        tc.cache_variables["CMAKE_CI_BUILD"]      = True
        tc.cache_variables["CONAN_PACKAGE_BUILD"] = True
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # No install() rules exist for core/engine, so hand-copy the
        # public headers and the static archives
        for subdir in ("core/include", "engine/include"):
            copy(self, "*.h",
                 os.path.join(self.source_folder, subdir),
                 os.path.join(self.package_folder, "include"))
            copy(self, "*.tpp",
                 os.path.join(self.source_folder, subdir),
                 os.path.join(self.package_folder, "include"))
        copy(self, "*.a",
             self.build_folder,
             os.path.join(self.package_folder, "lib"),
             keep_path=False)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "piper")

        c = self.cpp_info.components

        c["piper_core"].libs = ["piper_core"]
        c["piper_core"].requires = ["nlohmann_json::nlohmann_json"]
        c["piper_core"].set_property("cmake_target_name", "piper::piper_core")

        c["piper_engine"].libs = ["piper_engine"]
        c["piper_engine"].requires = ["piper_core"]
        c["piper_engine"].set_property("cmake_target_name", "piper::piper_engine")
