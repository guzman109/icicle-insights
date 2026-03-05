import platform
import subprocess
from conan import ConanFile


class LibpqSystem(ConanFile):
    name = "libpq"
    version = "system"
    package_type = "shared-library"

    def package_id(self):
        # System package — same binary for all build configurations.
        self.info.clear()

    def package_info(self):
        if platform.system() == "Darwin":
            # Homebrew installs libpq keg-only; resolve the prefix explicitly
            # so the linker can find it without a manual LIBRARY_PATH export.
            result = subprocess.run(
                ["brew", "--prefix", "libpq"],
                capture_output=True, text=True
            )
            prefix = result.stdout.strip()
            if prefix:
                self.cpp_info.components["pq"].libdirs = [f"{prefix}/lib"]
                self.cpp_info.components["pq"].includedirs = [f"{prefix}/include"]
            self.cpp_info.components["pq"].libs = ["pq"]
        else:
            # On Linux (Alpine/Debian), libpq is in standard system paths.
            self.cpp_info.components["pq"].system_libs = ["pq"]
