# Swan Dict 天鹅词典

<p align="center">
  <img src="assets/swan-2494916.jpg" alt="Swan Dict" width="320">
</p>

Swan Dict 是一个 KDE Plasma 6 小组件，基于系统自带的 Digital Clock 改造。在系统中的任意应用选中单词即可触发划词翻译。翻译内容会取代桌面的日期部分，减少桌面空间的占用。

<p align="center">
  <img src="assets/屏幕截图_20260602_203624.jpg" alt="work" width="320">
</p>

将鼠标悬停在小组件上方或点击小组件可以获得更详尽的翻译。

https://github.com/user-attachments/assets/03ee851b-52c1-4550-bb38-dbdf94e013e0

它会读取当前 primary selection：

- 在紧凑视图中，用选中文本的简短释义替换原本的日期文本。
- 悬停时显示本地词典释义。
- 点击时打开词典弹窗，显示完整释义、变形表、复制按钮等。
- 多词选中时，按单词拆分并分别显示词典内容。
- 可选配置 DeepSeek API key，在弹窗中手动触发整句翻译。

本地词典数据来自 [ECDICT](https://github.com/skywind3000/ECDICT)。

选中文本预处理思路参考：

```text
https://github.com/program-in-chinese/vscode_english_chinese_dictionary
```

## 功能

- 支持 KDE Plasma 6。
- Wayland 下通过内置 Wayland primary-selection client 读取 primary selection。
- X11 下通过 `QClipboard::Selection` 读取 primary selection。
- 使用本地 SQLite 词典，不依赖网络完成单词翻译。
- 支持 DeepSeek 手动整句翻译。
- 支持简体中文界面翻译。
- 支持从系统安装的 Digital Clock 复制源码并应用补丁，尽量匹配发行版自己的 Plasma 版本。

## 依赖

构建依赖大致包括：

- CMake
- Extra CMake Modules
- pkg-config / pkgconf
- C++17 编译器
- Python 3
- Qt 6 Core / Gui / Network / Qml / Sql
- KF6 I18n
- KF6 Package
- Plasma Workspace
- Wayland client 开发文件和 wayland-protocols
- KWin 开发文件，用于构建可选鼠标点击辅助 effect

运行依赖：

- Plasma 6
- Qt 6 SQLite 插件
- 可选：KWin 鼠标点击辅助 effect，用于在用户点击其它位置后更快清空过期选区翻译

Debian 13 上可参考：

```console
sudo apt --mark-auto install \
  cmake extra-cmake-modules ninja-build pkgconf python3 \
  qt6-base-dev qt6-declarative-dev qt6-tools-dev-tools \
  libkf6i18n-dev libkf6package-dev \
  plasma-workspace kwin-dev libwayland-dev wayland-protocols
```

## 初始化源码

```console
git submodule update --init --recursive
```

ECDICT 以 git submodule 形式放在：

```text
third_party/ECDICT
```

## 构建

```console
cmake -B build -S .
cmake --build build
```

CMake 会在构建时从：

```text
third_party/ECDICT/ecdict.csv
```

生成：

```text
applet/contents/data/ecdict.sqlite
```

该 SQLite 文件是生成产物，不应提交到 git。

如果只想快速测试 importer，可以使用 ECDICT 的 mini 数据：

```console
python3 tools/import-ecdict/import_ecdict.py \
  --source third_party/ECDICT/ecdict.mini.csv \
  --output applet/contents/data/ecdict.sqlite
```

## 从源码树测试

开发时通常不需要安装小组件，可以直接从源码树启动：

```console
QML_IMPORT_PATH=build/src plasmoidviewer -a applet
```

如果修改了 C++ 代码，重新构建：

```console
cmake --build build
```

## 可选 KWin 鼠标点击辅助（默认开启）

Wayland 下 primary selection 的提供方有时不会在“视觉上取消选中”时清空 selection。
因此仅靠 primary-selection 协议有时可能会读到上一次选中的文本。

本项目提供一个可选 KWin effect helper（默认开启）：

```text
kwin-helper/
```

它在 KWin 内部监听全局鼠标按下事件，并通过 session D-Bus 发送信号给小组件。
小组件如果收不到该信号，会继续使用内置 primary-selection 读取逻辑，不影响运行。

构建辅助 effect：

```console
cmake -B build -S . -DSWAN_DICT_BUILD_KWIN_HELPER=ON
cmake --build build
sudo cmake --install build
```

安装后，将在词典启动时自动启动（可配置不自动启动），也可以通过 KWin D-Bus 加载 effect：

```console
qdbus6 org.kde.KWin /Effects loadEffect swandictmousehelper
```

## 安装

系统安装：

```console
sudo cmake --install build
```

开发时也可以只安装或升级 applet 包：

```console
kpackagetool6 --type Plasma/Applet --install applet
kpackagetool6 --type Plasma/Applet --upgrade applet
```

注意：原生 QML 插件 `.so` 由 CMake 安装，不建议手动复制到 applet 目录。

## Digital Clock 同步机制

本项目不直接维护完整的 KDE Digital Clock 源码副本。

系统安装的 Digital Clock 通常位于：

```text
/usr/share/plasma/plasmoids/org.kde.plasma.digitalclock/contents
```

同步脚本：

```console
scripts/sync-digital-clock.sh
```

默认不会覆盖已有生成文件。需要显式设置环境变量才会覆盖：

```console
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 scripts/sync-digital-clock.sh
```

也可以指定其它 Digital Clock 源码位置：

```console
SWAN_DICT_DIGITAL_CLOCK_SOURCE=/path/to/digital-clock/contents \
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 \
scripts/sync-digital-clock.sh
```

这样做是为了让本项目复制目标发行版自己的 Digital Clock QML，从而尽量匹配：

```qml
import org.kde.plasma.private.digitalclock
```

## 补丁工作流

上游 Digital Clock 生成文件不直接作为 canonical source。

对这些文件的修改保存为补丁：

```text
patches/0001-digital-clock-qml-date-label.patch
patches/0002-tooltip-qml-dictionary-content.patch
patches/0003-main-qml-swan-dict-wiring.patch
patches/0004-config-qml-translation-page.patch
patches/0005-main-xml-translation-settings.patch
```

修改以下生成文件后，需要重新生成补丁：

```console
scripts/regenerate-patches.sh
```

项目自有 QML 文件直接提交，不需要生成补丁：

```text
applet/contents/ui/DictionaryPopup.qml
applet/contents/ui/configTranslation.qml
```

## 配置项

小组件配置中新增了 Translation 页面：

- Dictionary selection limit：本地词典处理选区的长度上限，默认 `128`。
- Date replacement length：紧凑视图日期替换文本长度，默认 `10`。
- KWin mouse helper：启动小组件时尝试加载可选 KWin 鼠标点击辅助 effect，默认开启。
- Sentence translation：是否允许 DeepSeek 整句翻译。
- DeepSeek API key：DeepSeek API key。

DeepSeek 翻译只会在点击弹窗中通过按钮手动触发。

它不会在悬停时自动调用，也不会在弹窗打开时自动调用。

## 国际化

KDE 翻译提取脚本：

```text
Messages.sh
```

简体中文翻译文件：

```text
po/zh_CN/plasma_applet_com.github.LXYan2333.swan-dict.po
```

## OBS 打包

OBS 相关说明见：

```text
packaging/obs/README.md
```

推荐策略：

- OBS 构建环境安装目标发行版自己的 `plasma-workspace`。
- 构建时从系统 Digital Clock 复制 QML 并应用本项目补丁。
- 构建时从 `third_party/ECDICT/ecdict.csv` 生成 SQLite 数据库。
- source package 不包含 `build/` 和 `applet/contents/data/ecdict.sqlite`。

## 许可证

小组件代码遵循 GPL-2.0-or-later。

ECDICT 数据遵循其上游项目许可证。
