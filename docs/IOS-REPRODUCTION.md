# 从源码到小米 8 iOS 刷入的实现路线

本页补齐系统移植工程与菜单工程之间的关系。真正的 iOS 启动依赖 dipper-xnu 的 XNU/驱动、xnuloader、用户空间及平台 UEFI；本仓库的 UI 只负责选择引导路径。全流程需要使用者合法自备的系统/SDK、原机固件和字库，本仓库不托管这些文件。

## 1. 获取固定复刻工程

```bash
python3 tools/agent_preflight.py
python3 tools/fetch_ios_reference.py --out local/ios-reference
cd local/ios-reference
sha256sum -c SHA256SUMS
python3 tools/prepare/prepare.py
```

最后一条只列出上游及固定版本，不下载或写机。固定参考提交见 UPSTREAM_SOURCES.json；它是已有公开构建教程和补丁的来源，不是本项目原创内核。阅读其 `docs/build/BUILD.md`、`INPUTS.md`、`USERLAND.md` 和 `docs/install/DEVICE_PREPARATION.md`，准备匹配版本的工具链、iPhoneOS SDK、系统输入及原机 GPU/Wi-Fi/蓝牙固件。按清单逐项核对摘要，不能随机换同名文件。工具不会代替用户提供授权材料。

准备好依赖后，在该工程执行：

```bash
python3 tools/prepare/prepare.py --prepare
```

这会下载固定源码并应用其补丁，包括合法开源的 XNU/HackiOS 基线；不代表系统镜像已准备好。根据该提交 BUILD 教程编译其各内核依赖和内核，再按 USERLAND 教程生成用户空间，不能跳过中间步骤直接运行下列末端命令。`tools/build/` 中真实实现入口包括 `build-userland.sh`、`pack-darwin.py`、`build-loader-uefi.sh` 和 `build-payload.py`；先读源码及 `--help`，它们的参数以固定版本为准。

## 2. 核对设备、备份、首次布局

只对已核验的 dipper 使用本节。确认 BL 解锁、有稳定 USB、已有可用 recovery，备份当前 Android 启动组件和用户数据。Android 普通 shell 没有权限时使用已经获得授权的 root/recovery，不把 Permission denied 当成工具成功。

首次安装阅读上游 `docs/install/INSTALL.md` 和 `PARTITION_DARWIN.md`；已有安装阅读 `EXISTING_INSTALL.md`，优先校验并保留布局。上游 `tools/partition/partition-darwin.py`、`restore-android-layout.py` 是针对特定布局的实现，必须先检查其前提及 dry-run。不能把旧机的固定 LBA、4096 字节假设或分区号直接用于新机，不能复制清零存档示例作为常规安装步骤。

Agent 应记录：物理 LUN、逻辑扇区大小、GPT 主/备 CRC、每个相关分区的起止/类型/唯一 GUID、挂载状态、实际容量；sysfs 的 start/size 以 512 字节为单位，不等于 GPT 逻辑扇区。生成可审阅的布局和备份报告后，才执行用户授权的分区/写入。读回镜像范围并在主机和设备端比较 SHA256；保留分区尾部有效存档。

## 3. 让 loader、内核和写入器匹配实际布局

上游 `docs/install/STARTUP.md` 列明具体同步位置：

- `work/hackios/xnuloader/loader.c` 的 `BLOCK_DARWIN_FIRST/LAST/TYPE/UNIQ`；
- `work/hackios/xnu-src/xnu-8796.121.2/bsd/dev/redwood_ufs.c` 的 `UFS_DARWIN_FIRST/BLOCKS` 和写入边界；
- 如使用分区写入工具，同步它的节点、GUID、逻辑扇区、范围和保护区。

私有 GUID 不写回公共仓库；GUID 字符串与 C 字段/磁盘原始字节序要正确转换。边界变化必须重建相应内核与 loader；只改唯一 GUID 也必须重建 loader 和包含它的 UEFI。不要取消身份/范围校验来“兼容”。

## 4. 构建与主机临时引导

在上游工程按完整 BUILD/USERLAND 教程完成前置构建后：

```bash
bash tools/build/build-userland.sh
bash tools/build/build-loader-uefi.sh
python3 tools/build/build-payload.py --output work/runs/first-readonly --seconds 300
(cd work/runs/first-readonly && sha256sum -c SHA256SUMS)
```

新目录必须不存在。先做不写盘、不恢复旧状态的基础桌面测试；需要网络等功能时按该版本帮助和教程加入 `--wifi` / `--peripherals` / `--bluetooth`，不要凭猜测开启全部开关。

严格按该提交 STARTUP 中的 **RAM 启动顺序和输出文件名**执行 `fastboot boot`、loader 载荷传输及运行命令，不把 UI `.efi/.ffs` 当手机镜像。xnuloader 的传输与 Android fastboot 是两个阶段；出现 `XNULOADER usb: started` 仅说明等待载荷，不等于系统挂起。loader 就绪后，该版本提供的只读取证入口包括 `fastboot getvar product/result`、`fastboot oem log` 和 `fastboot get_staged`。完整保存日志并确认实际桌面/触控。轮次之间完整重启，避免 GPU/基带未恢复的状态直接 rerun。

## 5. 从主机引导到本机双系统

本仓库已公开 `source/simple-init`、UI 固件、配置及 Android 交接模块，但不提供带系统载荷的最终 boot。先按 BUILD/FONTS 生成本地字库和菜单组件，再按 INSTALL 的 FDF 示例集成到**匹配的宿主 UEFI**。保留原 Android 内核、DTB、ramdisk 与格式，正确匹配 BootShim 的 Android 交接协议、载荷放置和预留内存；`AndroidHandoff` 中的 Mi8 物理地址不能跨机型复用。

本机自动启动 iOS 的整合还必须包含可用的本地 loader、内核/设备树/启动参数及存储读入路径，不能停留在只等待 USB 的 loader。为每个输入记录摘要和布局，核对压缩/解压体积、BootShim/FD/载荷偏移、Android boot header、保留区和目标分区真实大小。当前公开 UI 组件不自动生成这个系统专用组合；agent 必须依据固定上游及目标布局实现并验证，不能宣称一条通用打包命令已覆盖所有目标。

先 RAM 启动组合镜像，验证菜单 Android/iOS、返回、倒计时和数据；再按 owner 授权，仅写实际引导分区。A/B、vendor_boot/init_boot 等设备必须建立自己的槽位方案，不能套 Mi8 单 boot 写法。保留候选与旧镜像，写后完整回读比较；同时核验 recovery 与 GPT 未改。最后断线冷启动验收两个系统。

## 6. 数据与真实限制

启用持久数据写入前，先核对该构建的写入白名单、存档版本、双槽位置/上限、CRC 和头部最后提交规则。用新存档验证中文/外观/Wi-Fi、照片和应用保留；旧存档导致桌面异常时备份后隔离分析，不能直接覆盖最新有效槽。强制断电可能丢失尚未提交的变化。当前实现的 `/private/var` 仍是有界 RAMFS，并不是几十 GB 闪存可写卷。

Wi-Fi 可连接不等于可上网；需实际网页刷新。蓝牙连接不等于有音频；相机、滑动关机等问题仍见 STATUS。安装与适配过程中的失败定位、实际本机结果见 HISTORY、EXPERIMENTS、VALIDATION。固定上游的个别教程是早期阶段记录，应结合本仓库现状及所选源码判断，不能把历史“未测”自动改成“通过”。

## 本机组合镜像实现源码

`source/device-adapters/dipper/BootShim.Dualboot.S` 与 `source/edk2-integration/Reboot2PayloadApp.c` 提供实际 Mi8 warm-reset 交接配对源码。`tools/compose_dipper_boot.py` 是将本机打包步骤参数化的实现：保留原 Android kernel/DTB/ramdisk，调用从固定平台源码构建的 kernelpatcher，补齐 ARM64 解压预留长度，追加自备 UEFI 与自动启动载荷，去掉 USB 等待记录，再压缩回 legacy Android boot。所有生成物只进 local/，工具不接触手机。

```bash
python3 tools/compose_dipper_boot.py --device dipper \
 --original-boot local/backup/boot.img --original-sha256 <核验过的原镜像SHA256> \
 --firmware local/build/SDM845_UEFI.fd --shim local/build/BootShim.Dualboot.bin \
 --kernelpatcher local/build/kernelpatcher --payload local/build/daily-payload.bin \
 --boot-partition-bytes <实际读取的容量> --out local/candidate-01
```

仅支持明确核对的 Mi8 legacy header v0 布局。参数化工具增加了格式/容量拒绝条件，已做主机语法和负例检查，未用它重新刷机。原本机等价打包步骤有历史设备验收，但不能把该证据转移为新脚本的端到端设备认证。未知布局会拒绝；A/B 或其他 header 版本必须另实现适配器，不能删除断言继续运行。
