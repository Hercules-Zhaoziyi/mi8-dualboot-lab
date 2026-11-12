# AI agent 复刻协议

用户可把本仓库 URL 交给 Codex 或其他 agent，并给出任务：

> 阅读 AGENTS.md；核实我的设备与主机。若是 dipper，按 IOS-REPRODUCTION 从固定上游复刻，保留 Android 和恢复入口，再集成本仓库 UI。若是其他机型，先按 PORTING 输出缺口和适配计划。私人材料只存 local/。任何设备写入都遵循我实际授权的范围。先提交可核验的备份、布局和候选记录，不要把示例数值直接刷入我的设备。

## 状态机与交付证据

| 阶段 | 输入/工作 | 进入下一阶段的证据 |
|---|---|---|
| discover | 主机系统/磁盘、ADB/fastboot 版本、机型/SoC/RAM/系统、解锁、root/recovery、USB | 原始输出保存在本地；确认实际设备，不把序列号公开 |
| backup | boot/recovery/vendor_boot/init_boot（按实际存在）、两份 GPT、重要数据、存档 | 设备端与主机端 SHA256 一致；已知恢复入口；可用空间充足 |
| sources | 固定上游、校验清单、工具链、合法自备材料 | commit 与 SHA256 匹配，缺项清单为空或明确阻塞 |
| layout | 逻辑扇区、LUN/slots、GPT CRC、挂载、系统镜像和可写存档边界 | dry-run 布局；loader/kernel/writer 的边界与唯一 GUID 一致 |
| build | 内核/用户空间/loader/UEFI/BootShim/原 Android 载荷 | 对应源码与输入摘要；完整候选检查；不混用旧版本 |
| ram-boot | 经匹配机制临时启动；先 Android，后无写盘 iOS | 实际桌面、屏幕/输入、日志和双系统验证；异常完整重启 |
| persist | 有界写入与新存档；重启恢复验证 | 头/内容 CRC 与记录边界；设置、照片和应用的实际检查 |
| install | 仅授权范围的正式写入与回读 | 当前分区和候选容量匹配；回退文件有效；完整回读；恢复/GPT 不变 |
| acceptance | 断开电脑冷启动、菜单两条路径、倒计时/取消、数据保留 | owner 的具体观察与日志；未验证项保留为 unknown |

先运行 `python3 tools/agent_preflight.py` 做仓库层检查。它不连接手机，不构建或下载系统。用 `tools/fetch_ios_reference.py` 获取固定的上游复刻工程，再按其文档逐步执行。首次分区与已有安装走不同路径；不要把重新分区当成每次更新的必要操作。

`profiles/report.template.json` 是本地交接模板；不得在公共 Git 中填写设备身份和账号。网络或授权弹窗需要持有人配合时，可继续做不依赖该步骤的主机工作。不要声称工具能远程操作物理按键。
