# CI 工作流（待启用）

`windows-release.yml` 是 Windows 构建 + 测试 + 打包 + 发布的 GitHub Actions 工作流。

当前推送凭据没有 `workflow` 权限，无法直接写入 `.github/workflows/`。启用方法（任选其一）：

1. 网页：在 GitHub 仓库里新建文件 `.github/workflows/windows-release.yml`，内容复制本文件；
2. 本机：`git mv ci/windows-release.yml .github/workflows/` 后提交推送。

启用后：PR 自动构建测试并上传截图；推送 `v*` 标签（如 `v1.1.0`）自动生成 Release 并附带 `Ciallo-v1.1.0-windows-x64.zip`。
