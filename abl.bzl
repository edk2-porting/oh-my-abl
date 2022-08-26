load("//build/kernel/kleaf:directory_with_structure.bzl", dws = "directory_with_structure")
load("//build/kernel/kleaf/impl:common_providers.bzl", "KernelEnvInfo")
load("//msm-kernel:target_variants.bzl", "get_all_variants")
load(
    "//build/bazel_extensions:abl_extensions.bzl",
    "extra_build_configs",
    "extra_function_snippets",
    "extra_post_gen_snippets",
    "extra_srcs",
)

def _abl_impl(ctx):
    inputs = []
    inputs += ctx.files.srcs
    inputs += ctx.attr.kernel_build[KernelEnvInfo].dependencies

    output_files = [ctx.actions.declare_file("{}.tar.gz".format(ctx.label.name))]

    command = ctx.attr.kernel_build[KernelEnvInfo].setup
    command += """
      set -o errexit
      ABL_OUT_DIR=${PWD}/bootable/bootloader/edk2/out

      abl_image_generate() {
        PREBUILT_HOST_TOOLS="BUILD_CC=clang BUILD_CXX=clang++ LDPATH=-fuse-ld=lld BUILD_AR=llvm-ar"

        MKABL_ARGS=("-C" "${ROOT_DIR}/${ABL_SRC}")
        MKABL_ARGS+=("BOOTLOADER_OUT=${ABL_OUT_DIR}/obj/ABL_OUT" "all")
        MKABL_ARGS+=("BUILDDIR=${ROOT_DIR}/${ABL_SRC}")
        MKABL_ARGS+=("PREBUILT_HOST_TOOLS=${PREBUILT_HOST_TOOLS}")
        MKABL_ARGS+=("${MAKE_FLAGS[@]}")
        MKABL_ARGS+=("CLANG_BIN=${ROOT_DIR}/${CLANG_PREBUILT_BIN}/")

        echo "MAKING"
        make "${MKABL_ARGS[@]}"
        echo "MADE"

        ABL_DEBUG_FILE="$(find "${ABL_OUT_DIR}" -name LinuxLoader.debug)"
        if [ -e "${ABL_DEBUG_FILE}" ]; then
          cp "${ABL_DEBUG_FILE}" "${ABL_IMAGE_DIR}/LinuxLoader_${TARGET_BUILD_VARIANT}.debug"
          cp "${ABL_OUT_DIR}/unsigned_abl.elf" "${ABL_IMAGE_DIR}/unsigned_abl_${TARGET_BUILD_VARIANT}.elf"
        fi
      }
    """
    for snippet in ctx.attr.extra_function_snippets:
        command += snippet

    command += """
      echo "========================================================"
      echo " Building abl"

      [ -z "${{ABL_SRC}}" ] && ABL_SRC=bootable/bootloader/edk2

      if [ ! -d "${{ROOT_DIR}}/${{ABL_SRC}}" ]; then
        echo "*** STOP *** Please check the edk2 path: ${{ROOT_DIR}}/${{ABL_SRC}}"
        exit 1
      fi

      for extra_config in {extra_build_configs}; do
        source "${{extra_config}}"
      done

      source "${{ABL_SRC}}/QcomModulePkg/{build_config}"

      [ -z "${{ABL_OUT_DIR}}" ] && ABL_OUT_DIR=${{COMMON_OUT_DIR}}

      [ -z "${{TARGET_BUILD_VARIANT}}" ] && TARGET_BUILD_VARIANT=userdebug

      ABL_OUT_DIR=${{ABL_OUT_DIR}}/abl-${{TARGET_BUILD_VARIANT}}
      ABL_IMAGE_NAME=abl_${{TARGET_BUILD_VARIANT}}.elf

      [ -z "${{ABL_IMAGE_DIR}}" ] && ABL_IMAGE_DIR=${{DIST_DIR}}
      [ -z "${{ABL_IMAGE_DIR}}" ] && ABL_IMAGE_DIR=${{ABL_OUT_DIR}}
      mkdir -p "${{ABL_IMAGE_DIR}}"

      abl_image_generate
    """.format(
        extra_build_configs = " ".join(ctx.attr.extra_build_configs),
        build_config = ctx.attr.abl_build_config,
    )

    for snippet in ctx.attr.extra_post_gen_snippets:
        command += snippet

    command += """
      # Copy to bazel output dir
      abs_out_dir="${{PWD}}/{abl_out_dir}"
      mkdir -p "${{abs_out_dir}}"
      cd "${{ABL_IMAGE_DIR}}" && tar -czf "${{abs_out_dir}}/{abl_out_name}" ./*.elf
      """.format(
        abl_out_dir = output_files[0].dirname,
        abl_out_name = output_files[0].basename,
        rule_ext = ctx.label.name,
    )

    ctx.actions.run_shell(
        mnemonic = "Abl",
        inputs = inputs,
        outputs = output_files,
        command = command,
        progress_message = "Building {}".format(ctx.label),
    )

    return [
        DefaultInfo(
            files = depset(output_files),
        ),
    ]

abl = rule(
    implementation = _abl_impl,
    doc = """Generates a rule that builds the Android Bootloader (ABL)

    Example:
    ```
    abl(
        name = "kalama_gki_abl",
        srcs = glob(["**"])
        abl_build_config = "build.config.msm.kalama",
        kernel_build = "//msm-kernel:kalama_gki",
    )
    ```

    Args:
        name: Name of the abl target
        srcs: Source files to build the abl elf. If unspecified or value
          is `None`, it is by default a full glob of the directory:
          ```
          glob(["**"])
          ```
        kernel_build: Label referring to the kernel_build module.
        abl_build_config: ABL build config
        extra_function_snippets: list of additional shell functions to define at the top of
          the build script
        extra_post_gen_snippets: list of additional shell commands to run at the end of
          the build script
        extra_build_configs: list of additional build configs to source prior to building
        kwargs: Additional attributes to the internal rule, e.g.
          [`visibility`](https://docs.bazel.build/versions/main/visibility.html).
          See complete list
          [here](https://docs.bazel.build/versions/main/be/common-definitions.html#common-attributes).
    """,
    attrs = {
        "srcs": attr.label_list(
            mandatory = True,
            allow_files = True,
        ),
        "abl_build_config": attr.string(),
        "extra_function_snippets": attr.string_list(),
        "extra_post_gen_snippets": attr.string_list(),
        "extra_build_configs": attr.string_list(),
        "kernel_build": attr.label(
            mandatory = True,
            providers = [KernelEnvInfo],
        ),
    },
)

def define_abl_targets():
    for target, variant in get_all_variants():
        define_abl(target, variant)

def define_abl(msm_target, variant):
    target = msm_target + "_" + variant

    abl(
        name = "{}_abl".format(target),
        kernel_build = "//msm-kernel:{}_env".format(target),
        abl_build_config = "build.config.msm.{}".format(msm_target),
        srcs = native.glob(["**"]) + extra_srcs,
        extra_function_snippets = extra_function_snippets,
        extra_post_gen_snippets = extra_post_gen_snippets,
        extra_build_configs = extra_build_configs,
        visibility = ["//visibility:public"],
    )
