# 同款字库与公开分发

## 界面与 SVG 保持原样

首页和高级页面保留定稿的文字、布局、壁纸与交互；三个用户指定 SVG 的原件位于 `assets/logos/`，有独立 SHA256 清单。没有用字体图标替换 Android、Apple 或小红书标志。

字体是文字字形数据，不是这三个 SVG。不同字体会改变字形、字重和字距，所以公开组件不把替换字体版冒充定稿同款，也不直接分发 Windows 字库软件。

## 两种构建方式

- **本机私有整合方式**：从自己有权使用的本机字库生成同款子集，嵌入自己的本地 UI 构建。手机安装实验使用此方式。
- **公开固件组件方式**：Release 的 EFI/FFS 不内置该字库，使用两个 RAW FFS 字体资源。用户在本机生成并加入自己的固件卷，组件读取后呈现同款界面。缺少资源时进入手动维护提示并关闭自动启动。

字体资源始终留在 `local/`，不提交 Git，也不上传 Release。字体权利和许可由本地使用者确认；保留原始字体的版权、名称和嵌入限制元数据。

## 资源接口

| 资源 | RAW FFS GUID | 内容 |
| --- | --- | --- |
| regular | `e71a7a5c-8196-4d48-b641-09d21a13bdf4` | 本机生成的常规字形 TTF |
| bold | `30c1b010-8e7b-48a7-bd44-6978cf57d210` | 本机生成的粗体 TTF |

公开组件通过标准 `GetSectionFromAnyFv(..., EFI_SECTION_RAW, 0, ...)` 读取资源。字库提供层不包含 OS、内核或应用数据。

## 本机生成

在 Python 环境安装固定工具版本 `fonttools==4.68.0`，执行：

```bash
python tools/make_local_font_pack.py \
  --regular /mnt/c/Windows/Fonts/msyh.ttc \
  --bold /mnt/c/Windows/Fonts/msyhbd.ttc \
  --out local/fonts \
  --base-tools local/edk2-sdm845/Common/edk2/BaseTools/Source/C/bin
```

工具裁剪本界面需要的字符，保留版权与字体元数据，拒绝受限的嵌入/子集标志，并输出字库和 RAW FFS。使用自己的合法输入，不从仓库下载字库。

原手机实验输入为 Microsoft YaHei 常规/粗体的本机版本。若要求精确字形一致，需要一致的输入版本；别的合法字库可以加载，但不声明与定稿逐像素一致。源文件本身和生成的私有字体包均不公开。

FDF 集成模板位于 `source/edk2-integration/Fonts.private.fdf.inc.example`。实际路径通过本机构建变量提供，不把字库内容放入公共仓库。
