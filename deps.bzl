#
# Copyright 2020 the authors listed in CONTRIBUTORS.md
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
load("@rules_foreign_cc//:workspace_definitions.bzl", "rules_foreign_cc_dependencies")

def pir_deps():

    if "com_microsoft_gsl" not in native.existing_rules():
        http_archive(
            name = "com_microsoft_gsl",
            sha256 = "d3234d7f94cea4389e3ca70619b82e8fb4c2f33bb3a070799f1e18eef500a083",
            build_file = "//third_party:gsl.BUILD",
            strip_prefix = "GSL-3.1.0/include",
            urls = ["https://github.com/microsoft/GSL/archive/v3.1.0.tar.gz"],
        )

    if "com_microsoft_seal" not in native.existing_rules():
        http_archive(
            name = "com_microsoft_seal",
            build_file = "//third_party:seal.BUILD",
            strip_prefix = "SEAL-3.7.2",
            sha256 = "12676de5766b8e2d641d6e45e92114ccdf8debd6f6d44b42a2ecc39a59b0bf13",
            urls = ["https://github.com/microsoft/SEAL/archive/v3.7.2.tar.gz"],
        )
        
    rules_proto_dependencies()

    rules_proto_toolchains()

    rules_foreign_cc_dependencies()
