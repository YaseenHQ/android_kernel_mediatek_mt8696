# Building

Toolchain: clang r416183b (LineageOS prebuilt
android_prebuilts_clang_kernel_linux-x86_clang-r416183b), GNU make 4.3
(make 4.4 breaks 5.10 Kconfig), aarch64-linux-gnu binutils, LLVM tools.

    export PATH=<clang>/bin:$PATH
    make O=out CC=clang LD=ld.lld NM=llvm-nm OBJCOPY=llvm-objcopy \
         LLVM_IAS=1 olddefconfig     # .config from arch/arm64/configs/karat_lineage.config
    make O=out CC=clang ... -j8 Image.gz dtbs modules

CC must be passed as a make variable (env-only export loses
CONFIG_CC_IS_CLANG). The base config is the extracted live
configuration of the RS8185a kernel. Boot image geometry: header v2,
base 0x40078000, kernel 0x00008000, ramdisk 0x14f88000, tags/dtb
0x13f88000, page 2048, cmdline bootopt=64S3,32N2,64N2
androidboot.boot_devices=soc/11230000.mmc.
