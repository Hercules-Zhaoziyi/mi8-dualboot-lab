保留定稿浅色玻璃界面、原始 Android / Apple / 小红书 SVG、原生文字、低调页脚与统一高级页面。

- 10 秒未操作默认 Android；第一次按键/触控取消本次倒计时，返回不会重启计时。
- 修复图标裁切与遮挡、高级选项残留勾选/蓝框、多层返回和页面生命周期问题。
- 包含 AArch64 `HerculesBootMenu.efi/.ffs`、`AndroidHandoff.efi/.ffs`、`HerculesBootConfig.ffs`、配置、对应源码、安装/构建/使用说明和 SHA256。
- 三个 SVG 原件保留；没有用字体图标替换。字体数据由用户在本机生成 RAW FFS 资源，公开文件不内置 Windows 字库。详见仓库 docs/FONTS.md。

**这是实验性 UI 固件组件，不是可直接刷入 boot 分区的完整镜像。** 需要兼容 edk2-sdm845 / BootShim、本地字体资源和自行合法准备的系统输入。不得把 EFI/FFS 直接当 boot.img 刷写。

公开源码已重建并通过实际 LVGL / FreeType / NanoSVG 与 ASAN 主机回归；私人完整整合版经持有人实机确认。公开外部字体封装未重新刷机测试，故作为 prerelease 发布。未实现数十 GB 可写扩容、iOS 滑动关机修复或蓝牙耳机音频。

不含 Apple 专有系统镜像/内核二进制/库、IPA、用户数据、账号、设备标识或私有字库。保留依法要求的开源版权通知和用户指定商标图形。
