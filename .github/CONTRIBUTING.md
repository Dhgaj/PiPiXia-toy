# 贡献指南 (Contributing Guidelines)

感谢你对 PiPiXia 编程语言编译器项目的关注！

## ⚠️ 重要许可证声明

**本项目采用严格的专有许可证**。在贡献之前，请仔细阅读 [LICENSE](../LICENSE) 文件。

### 关键要点：
- 本项目由 [Dhgaj](https://github.com/Dhgaj) 拥有和维护
- 任何使用都必须注明原作者
- 禁止商业使用
- 禁止未经授权的重新分发

## 🤝 如何贡献

### 1. 报告 Bug
- 使用 [Bug Report 模板](.github/ISSUE_TEMPLATE/bug_report.md)
- 提供详细的重现步骤
- 包含环境信息和错误输出

### 2. 功能请求
- 使用 [Feature Request 模板](.github/ISSUE_TEMPLATE/feature_request.md)
- 详细描述用例和动机
- 考虑实现复杂度和优先级

### 3. 提问
- 使用 [Question 模板](.github/ISSUE_TEMPLATE/question.md)
- 先查看现有文档和 issues
- 提供足够的上下文信息

### 4. 代码贡献

#### 准备工作
1. Fork 本仓库
2. 创建功能分支: `git checkout -b feature/your-feature-name`
3. 确保开发环境正确配置

#### 开发规范
- **代码风格**: 遵循现有代码风格
- **注释**: 使用中文注释
- **文件命名**: 使用小写字母和下划线
- **测试**: 为新功能添加测试用例

#### 提交规范
```
类型(范围): 简短描述

详细描述（如果需要）

- 变更点1
- 变更点2
```

类型：
- `feat`: 新功能
- `fix`: Bug 修复
- `docs`: 文档更新
- `style`: 代码格式
- `refactor`: 代码重构
- `test`: 测试相关
- `chore`: 构建/工具

#### Pull Request 流程
1. 确保所有测试通过: `make test`
2. 更新相关文档
3. 使用 [PR 模板](.github/pull_request_template.md)
4. 等待代码审查

## 🛠️ 开发环境设置

### 系统要求
- **支持的系统**: Linux, macOS
- **编译器**: GCC 或 Clang (支持 C++17)
- **构建工具**: Make, Flex, Bison
- **LLVM**: 版本 18+ 推荐

### 快速开始
```bash
# 1. 克隆仓库
git clone https://github.com/Dhgaj/PiPiXia-toy.git
cd PiPiXia-toy

# 2. 检测平台并安装依赖
./scripts/01_detect_platform.sh

# 3. 构建编译器
make

# 4. 运行测试
make test
```

### 开发工作流
```bash
# 创建功能分支
git checkout -b feature/new-feature

# 开发和测试
make clean
make
make test

# 提交更改
git add .
git commit -m "feat: 添加新功能"

# 推送并创建 PR
git push origin feature/new-feature
```

## 📋 代码审查标准

### 必须满足的条件
- [ ] 代码编译无警告
- [ ] 所有测试通过
- [ ] 代码风格一致
- [ ] 有适当的注释
- [ ] 更新了相关文档

### 审查重点
- **正确性**: 代码逻辑正确
- **性能**: 不引入明显的性能问题
- **安全性**: 没有内存泄漏或安全漏洞
- **可维护性**: 代码清晰易懂
- **兼容性**: 不破坏现有功能

## 🚫 不接受的贡献

- 违反许可证条款的代码
- 未经讨论的重大架构变更
- 不符合项目目标的功能
- 质量不达标的代码
- 缺乏测试的重要功能

## 📞 联系方式

- **项目维护者**: [Dhgaj](https://github.com/Dhgaj)
- **邮箱**: [sifanlian@gmail.com](mailto:sifanlian@gmail.com)
- **GitHub Issues**: 用于 bug 报告和功能请求
- **GitHub Discussions**: 用于一般讨论

## 🙏 致谢

感谢所有为 PiPiXia 项目做出贡献的开发者！

---

**注意**: 
- 提交贡献即表示你同意项目的许可证条款
- 项目维护者保留接受或拒绝任何贡献的权利
- 所有决定都以项目的最佳利益为准

**Attribution**: 本项目由 [Dhgaj](https://github.com/Dhgaj) 开发和维护。
