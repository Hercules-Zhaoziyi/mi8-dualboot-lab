# SPDX-License-Identifier: MIT
# UI application build only. No FDF, platform firmware image or OS payload.
[Defines]
  VENDOR_NAME = Xiaomi
  PLATFORM_NAME = HerculesBootUI
  PLATFORM_GUID = 67f42a53-5c20-485f-8b3a-73303fd55937
  PLATFORM_VERSION = 0.1.0
  DSC_SPECIFICATION = 0x00010019
  OUTPUT_DIRECTORY = Build/HerculesBootUI
  SUPPORTED_ARCHITECTURES = AARCH64
  BUILD_TARGETS = DEBUG|RELEASE
  SKUID_IDENTIFIER = DEFAULT

!include Platform/Qualcomm/sdm845/sdm845.dsc

[BuildOptions.common]
  GCC:*_*_AARCH64_CC_FLAGS = -DENABLE_SIMPLE_INIT -DENABLE_LINUX_SIMPLE_MASS_STORAGE -DHERCULES_EXTERNAL_UI_FONTS=1 -ffile-prefix-map=$(HERCULES_SI_ROOT)=simple-init -ffile-prefix-map=$(HERCULES_PORT_ROOT)=edk2-port

[PcdsFixedAtBuild.common]
  gQcomTokenSpaceGuid.PcdMipiFrameBufferWidth|1080
  gQcomTokenSpaceGuid.PcdMipiFrameBufferHeight|2248
  gSimpleInitTokenSpaceGuid.PcdGuiDefaultDPI|410
  gRenegadePkgTokenSpaceGuid.PcdDeviceVendor|"Xiaomi"
  gRenegadePkgTokenSpaceGuid.PcdDeviceProduct|"MI 8"
  gRenegadePkgTokenSpaceGuid.PcdDeviceCodeName|"dipper"

[Components.common]
  Platform/RenegadePkg/Application/Reboot2PayloadApp/Reboot2PayloadApp.inf
