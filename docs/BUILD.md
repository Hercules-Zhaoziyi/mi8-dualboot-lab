# 构建

## 范围

公开构建只生成 UI 和 Android 交接的 EFI/FFS。不会构建固件卷、手机 boot 镜像、XNU、Apple 用户空间或 IPSW，也不执行手机命令。

主要源码在 `source/simple-init/`，保留其原始许可证和第三方库。平台依赖按 `UPSTREAM_SOURCES.json` 固定版本获取。不要用未知分支覆盖依赖后仍声称与 Release 一致。

## 环境

已用 Windows 11 + WSL Ubuntu 构建。模块需要 Linux/WSL、Git、Python 3、GCC/G++、AArch64 GNU 交叉工具链、Clang、Make、Gettext、UUID 开发包与 edk2 BaseTools 依赖。原生主机回归还需 CMake、FreeType 运行库；字体裁剪工具为可选的 FontTools 4.68.0。

示例软件包名称：

```bash
sudo apt update
sudo apt install git python3 python3-venv gcc g++ make clang lld \
  gcc-aarch64-linux-gnu g++-aarch64-linux-gnu gettext uuid-dev nasm \
  acpica-tools cmake xz-utils zip
```

这是已装依赖环境的构建记录，不宣称完成了全新系统的全部安装回归。

## 依赖与编译

```bash
python tools/prepare_sources.py --dest local/edk2-sdm845
bash tools/build_ui_components.sh
python tools/package_components.py --workspace local/build-ui/workspace --out dist/ui
```

可用环境变量 `HERCULES_PORT_ROOT` 指向自己已有的固定版本平台树。脚本使用模块专用 DSC，不引用 XNU/Darwin 包，不读取 `XNU_PAYLOAD`，也不生成/刷写 `.img`。

Clang 构建沿用经过设备验证的 `FIX_CLANG=1` 选项，避免旧优化路径的问题。主机 GCC 默认标准变化由局部 `tools/host-bin` 包装器处理，不改变系统编译器配置。

`dist/ui/` 包含 UI EFI/FFS、Android 交接组件、清单和 SHA256。组件依赖兼容宿主固件与本地字库资源，安装见 [INSTALL](INSTALL.md)。

## 测试

```bash
python tools/run_host_tests.py --simple-init source/simple-init \
  --local-fonts local/fonts --out local/tests
python tools/audit_publication.py .
```

测试实际 LVGL、FreeType 与 NanoSVG，按固件 `SCALE=2` 采样契约检查图标；覆盖真实文字节点、两页署名、系统与高级选中、按钮路由、12 次页面往返、多层返回、10 秒默认 Android、首次输入取消、返回不恢复计时和内存安全。没有本地同款字库时只执行可用的静态检查并明确说明未执行渲染测试，不把跳过当通过。

## 对应源码

Release 源包和本仓库包含本 UI 修改、实际 SimpleInit 源码及其库、集成脚本和许可证。未修改的 edk2/平台依赖以精确提交和可获取地址列出；获取脚本不下载 Apple 系统材料。公开发布不是完整 iOS 镜像的再分发授权。

构建脚本会为公开组件移除本机源码绝对路径；Release SHA256 对应已上传文件。未声明跨工具链逐字节确定性，构建元数据和工具链版本可能改变二进制摘要。

构建脚本会为公开组件移除本机源码绝对路径；Release SHA256 对应已上传文件。未声明跨工具链逐字节确定性，构建元数据和工具链版本可能改变二进制摘要。
