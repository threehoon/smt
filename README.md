# 迅贴 SwiftPlace（smtpro）

SMT 贴片机上位机软件。当前阶段实现了贴装程序的编辑与存储，设备控制和贴装执行在后续阶段加入。

已有功能：

- 程序（拼板、贴片位、元件、封装、送料器）的新建、打开、保存，文件格式为 `.smt.xml`
- 编辑时实时校验，撤销 / 重做
- 从 CAD 坐标文件（KiCad `.pos` / `.csv`、Altium Pick Place、嘉立创 CPL）导入贴片位
- 打包为 Windows 安装程序或免安装目录

## 环境要求

仅支持 Windows x64。

| 项 | 要求 |
|---|---|
| Qt | 5.14.2 MSVC 2017 64-bit（默认路径 `D:\Qt\5.14.2\msvc2017_64`） |
| 编译器 | MSVC 工具集 **v143 14.44**，**不能用 14.51 (v145)** |
| CMake | 3.16 及以上 |
| Ninja | 任意较新版本（Visual Studio 自带） |
| C++ 标准 | C++17 |

不能用 MSVC 14.51 的原因：Qt 5.14.2 的头文件使用了 `stdext::make_checked_array_iterator`，14.51 的 STL 已删除该接口，编译会报 `C3861`。

## 构建与运行

### 命令行

1. 修改 `dev-env.bat` 中的 Visual Studio 路径（`vcvarsall.bat`）和 `QTDIR`，指向本机安装位置。
2. 在仓库根目录执行：

```bat
dev-env.bat
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/Qt/5.14.2/msvc2017_64
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

配置输出里应看到编译器是 `MSVC 19.44`。如果显示 GCC，说明 PATH 里的 MinGW 抢先了，需要先运行 `dev-env.bat`。

3. 运行：

```bat
build\debug\bin\SwiftPlace.exe
```

运行前 PATH 中需要有 `<Qt>\bin`，`dev-env.bat` 已处理。

### Visual Studio

用"打开文件夹"方式打开仓库根目录，选择 `debug` 或 `release` 配置预设。`CMakePresets.json` 已固定 v143 14.44 工具集。如果 Qt 不在 `D:/Qt/5.14.2/msvc2017_64`，把该文件中的 `CMAKE_PREFIX_PATH` 和 `PATH` 改成本机路径，或放到本机的 `CMakeUserPresets.json`（已被 git 忽略）里覆盖。

### 使用示例

启动后从"文件"菜单打开 `samples/example.smt.xml`。CAD 导入可用 `samples/cad/` 下的文件试用。

## 打包

```bat
package.bat
```

依次执行 Release 构建、测试、`cmake --install` 到 `build/release/stage`（含 windeployqt 和 MSVC 运行库，这个目录本身就是免安装版），再用 Inno Setup 6 生成 `dist/SwiftPlace-<版本>-setup.exe`。

生成安装程序需要先安装 Inno Setup 6：`winget install JRSoftware.InnoSetup`。没有它时 `build/release/stage` 仍然可用。

## 目录结构

```
src/
  core/      领域模型、程序文件读写、校验、编辑与撤销、CAD 导入（只依赖 QtCore）
  machine/   设备抽象与仿真实现（占位）
  app/       入口，暴露给 QML 的列表模型和文档对象
  qml/       Qt Quick 界面
packaging/   图标、版本资源、安装脚本
tests/       Qt Test 用例（core / app / samples）
docs/        program-format.md 程序文件格式；cad-import.md 坐标文件导入规则
samples/     示例程序与 CAD 坐标文件
```

依赖方向为 `qml → app → machine → core`。

## 文档

- [程序文件格式](docs/program-format.md)
- [CAD 坐标文件导入](docs/cad-import.md)

## 许可证

[MIT](LICENSE)
