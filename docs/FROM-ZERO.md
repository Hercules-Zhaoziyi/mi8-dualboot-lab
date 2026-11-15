# 从零复刻：dipper 主机、材料、系统与双启动

适用目标：小米8 **dipper / SDM845**。不是 Mi8 SE、Lite、Pro 或其他 SDM845 手机的通用刷机包。本文是分阶段工程流程，不是已经在全新电脑上完整验收的一键安装器。菜单启动、iOS桌面和永久双启动是三项不同验收。

当前必须先读 [2026-11-14验证与启动回归](UPDATES-2026-11-14.md)：公开源码存在尚未完成正式版本集成的启动回归，不能直接把旧 Release 当作本文构建产物。不要从外观、手机代号或 fastboot OKAY 推定兼容。

## 0. Agent 执行顺序与目录

1. 先读取 README、AGENTS、本文、STATUS 和 UPDATES。记录本仓库提交、UPSTREAM_SOURCES.json 以及上游提交。
2. 自行检查已有文件与工具，能够匿名取得的固定公开依赖自行准备。不要反复泛问用户“有没有SDK/payload”。只有缺少具体、无法自动取得的输入时才问文件名、版本、路径与原因。
3. 不把 GitHub URL 当成本机路径，也不借用作者电脑的目录、镜像、分区 GUID、备份或校准文件。
4. 每条命令保存退出码；失败时保存现场和日志，不继续下一个写入阶段。不可自动把固定提交换成 main，不可降低校验条件。
5. 首次安装涉及清空 Android 数据；先完成具体备份、布局和可审阅方案，再按用户已经授权的范围执行。已有授权不重复索要笼统确认，涉及额外清空/回锁等新范围另行确认。

本文 Bash 构建在 Linux x86_64 中执行。建议 Ubuntu 26.04.1，与上游记录一致；全新系统的依赖安装未完成端到端回归。至少预留80GiB（建议更多，非实测最低），构建目录放Linux文件系统、无空格路径。

```text
mi8-dualboot-lab/                公共菜单工程，下文 LAB
  local/ios-reference/          固定上游，下文 DIPPER_PROJECT_ROOT
    local/inputs/               原始材料
    local/theos/                工具链和SDK
    work/                       一套活动构建树
    work/runs/<唯一轮次>/        候选、日志和摘要
  local/devices/<自定别名>/      私有配置
  local/backups/<轮次>/          本机备份
```

每阶段生成 local/runs/<id>/report.json，字段至少包括 stage、source_commits、inputs（来源/版本/绝对路径/摘要）、commands（命令/退出码/日志）、outputs（摘要）、device_evidence、unresolved。不公开原始报告中的设备身份和用户资料。

## 1. Windows11 / WSL 与 USB

管理员 PowerShell 中先检查 `wsl --status`、`wsl --list --online`。若无WSL，执行 `wsl --install`，按提示重启，再从当前列表选择适用Ubuntu发行版。不要假定所有电脑都已有同名发行版。可用 `wsl --manage <发行版> --move <其他盘路径>` 的前提是本机 `wsl --help` 支持；旧版本使用微软文档的导出/导入步骤，先备份，不删除旧实例直到新实例验收。

WSL用于构建。手机ADB/FASTBOOT优先使用Windows原生 [Google Platform Tools](https://developer.android.com/tools/releases/platform-tools)。解压后在其目录执行 `./adb.exe version` 和 `./fastboot.exe --version`；PowerShell必须使用 `./` 或绝对路径。不要将Windows的 .exe 交给Linux执行，也不要同时让Windows和WSL抢同一USB设备。

Android开启USB调试、解锁屏幕后允许本电脑。`adb devices -l` 中 unauthorized 需要手机授权，offline/空列表先检查线材、USB接口和驱动。FASTBOOT应为Android Bootloader接口，TWRP为ADB接口。Windows报“设备描述符请求失败”时先排查线/口/连接状态，不能直接强塞任意苹果驱动。不要批量卸载其他设备驱动。

USB转发只在明确选择Linux直连方案时使用 [usbipd-win](https://github.com/dorssel/usbipd-win)；它不是Windows ADB的必备依赖。PowerShell旧版文本重定向可能损坏二进制，备份应先写到手机RAM再用 `adb pull`，或使用支持二进制的 subprocess 流。

## 2. Linux依赖和源码

下面apt列表是起点，安装失败按发行版包名修正并记录，不宣称覆盖全部传递依赖。

```bash
sudo apt update
sudo apt install git curl ca-certificates build-essential clang lld python3 python3-venv \
 perl xz-utils flex bison m4 acpica-tools nasm gettext bc rsync device-tree-compiler \
 gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu uuid-dev zlib1g-dev libssl-dev \
 meson ninja-build autoconf automake pkg-config cmake 7zip
uname -m
7z i  # 必须支持APFS，版本>=23；若发行版命令名为7zz，先按上游入口确认兼容
```

```bash
git clone https://github.com/Hercules-Zhaoziyi/mi8-dualboot-lab.git
cd mi8-dualboot-lab
export LAB="$PWD"
python3 tools/agent_preflight.py
python3 tools/fetch_ios_reference.py --out local/ios-reference
cd local/ios-reference
export DIPPER_PROJECT_ROOT="$PWD"
export DIPPER_INPUTS="$PWD/local/inputs"
export THEOS="$PWD/local/theos"
mkdir -p "$DIPPER_INPUTS" "$THEOS/sdks" "$THEOS/toolchain"
git rev-parse HEAD
sha256sum -c SHA256SUMS
python3 tools/prepare/prepare.py
```

参考提交锁定为 **f7cd0f0a61b97c12d9518f0b3fa53bc062c1f6f9**，明确更新此前 d96d9d5 锁。固定脚本/补丁和本文命令必须同版本。不要复制本机旧work来伪装从零构建。

## 3. 材料：下载来源、校验和存放

本仓库 [MATERIALS.json](MATERIALS.json) 列出固定URL、原始文件名和可用SHA256，来源为该提交 `docs/build/INPUTS.md`。SDK归档缺少同等锁定摘要，使用上游 check-sdk.py 检查解包树；“自己算出的SHA256”不是独立可信来源。

| 材料 | 获取/用途 |
|---|---|
| iOSToolchain-x86_64.tar.xz | L1ghtmann/llvm-project固定Release；Theos交叉工具链 |
| iPhoneOS16.5.sdk.tar.xz | theos/sdks固定Release；iOS16.5 SDK |
| MacOSX13.3.sdk.tar.xz | joseluisq/macosx-sdks固定Release；内核生成头 |
| linux-7.0.14.tar.xz | kernel.org；ath10k源码 |
| freebsd-15.1-src.txz | FreeBSD15.1官方src.txz，下载后按清单命名 |
| mesa-25.0.7.tar.xz | Mesa官方archive；图形用户态 |
| iPhone10,3,iPhone10,6_16.5_20F66_Restore.ipsw | Apple CDN原版16.5/20F66；只在本机提取，不在本仓库分发 |
| firmware-5.bin | linux-firmware WCN3990/hw1.0固定内容，非本机modem固件的替代品 |
| TWRP3.7.0_9-0-dipper.img | [TeamWin设备页](https://twrp.me/xiaomi/xiaomimi8.html)和[版本页](https://dl.twrp.me/dipper/twrp-3.7.0_9-0-dipper.img.html)，校验官方摘要 |
| ARM64 sgdisk | 已核验来源的目标架构工具；本仓库没有验证统一下载二进制，不用主机x86程序代替 |
| GPU/Wi-Fi/蓝牙/TAS2557固件 | 从目标手机适配的vendor/modem等输入只读提取；按上游DEVICE_PREPARATION/STARTUP逐项核验 |
| UI字体 | 自行具备使用权的字体，执行本仓库make_local_font_pack.py；必须覆盖当前全部界面文本 |

对清单中每个URL，用 `curl --fail --location --retry 3 'URL' -o "$DIPPER_INPUTS/对应文件名.partial"` 下载；摘要匹配后改名。403/404/超时要记录实际URL和响应，不更换随机网盘资源。合法取得和使用所需SDK/系统输入是使用者责任。

```bash
tar -xJf "$DIPPER_INPUTS/iOSToolchain-x86_64.tar.xz" -C "$THEOS/toolchain"
tar -xJf "$DIPPER_INPUTS/iPhoneOS16.5.sdk.tar.xz" -C "$THEOS/sdks"
python3 tools/prepare/check-sdk.py "$THEOS/sdks/iPhoneOS16.5.sdk"
"$THEOS/toolchain/linux/iphone/bin/clang" --version
python3 tools/prepare/prepare.py --prepare 2>&1 | tee "$DIPPER_INPUTS/prepare.log"
# 使用Bash pipefail或检查PIPESTATUS，不能只看tee退出码
```

成功标记为 Sources prepared and hashes checked。失败先提取最后一个失败命令、退出码、网络/子模块/补丁/摘要原因；保留现场。脚本拒绝已有work时，不反复叠加补丁，也不直接rm -rf。必要时归档失败目录后重新准备，保持单一活动构建树。

```bash
K="$DIPPER_PROJECT_ROOT/work/hackios"
mkdir -p "$K/xnu-deps/build/ath10k" "$K/xnu-deps/build/mesa"
cp "$DIPPER_INPUTS/linux-7.0.14.tar.xz" "$DIPPER_INPUTS/freebsd-15.1-src.txz" "$K/xnu-deps/build/ath10k/"
cp "$DIPPER_INPUTS/mesa-25.0.7.tar.xz" "$K/xnu-deps/build/mesa/"
tar -xJf "$DIPPER_INPUTS/MacOSX13.3.sdk.tar.xz" -C "$K/xnu-deps" \
 --wildcards '*MacOSX13.3.sdk/System/Library/Frameworks/Kernel.framework*'
```

## 4. 设备、TWRP、原机备份

Windows主机设置自己的 `$serial`，Linux主机使用自己的 `$DEVICE_SERIAL` / `$BOOT_SERIAL`，不要拷贝作者序列号。Android核验 `ro.product.device` 为dipper，另记录ROM完整版本、Android版本、BL状态。已有可用TWRP不重复刷。

TWRP安装须BL已解锁、目标镜像官方摘要通过、实际有recovery分区、容量足够且恢复材料完备。未解锁的设备按小米官方流程处理，不能把解锁当成保留数据操作。官方首次安装形式：

```bash
adb -s "$DEVICE_SERIAL" reboot bootloader
fastboot -s "$BOOT_SERIAL" getvar product
fastboot -s "$BOOT_SERIAL" getvar partition-size:recovery
fastboot -s "$BOOT_SERIAL" flash recovery "$TWRP_IMAGE"
# 按TeamWin该机说明直接进入Recovery，避免Android自动恢复原厂Recovery
```

本机此首次安装序列未独立重测；引用TeamWin说明，不能称为本仓库实机认证。TWRP中核验 `id` 为root、`ro.product.device` 和 `ro.twrp.version`。不可把“root已装”替代 su实际返回uid=0。

备份每个实际存在且相关的 boot、recovery、modem、persist、modemst1/2、fsg，以及原始主备GPT和个人文件。先查 by-name 链接、节点容量、挂载及LUN。对单个已确认分区，可使用：

```bash
adb -s "$DEVICE_SERIAL" shell 'dd if=/dev/block/by-name/boot of=/tmp/original-boot.img bs=1048576; sha256sum /tmp/original-boot.img'
adb -s "$DEVICE_SERIAL" pull /tmp/original-boot.img ./original-boot.img
sha256sum original-boot.img
```

手机端与主机端摘要、读出字节数必须吻合；命令任一失败停止。GPT备份并不包含Android用户文件。为其他分区逐项生成明确命令，不能把所有设备节点批量写入/恢复。大备份避免挤满Recovery /tmp，分块/流式读取并检查计数和退出码。

固件提取按固定上游 `docs/install/DEVICE_PREPARATION.md`。GPU输出名为 a630_gmu.bin、a630_sqe.fw、a630_zap.mdt、a630_zap.b02，进入work/hackios/fw-gpu；Wi-Fi描述文件进fw-wifi；蓝牙进fw-bt。TAS2557路径和提取方法以该提交STARTUP扬声器章节为准，记录本机原文件摘要。缺项不能宣称设备就绪。

## 5. 编译XNU与系统材料

```bash
set -o pipefail
cd "$K"
bash xnu-deps/setup.sh 2>&1 | tee "$DIPPER_INPUTS/deps.log"
bash tools/mk-ath10k.sh 2>&1 | tee "$DIPPER_INPUTS/ath10k.log"
bash xnu-build.sh -j6 build 2>&1 | tee "$DIPPER_INPUTS/xnu.log"
```

仅在前一项成功时继续；`build`不等于全部`all`检查通过。内核产物为 `xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_BCM2837/mach.development.bcm2837`，应为ARM64 Mach-O，记录SHA256。

```bash
cd "$DIPPER_PROJECT_ROOT"
python3 tools/prepare/prepare-ios.py --ipsw "$DIPPER_INPUTS/iPhone10,3,iPhone10,6_16.5_20F66_Restore.ipsw"
export BUILD_TIMEZONE=Asia/Shanghai
export BUILD_JOBS=6
bash tools/build/build-userland.sh
python3 tools/build/pack-darwin.py
```

prepare-ios验证IPSW/BuildManifest并提取系统和cryptex；不要使用另一工作树的提取目录。用户态产生root.apfs、flashboot.apfs。darwin.img为原始APFS组合，不是压缩DMG、RAM system.apfs或整盘GPT展开物。此固定版本应为8365539328字节，SHA256 `9efb156f417e32b6f92d709af33c0e8e584693c3fbd29eaf139b57eee3735560`；不同输入版本不得沿用该摘要或偏移。

## 6. 首次布局：先计划，后写入

首次darwin路线会重建userdata并清空Android数据。已有darwin先核对布局/镜像，不重复分区。不同128GB设备也不能复用他人的唯一GUID、LBA或GPT备份。

在Recovery取得主备GPT完整原始范围，范围由逻辑扇区大小、表项数量和磁盘末LBA计算；sysfs start/size单位为512字节，不能混同GPT单位。验证header/table CRC、主备对应、所有分区无重叠、userdata确在尾部及未挂载。记录sgdisk版本和ARM64二进制摘要。

按固定上游 `docs/install/PARTITION_DARWIN.md` 的JSON字段创建本机配置：schema_version、device、serial、twrp_version、disk、sector_size、disk_bytes、sgdisk_sha256、before_primary/backup（路径/摘要）、darwin_size_gib、darwin_guid。示例24GiB不是平分128GB方案；根据实际镜像、数据区和Android所需空间审查。

```bash
python3 tools/partition/partition-darwin.py --config local/devices/my-device/partition.json
python3 tools/partition/partition-darwin.py --config local/devices/my-device/partition.json --output-dir work/runs/partition-01
```

此工具只离线生成计划，不直接改手机。审阅plan.json、create-darwin.sh、SHA256SUMS，确认所有其他分区不变。将核验工具和本轮脚本传入Recovery并回读核验，先按上游做sgdisk --pretend演练。用户授权的具体方案执行形式为 `adb -s "$DEVICE_SERIAL" shell sh /tmp/create-darwin.sh CREATE-DARWIN`。

写后回读主备GPT，用同一工具的 --verify-primary/--verify-backup 校验；重启Recovery后重新读节点容量，加 --kernel-userdata-bytes / --kernel-darwin-bytes验收。内核仍用旧表时不能格式化。确认新布局已识别，再按该方案在TWRP Format Data重建userdata，启动MIUI完成初始化、检查df。不能把仅清缓存视为重建文件系统。

## 7. 同步布局、刷系统和回读

将本机darwin首末LBA、类型及唯一GUID同步到 `work/hackios/xnuloader/loader.c` 的BLOCK_DARWIN字段；将相同边界与写入保护同步到 `bsd/dev/redwood_ufs.c` 的UFS_DARWIN字段。GUID首三整数与GPT字节序不同，需正确转换。公开全零GUID是占位，不可取消校验绕过。修改内核边界后重建内核，修改GUID后至少重建loader和包含它的UEFI。

```bash
cd "$DIPPER_PROJECT_ROOT"
bash tools/build/build-loader-uefi.sh
fastboot -s "$BOOT_SERIAL" getvar product
fastboot -s "$BOOT_SERIAL" getvar partition-size:darwin
fastboot -s "$BOOT_SERIAL" getvar partition-type:darwin
# 身份/布局/容量/输入摘要/恢复条件均通过后，才执行：
fastboot -s "$BOOT_SERIAL" flash darwin "$DIPPER_PROJECT_ROOT/work/images/darwin.img"
```

只写darwin，不替换成userdata、boot或整盘。返回Recovery读取**实际镜像长度**做摘要比较。此固定原始镜像可 `dd ... bs=1048576 count=7978 | sha256sum`，必须支持pipefail、确认完整读取8365539328字节，且摘要与主机相同。不同镜像重新计算长度；稀疏传输OKAY不替代回读。

## 8. 先走上游主机辅助启动，不先永久双启动

```bash
cd "$DIPPER_PROJECT_ROOT"
export RUN_ID=desktop-readonly-01
python3 tools/build/build-payload.py --output "work/runs/$RUN_ID/candidate" --seconds 300 --wifi
export CANDIDATE="$PWD/work/runs/$RUN_ID/candidate"
(cd "$CANDIDATE" && sha256sum -c SHA256SUMS)
fastboot -s "$BOOT_SERIAL" boot "$CANDIDATE/boot-dipper.img"
# 等待重新枚举，明确product为xnuloader后：
fastboot -s xnuloader getvar product
fastboot -s xnuloader oem ramdisk
fastboot -s xnuloader stage "$CANDIDATE/flashboot.apfs"
fastboot -s xnuloader stage "$CANDIDATE/payload.bin"
fastboot -s xnuloader oem run
```

若未准备完整Wi-Fi输入，先省略--wifi做无线关闭对照。初轮不加ufs_write或旧状态恢复。五分钟测试不是日常无限时版本。读取 result、oem log，再 `get_staged` 保存kernel日志。看到XNULOADER usb:started只表示等待主机；no payload表示载荷没有到约定地址；jumping to后无输出不能证明内核正常运行。

验收须有桌面、触摸、图像、实际新网页、声音等逐项记录。电池/蓝牙连接不证明通话或蓝牙音频可用。完成只读阶段后再按固定上游STARTUP的--persist规则测试存档。归档头slot编号必须匹配实际槽，头CRC和body CRC均通过，新头最后提交；保留最新有效旧槽，不能清空双槽来“修复”。

## 9. 触屏菜单及本机双启动集成

先独立完成本仓库 [BUILD](BUILD.md)、[FONTS](FONTS.md)、[INSTALL](INSTALL.md)，确认菜单组件与字体资源不是iOS系统。更新字体子集时检查每个当前界面汉字，不依赖不同字号的gui_font缺字回退。

dipper FTS集成必须同时做：

- 将 source/device-adapters/dipper/Platform 下驱动放回对应平台，INF加入DSC和FDF；只复制UI不会加载硬件驱动。
- 将同目录Silicon下HerculesFtsProbe.h放到QcomPkg Include/Protocol。
- 使用已更新uefi_touch.c、hercules_pointer_state.h、tap_guard和菜单代码；开启ENABLE_UEFI，核对AbsolutePointer协议和坐标/抬手事件。
- 先核对本机是否为FTM5，设备树/I2C/供电/TCS所有权。正式版不定义两项TRACE_DIAGNOSTIC和RAM_NO_COUNTDOWN。诊断版分别记录驱动加载、供电、ID、事件阶段，不能对未知控制器写猜测寄存器。
- FTS实机证据仅来自一台dipper。其他同型号手机仍需验证芯片、屏幕及固件组合。

按tools/compose_dipper_boot.py --help提供自己的原boot、SHA256、匹配FD、BootShim、kernelpatcher、自备XNUP及boot真实容量。此入口只处理限定legacy格式，拒绝的格式应实现适配，不删除断言。BootShim要联编charger_probe.c及HerculesCharger.ld，保留原Android跳转、原DTB/ramdisk、固件与载荷地址和解压预留。必须包含复制XNUP至0xF0000000的步骤；当前修复有ARM64模拟证据，但没有完整冷启动稳定性结论。

先fastboot boot组合候选，逐项测试：菜单触控/误触取消/高级页返回/10秒无操作Android/首次输入取消计时；Android桌面与ADB启动完成；iOS桌面及相同候选内核/参数/存档；进入FASTBOOT/TWRP、拔线关机、关机插线充电。失败则使用自有原boot回退，不能因为UI正常就永久写。

## 10. 正式写入与回退

本仓库当前没有经过全新设备端到端认证的“一键永久双启动”方案。只有上述候选实测和恢复路径都通过后，才按已授权范围执行 `fastboot -s "$BOOT_SERIAL" flash boot "$COMBINED_BOOT"`。核对目标确是本机boot、非recovery，镜像格式与真实容量匹配；保留全分区原boot和候选摘要。

写后回读候选实际长度做SHA256比较，另记录整个boot分区摘要；确认Recovery和GPT前后未变。不能在文件末尾含残留数据时直接拿全分区hash与较短候选hash比较。断开电脑，冷启动分别验收Android和iOS。临时fastboot boot随重启丢失，普通重启不会自动保留临时修复。

仅恢复boot：回到可用FASTBOOT或TWRP，校验自己的原boot后写回并完整回读；不恢复别人的镜像。布局恢复：使用固定上游restore-android-layout.py、本机原GPT与本轮备份生成方案，属于另一次会清数据的操作，仅刷boot不能恢复GPT/用户文件。不要锁BL、碰设备校准分区或猜测EDL写法。

## 11. 故障定位速查

| 现象 | 首要证据/下一步 |
|---|---|
| adb找不到 | 原生平台工具路径、驱动、授权、线/口及手机模式 |
| prepare失败 | 精确失败命令/退出码、固定commit、网络和子模块；保留失败work |
| 内核编译失败 | 第一处真实编译/链接错误，THEOS和SDK树检查；不只贴末尾make Error2 |
| 菜单能看不能摸 | DSC/FDF是否编入FTS，协议/供电/芯片ID/事件到UI逐层检查 |
| Android返回FASTBOOT | 原boot格式、交接magic、BootShim分支目标及覆盖范围；先自有备份恢复 |
| no payload | XNUP magic/长度/偏移、BootShim复制步骤、固定地址、固件完整性 |
| jumping to后卡住 | 候选hash、完整cmdline、内核/DTB/存储GUID匹配、kernel日志；不是游戏签名结论 |
| 游戏无图标 | 存档实际选槽和CRC→bundle文件→可执行权限→LS注册日志，不先反复重装 |
| 容量条接近满 | 真实RAMFS页占用/元数据/双槽上限；增加闪存分区不自动扩大内存盘 |
| Recovery /tmp不足 | 不再放第二份大回读文件，流式校验或分块回主机；保留旧有效槽 |

## 12. 完成定义

材料齐备、准备成功、内核构建、用户态构建、系统镜像校验、备份与布局、系统刷写回读、只读桌面、数据恢复、菜单RAM双系统、永久写入回读、断线冷启动应分别有证据。某一阶段未通过就如实标记，不能用“还差刷进去”“型号一样通用”代替结论。原始聊天、私人提示词和账号资料不得写进公共文档。
