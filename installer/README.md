# Mi8 Install Desk — 开发预览

Windows 11 图形向导，Python 3.11+ 标准库/Tk，无 pip 依赖。双击 Start.cmd；若 PowerShell 执行策略阻止运行，请在终端用已安装 Python 直接运行 `python installer/mi8_install_desk.py`，不需要全局放宽执行策略。

## 能做什么

工作磁盘空间检查、独立工作目录/私有事件与状态记录、单设备与 dipper 身份/解锁核验、HTTPS+可信 SHA256 下载缓存、ADB 未授权/离线提示、TWRP 临时启动、TWRP root 与非 A/B 检查、boot/recovery 完整备份及两端摘要核验、镜像容量/魔数/摘要检查、按具体分区确认后写入、候选全长回读、失败尝试恢复、原 boot 恢复。所有设备操作由软件使用者点击；本次开发未触碰维护者手机。

## 从零准备

1. Windows 官方 Python 安装器安装 Python/Tk；安装目录加入 PATH，重新打开终端。手机必须自行完成官方 BL 解锁；软件不会绕过解锁或清除账号锁。准备可用数据线和主板 USB，必要时使用机型厂商驱动；不静默安装未知驱动、不全局关闭签名校验。
2. 从 [Google 官方 Platform Tools](https://developer.android.com/tools/releases/platform-tools) 下载 Windows ZIP，解压，将含 adb.exe/fastboot.exe 的目录填入软件。保持授权弹窗可见。工具授权条款由使用者确认；不要从随机网站下载改版工具。
3. 从 [TeamWin 的 dipper 页面](https://dl.twrp.me/dipper/) 获取正确的 TWRP 和官方 SHA256。官方要求通过 HTML 页面下载，不允许直链/镜像，因此软件不托管或硬编码镜像直链。把文件与可信摘要填入后，在 FASTBOOT 使用“临时启动TWRP”。若需永久安装，先备份再点“刷入recovery”，输入 `FLASH recovery`。不要在同一轮未核实的设备上盲刷。
4. 进入 TWRP，保持 USB 连接；按需关闭 MTP。点击“备份boot/recovery”。备份不等于全数据/GPT备份；首次 iOS 安装还必须按项目 IOS-REPRODUCTION 做数据、GPT与布局计划。
5. iOS 系统、SDK 和设备固件由使用者合法准备；从 README 的固定上游流程重建，再用本项目 compose_dipper_boot.py 组合本机候选。可在“HTTPS下载地址”中填自己的合法来源和独立可信 SHA256 下载输入；软件不会把下载后自算摘要冒充独立校验。不得把 EFI/FFS、IPSW 或任意 darwin.img 当 boot.img。
6. 完成 RAM 临时引导、原 Android 与 iOS 启动和布局验证后，选择针对这台设备构建的完整 Android 格式 boot 候选，填可信摘要，在“写入确认”填 `FLASH boot` 后点击刷入。工具本身不能验证 iOS 是否正确移植，不能用通过魔数检查代替完整镜像与分区审核。
7. 写入与回读完成后人工检查两系统、数据、恢复入口及断线冷启动。有 pending_write 或恢复失败时保持恢复环境，不要盲目重启。USB 断开期间无法自动恢复；重连同一设备后检查状态并使用“恢复boot”。私有日志不得上传 GitHub。

## 未完成与边界

这是可运行的维护向导开发版，**还不是从原厂空白设备全自动安装 iOS 的一键软件**。首次 GPT 重分区、原厂 Android 数据迁移、自动 Apple 输入获取/SDK安装、完整 WSL内核/用户空间构建、首次 darwin 写入和原厂救砖尚未集成。遇到这些步骤必须转到项目工程流程，不伪造成功。没有全新电脑或第二台 Mi8 的端到端测试，不能保证“所有情况”。软件也不检测任意危险候选中的内核语义；刷入前必须完成对应工程验证。

工作目录可放 D/E 等有空间的磁盘。状态恢复只保留日志与备份，不自动重放任何写入。操作超时不会保证远端 dd 已停止，需读取状态和分区后再恢复。镜像下载失败保留 .part，摘要不符停止，不自动替换版本。所有备份与输入在 local/ 中，不随发布包分发。


## 一键入口与分阶段工作流

双击 `Install.cmd`：缺少 Python 时通过系统 winget 安装官方 Python 包（需本机处理许可/UAC）；自动从 Google 官方 SDK 仓库元数据选取 Windows Platform Tools，按元数据提供的 SHA1/SHA256 校验后解压，并保存 SHA256 收据。Google 元数据和 ZIP 使用 HTTPS；这不是独立数字签名认证。准备模式不接触手机。下载存在摘要不一致即停止，不信任已有不明文件。

默认使用 `%LOCALAPPDATA%/HerculesMi8Install`；可换 D/E 盘：

```powershell
powershell -NoProfile -File installer/Install.ps1 -Work D:\Mi8Install
```

已有 iOS 布局、已有对应设备完整boot候选的用户，可复制 existing-install.template.json 到私有目录，填自己合法来源与可信 SHA256；TWRP 必须按 TeamWin 条款从页面获取，不提供硬编码下载直链。然后：

```powershell
powershell -NoProfile -File installer/Install.ps1 -Plan D:\private\install.json -Work D:\Mi8Install -Apply
```

工作流串联准备 → FASTBOOT身份/BL → 临时TWRP → boot/recovery备份 → 候选RAM启动 → 实机验收 → 回TWRP → 明确确认写boot → 回读。中途需要处理授权弹窗和物理按键；状态记录不会自动重放写入。计划改变要求新工作目录。首次重分区/系统构建仍拒绝执行；尚未提供可分发的完整iOS整包，因此不能宣称原厂手机双击即可完成全部安装。当前一键入口默认只做环境准备，不刷机。

软件不会用菜单的 EFI/FFS Release 冒充完整boot。首装需要先完成 docs/IOS-REPRODUCTION.md 的合法输入、编译、布局、完整数据/GPT备份与镜像组合。下一阶段必须补齐经过独立设备验证的首次分区适配器，才可开放首装写入。现有更新工作流也不自动备份全部用户数据，操作前必须单独完成。
