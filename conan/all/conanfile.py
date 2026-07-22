import os

from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.files import copy, get

required_conan_version = ">=2.10.0"


class PiperRecipe(ConanFile):
    name = "piper"
    description = "Visual designer for control-system pipelines (core + engine libraries)"
    license = "CECILL-C"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://github.com/leducp/Piper"
    topics = ("control-systems", "node-editor", "dataflow", "visual-programming")
    package_type = "library"

    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    def export_sources(self):
        # Local development: copy the sources from the project root (two levels
        # up from conan/all/) into the recipe. Released builds fetch a tagged
        # tarball via source() instead.
        root = os.path.abspath(os.path.join(self.recipe_folder, "../.."))
        copy(self, "CMakeLists.txt", src=root, dst=self.export_sources_folder)
        copy(self, "cmake/*",  src=root, dst=self.export_sources_folder)
        copy(self, "core/*",   src=root, dst=self.export_sources_folder,
             excludes=["**/tests/**", "**/examples/**", "**/py_bindings/**"])
        copy(self, "engine/*", src=root, dst=self.export_sources_folder,
             excludes=["**/tests/**", "**/examples/**", "**/py_bindings/**"])
        copy(self, "LICENCE",  src=root, dst=self.export_sources_folder)

    def source(self):
        # Fetches a tagged source tarball when building a released version.
        # get(self, **self.conan_data["sources"][self.version], strip_root=True)
        pass

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def validate(self):
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, 20)

    def requirements(self):
        self.requires("nlohmann_json/3.11.3")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_APP"]         = False
        tc.cache_variables["BUILD_PY_BINDINGS"] = False
        tc.cache_variables["BUILD_TESTS"]       = False
        tc.cache_variables["BUILD_EXAMPLES"]    = False
        tc.cache_variables["CMAKE_CI_BUILD"]    = True
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        # No install() rules exist for core/engine, so hand-copy the public
        # headers and the static archives.
        for subdir in ("core/include", "engine/include"):
            copy(self, "*.h",
                 os.path.join(self.source_folder, subdir),
                 os.path.join(self.package_folder, "include"))
        for pattern in ("*.a", "*.so", "*.so.*", "*.dylib"):
            copy(self, pattern,
                 self.build_folder,
                 os.path.join(self.package_folder, "lib"),
                 keep_path=False)
        copy(self, "LICENCE",
             src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "piper")

        c = self.cpp_info.components

        c["piper_core"].libs = ["piper_core"]
        c["piper_core"].requires = ["nlohmann_json::nlohmann_json"]
        c["piper_core"].set_property("cmake_target_name", "piper::piper_core")

        c["piper_engine"].libs = ["piper_engine"]
        c["piper_engine"].requires = ["piper_core"]
        c["piper_engine"].set_property("cmake_target_name", "piper::piper_engine")
