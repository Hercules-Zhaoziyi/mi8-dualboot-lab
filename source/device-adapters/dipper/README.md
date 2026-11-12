# Mi8-specific adapter source

BootShim.Dualboot.S is the actual dipper boot selection shim from the edk2-sdm845 derivative. Its BSD-family upstream notices are in LICENSES/edk2-sdm845.txt. It reads the warm-reset magic written by AndroidHandoff, then either enters the original Android kernel or copies the UEFI region to its configured base. It contains no Android kernel or iOS payload.

The magic address 0x17FE00000, branch patching, UEFI_BASE and UEFI_SIZE are specific to the reviewed Mi8 memory map. Never reuse on another target without a reserved-memory and reset-survival analysis. Build with the pinned platform BootShim assembler/linker flow; inspect tools/BootShim and the platform build script for symbol definitions. Retain matching kernelpatcher source from that fixed upstream tree. The local composer calls the user-built patcher; it does not download a prebuilt executable.

The composer supports only the experimentally used legacy Mi8 Android boot layout, 4096-byte page, no second stage or appended recovery DTBO, gzip ARM64 kernel with appended DTB and a 7 MiB firmware volume. Other formats/AVB/slots require a different adapter. No device writes are performed.
