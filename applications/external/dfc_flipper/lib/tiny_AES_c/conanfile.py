# SPDX-FileCopyrightText: kokke
# SPDX-FileCopyrightText: Mistial Dev
# SPDX-License-Identifier: Unlicense

from conans import ConanFile, CMake
from conans.errors import ConanException


class TinyAesCConan(ConanFile):
    name = "tiny-AES-c"
    version = "1.0.0"
    license = "The Unlicense"
    url = "https://github.com/kokke/tiny-AES-c"
    description = "Small portable AES128/192/256 in C"
    topics = ("encryption", "crypto", "AES")
    settings = "os", "compiler", "build_type", "arch"

    generators = "cmake"
    exports_sources = ["CMakeLists.txt", "*.c", "*.h", "*.hpp"]
    exports = ["unlicense.txt"]

    _options_dict = {
        "AES128": [True, False],
        "AES192": [True, False],
        "AES256": [True, False],
        "AES_ENABLE_CBC": [True, False],
        "AES_ENABLE_ECB": [True, False],
        "AES_ENABLE_CTR": [True, False],
        "AES_ENABLE_OFB": [True, False],
        "AES_ENABLE_GCM": [True, False],
        "AES_ENABLE_CCM": [True, False],
        "AES_ENABLE_EAX": [True, False],
        "AES_ENABLE_EAX_PRIME": [True, False],
        "AES_ENABLE_SIV": [True, False],
        "AES_ENABLE_CMAC": [True, False],
        "AES_ZEROIZE": [True, False],
        "AES_STRICT": [True, False],
        "AES_TINY": [True, False],
        "AES_WIDE_OPS": [True, False],
    }

    options = _options_dict

    default_options = {
        "AES128": True,
        "AES192": False,
        "AES256": False,
        "AES_ENABLE_CBC": False,
        "AES_ENABLE_ECB": False,
        "AES_ENABLE_CTR": True,
        "AES_ENABLE_OFB": False,
        "AES_ENABLE_GCM": False,
        "AES_ENABLE_CCM": False,
        "AES_ENABLE_EAX": False,
        "AES_ENABLE_EAX_PRIME": False,
        "AES_ENABLE_SIV": False,
        "AES_ENABLE_CMAC": False,
        "AES_ZEROIZE": True,
        "AES_STRICT": False,
        "AES_TINY": False,
        "AES_WIDE_OPS": False,
    }

    def configure(self):
        modes = [
            self.options.AES_ENABLE_CBC,
            self.options.AES_ENABLE_ECB,
            self.options.AES_ENABLE_CTR,
            self.options.AES_ENABLE_OFB,
            self.options.AES_ENABLE_GCM,
            self.options.AES_ENABLE_CCM,
            self.options.AES_ENABLE_EAX,
            self.options.AES_ENABLE_EAX_PRIME,
            self.options.AES_ENABLE_SIV,
            self.options.AES_ENABLE_CMAC,
        ]
        if not any(modes):
            raise ConanException("Need to at least specify one operation mode")

        if (
            not self.options.AES128
            and not self.options.AES192
            and not self.options.AES256
        ):
            raise ConanException(
                "Need to at least specify one of AES{128, 192, 256} modes"
            )

    def build(self):
        cmake = CMake(self)
        for key in self._options_dict.keys():
            val = self.options[key]
            if key.startswith("AES_ENABLE_"):
                cmake.definitions[f"TINY_{key}"] = "ON" if val else "OFF"
            elif key in ["AES_ZEROIZE", "AES_STRICT", "AES_TINY", "AES_WIDE_OPS"]:
                cmake.definitions[f"TINY_{key}"] = "ON" if val else "OFF"

        cmake.configure()
        cmake.build()

    def package(self):
        self.copy("*.h", dst="include")
        self.copy("*.hpp", dst="include")
        self.copy("*.a", dst="lib", keep_path=False)
        self.copy("unlicense.txt")

    def package_info(self):
        self.cpp_info.libs = ["tiny-aes"]
