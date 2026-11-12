# 安装、备份与集成

## 文件类型

| 文件 | 用途 |
| --- | --- |
| `HerculesBootMenu.efi` | AArch64 UEFI 应用，提供菜单 |
| `HerculesBootMenu.ffs` | 同一应用的 FFS 封装，文件 GUID 为 `6d77b2bb-69eb-42ab-becf-4f40c89568c3` |
| `AndroidHandoff.efi/.ffs` | 配合匹配双启动 BootShim 使用的 Android 交接模块 |
| 本机生成的字库 RAW FFS | UI 从固件卷读取的私有本地输入，不在 Release 内 |
| 自行组装的手机 `boot.img` | 含平台固件、原 Android 载荷和自备系统数据的本机整合产物，不在 Release 内 |

**不要把 `.efi` 或 `.ffs` 直接刷到手机 boot 分区。** 本仓库不提供可直接刷入的完整 iOS 整包。

## 先具备可恢复环境

适用目标为小米 8 `dipper`、已解锁 bootloader、有可用 USB 调试/FASTBOOT 和经过核验的恢复环境。其他容量、版本或设备不得套用本机分区数值。本项目不自动解锁、重分区或格式化。

在 TWRP 等已确认的 root 恢复环境中，先备份原 boot、recovery、分区表和现有数据。示例仅展示命名分区读取：

```powershell
adb shell "dd if=/dev/block/by-name/boot of=/tmp/boot-before.img bs=1048576"
adb pull /tmp/boot-before.img local/backup/boot-before.img
adb shell "dd if=/dev/block/by-name/recovery of=/tmp/recovery-before.img bs=1048576"
adb pull /tmp/recovery-before.img local/backup/recovery-before.img
Get-FileHash local/backup/boot-before.img -Algorithm SHA256
```

命令前必须核对设备身份、分区别名和容量，并比较设备端与主机端 SHA256。备份保存在 `local/`，不要提交仓库。已有 iOS 存档还需根据**你自己的布局**读取并校验头部和内容 CRC；不要复制旧实验中的固定 LBA。

## 推荐：源码集成

1. 按 [BUILD](BUILD.md) 构建纯 UI 组件。
2. 按 [FONTS](FONTS.md) 在本机生成同款字体 RAW FFS；不要上传这些文件。
3. 在自备的兼容 edk2-sdm845 固件中集成 UI 应用和两个字体资源。应用 GUID、Android 交接 GUID和本机 iOS 加载器入口必须与配置匹配。
4. 保留你自己的原 Android 内核、DTB 和 ramdisk。Android 交接模块依赖匹配的双启动 BootShim；单独放入交接模块不能替代该引导链。
5. iOS 加载器、内核和系统材料从合法来源自行准备，参考所选版本的 [dipper-xnu](https://github.com/youzjuzi/dipper-xnu) 文档。本仓库不下载或打包这些材料。
6. 在本机组装完整 boot 后检查 Android 头、解压保留区大小、64 MiB 等**实际目标容量**以及全部输入摘要，再进行 RAM 临时启动。

FFS 文件需按宿主 FDF 的压缩和固件卷布局集成，不能用十六进制覆盖文件来替代重新封装。Release 的 FFS 是组件文件，不代表所有固件卷都能直接容纳原始文件尺寸。

## 临时验证与正式写入

先用自己的完整测试镜像进行 `fastboot boot local/test-boot.img`，验证：无操作倒计时、取消计时、图标、原生文字、高级返回、Android 和 iOS、原数据恢复。此步骤不改变正式 boot。

只有临时启动通过、最新用户数据和原 boot 已完整备份后，再进行仅针对 boot 的受保护写入。写入流程应具备：设备身份/别名/容量校验、当前 boot 与回退文件摘要、候选摘要、恢复分区和 GPT 前后校验、候选全长回读以及验证失败自动恢复。不要使用来源不明的一键分区或 `flash_all_lock` 脚本。

本机实际过程按此原则完成了回读验证，但私人整合镜像和绑定私人设备的写入脚本不作为通用安装器发布。

## 回退

使用自己保存的、哈希核验过的原 boot 恢复引导；这不等于恢复全部分区或删除 iOS 数据。分区被改动时需要独立的数据和 GPT 恢复计划。回退前仍应保持可用 FASTBOOT/TWRP 入口，不能盲目恢复其他设备的备份。


固件卷集成还需 `HerculesBootConfig.ffs`（GUID `4b0364cf-1c5b-47aa-9073-d7b5039ce49b`）；它封装公开的 `simpleinit.static.uefi.cfg`，用于启用同款界面、10 秒默认 Android 和两个入口。单独复制 UI 应用不会自动加载这些配置。

源码 FDF 资源示例见 `source/edk2-integration/UI.fdf.inc.example` 与 `Fonts.private.fdf.inc.example`。使用二进制 FFS 时请由兼容宿主的固件卷打包流程加入对应 GUID 文件，不与同 GUID 的 INF/FILE 重复加入。不要将完整内核/数据载荷交给 UI 组件打包工具。

源码 FDF 资源示例见 `source/edk2-integration/UI.fdf.inc.example` 与 `Fonts.private.fdf.inc.example`。使用二进制 FFS 时请由兼容宿主的固件卷打包流程加入对应 GUID 文件，不与同 GUID 的 INF/FILE 重复加入。不要将完整内核/数据载荷交给 UI 组件打包工具。
