# v0.2.0 触屏实验组件

提供 AARCH64 菜单、Android交接、FTS DXE 的 EFI/FFS 及配置。不是完整手机boot镜像，不能fastboot flash boot这些组件。FTS FFS GUID为42d50d53-c590-4515-8a50-c21795b0ad6a，Depex TRUE，需宿主提供MMIO/供电/时钟适配；只在dipper/FTM5有设备证据。

菜单字库仍用本机生成的两项RAW FFS，见FONTS。触屏操作提示已更新。重启/关机修复代码包含在菜单组件，宿主构建必须保持dipper专用编译开关。FTS必须作为DXE加载，仅把菜单EFI放进去不会获得触屏。

这些公开外部字库组件已主机编译，未重新作为完整固件刷机验证。私人本机整合版有触摸及手势实机结果，不能把它等同于此次公开组件的端到端实测。当前iOS启动回归见UPDATES-2026-11-14；不含XNU、IPSW、APFS、系统载荷、用户存档或设备校准分区。

完整boot是目标设备原Android内核/DTB/ramdisk、宿主UEFI和自备载荷的组合。同为dipper也需匹配ROM、GUID、分区、内存布局；请从FROM-ZERO按本机材料生成，先RAM验证再写入。
