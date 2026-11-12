# Hercules Mi8 Dual-Boot Lab

Native-text light boot UI, reproducible UI firmware components, and an honest experiment journal for Xiaomi Mi 8 (`dipper`). See the [Chinese documentation](README.md) for the full record.

The approved layout and original Android, Apple and Xiaohongshu SVG files are preserved. The UI has consistent advanced pages, a small shared credit row, and a ten-second Android default that is cancelled for the current boot by the first key or touch input.

Releases contain AArch64 EFI applications and FFS components, configuration, checksums and corresponding UI source. They do not contain iOS, Apple OS/kernel payloads, complete phone boot images, IPAs, SDKs, private logs, credentials or proprietary UI font data. Locally supplied font RAW FFS resources preserve the approved typography; a replacement-font design is not shipped as the same UI.

EFI/FFS files are integration components, not files to flash directly to a phone boot partition. Read [INSTALL](docs/INSTALL.md), [FONTS](docs/FONTS.md), [BUILD](docs/BUILD.md), [VALIDATION](docs/VALIDATION.md) and the per-component license notices.

The private integrated build was device-tested. The public external-font packaging is module-built and host-tested, not reflashed to the maintainer's phone. Large writable iOS storage, half-disk migration, software shutdown, full audio and camera support are not claimed as implemented.
