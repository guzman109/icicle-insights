from conan import ConanFile


class ICICLEInsights(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("asio/1.36.0")
        self.requires("libpqxx/7.10.5")
        self.requires("spdlog/1.17.0")
        # Use the system-installed libpq instead of building it (and its
        # transitive OpenSSL dependency) from source.
        self.requires("libpq/system", override=True)
