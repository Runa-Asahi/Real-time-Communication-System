# CI 工作流

Windows 构建、测试、打包和发布的工作流有两份相同内容：

- `.github/workflows/windows-release.yml`：GitHub Actions 实际执行的文件
- `ci/windows-release.yml`：仓库里便于阅读的副本

推送 `v*` 标签（如 `v1.0.0`）会构建并发布正式版，附件为 `Ciallo-v1.0.0-windows-x64.zip`。手动触发时仍生成草稿预发布。
