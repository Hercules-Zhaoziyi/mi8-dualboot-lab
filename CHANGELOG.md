> 2026-11-14：新增触屏、模式切换、关机/充电修复源码及[验证与限制](docs/UPDATES-2026-11-14.md)。存在正式/临时版本启动回归，先阅读该记录。

# 更新记录

## v0.1.0-ui-preview — 2026-11-12

- 首次公开 UI 固件组件与对应源码、安装/使用/构建文档及脱敏实验记录。
- 保留定稿浅色主题、原生文字、三个指定 SVG 与两页共用的一行署名。
- 主标题略微缩小；10 秒默认 Android，首次输入取消且不恢复计时。
- 统一高级页和子菜单；修复返回禁用、焦点生命周期、系统选择保持及高级重复勾选。
- 按真实固件 2 倍 SVG 采样固定显示尺寸，避免裁切和文字覆盖。
- 公开组件使用本地字体资源提供层，不发布替换字体外观或私有字库。

公开封装属于实验性组件，模块构建与主机回归通过；具体设备验证范围见 `docs/VALIDATION.md`。

## Unreleased — Agent reproduction and portable menu source

Added AGENTS workflow, pinned iOS port reference fetcher, full stage-by-stage install/integration guidance, profile generator and evidence templates. Menu labels/target IDs are configurable and GOP-sized layouts accept non-Mi8 resolutions. Default artwork unchanged. Other devices and new scaling paths need validation; existing v0.1.0 artifacts are unchanged.

## Unreleased — Windows installer development preview

Added one-click preparation launcher, official SDK metadata checksum download, cached tool integrity receipts, resumable explicit plans, device binding, TWRP RAM boot, boot/recovery backup, RAM acceptance and guarded boot update. First-install partition migration remains disabled. Ten backend/workflow tests pass without phone operations; Windows GUI and clean-computer/second-device end-to-end remain untested.
