# Swan Dict 开发文档

本文面向项目开发者，说明 Swan Dict 的源码结构、构建生成物、运行时组件，以及数据如何在各组件之间流动。

## 项目定位

Swan Dict 是一个基于 KDE Plasma 6 Digital Clock 改造的小组件。它保留 Digital Clock 的主要结构，在日期显示位置加入 primary selection 词典查询结果，并在悬停或点击时显示更完整的词典内容。

项目不是纯 QML applet。它由四类代码共同组成：

- Plasma applet package：面板 UI、悬停提示、点击弹窗、配置页。
- Native QML plugin：C++ 提供 primary selection 监听、词典查询、DeepSeek 请求、剪贴板写入。
- ECDICT importer：把 ECDICT CSV 转成运行时 SQLite 数据库。
- Optional KWin helper：在 Wayland 下辅助检测用户鼠标点击，以便清理过期选区翻译。

## 目录结构

```text
applet/
```

Plasma applet 包。最终通过 `plasma_install_package()` 安装为：

```text
com.github.LXYan2333.swan-dict
```

其中：

- `applet/metadata.json`：Plasma 小组件元数据。
- `applet/contents/config/`：Digital Clock 配置文件，来自上游 Digital Clock 并应用补丁。
- `applet/contents/ui/`：QML UI 文件。
- `applet/contents/data/ecdict.sqlite`：构建时生成的 SQLite 词典数据库，不提交到 git。

```text
src/
```

原生 QML 插件源码，URI 为：

```qml
import com.github.LXYan2333.SwanDict
```

主要 C++ 类型：

- `SelectionWatcher`：读取 primary selection。
- `Translator`：查询本地 SQLite 词典，并处理 DeepSeek 句子翻译请求。
- `ClipboardWriter`：写入纯文本和富文本剪贴板内容。
- `WaylandPrimarySelection`：Wayland primary selection client 实现。

```text
kwin-helper/
```

可选 KWin effect。它监听 KWin 内部鼠标按下事件，并通过 session D-Bus 通知 applet。

```text
tools/import-ecdict/
```

ECDICT CSV 到 SQLite 的导入脚本。

```text
third_party/ECDICT/
```

ECDICT git submodule。运行时不会直接读取 CSV，而是读取构建生成的 SQLite。

```text
third_party/wl-clipboard-protocols/
```

项目复制的 wlr data control protocol XML。用于生成 Wayland client protocol 代码。

```text
patches/
patches/arch-plasma-6.6.5/
```

对上游 Digital Clock 生成文件的补丁。普通发行版使用 `patches/`。Arch OBS fallback 使用 `patches/arch-plasma-6.6.5/`。

```text
scripts/
```

同步和维护脚本：

- `sync-digital-clock.sh`：从系统 Digital Clock 复制 QML/config 并应用补丁。
- `regenerate-patches.sh`：从当前 applet 生成文件反向生成补丁。
- `prepare-arch-digital-clock-fallback.sh`：为 Arch OBS source archive 准备 Digital Clock fallback。

```text
packaging/
```

OBS、Debian、RPM、Arch 打包相关文件。

## Applet QML 文件分工

上游 Digital Clock 生成文件不是 canonical source。它们通常被 `.gitignore` 忽略，并由脚本复制后打补丁生成。

常见生成文件：

- `applet/contents/ui/main.qml`
- `applet/contents/ui/DigitalClock.qml`
- `applet/contents/ui/Tooltip.qml`
- `applet/contents/ui/CalendarView.qml`
- `applet/contents/ui/configAppearance.qml`
- `applet/contents/ui/configCalendar.qml`
- `applet/contents/ui/configTimeZones.qml`
- `applet/contents/config/main.xml`

项目自有 QML 文件直接提交：

- `DictionaryTooltip.qml`：悬停提示中的词典内容。
- `DictionaryPopup.qml`：点击弹窗中的词典内容、DeepSeek 按钮、复制按钮。
- `SwanDictController.qml`：连接 selection watcher、translator 和配置的控制器。
- `configTranslation.qml`：新增 Translation 配置页。

开发原则：

- 修改上游 Digital Clock 生成文件后，需要运行 `scripts/regenerate-patches.sh`。
- 修改项目自有 QML 文件，不需要重新生成补丁。
- 尽量把复杂词典 UI 放在项目自有 QML 文件中，让补丁只负责接入点。

## 构建时数据流

### Digital Clock QML 生成

构建配置阶段，顶层 `CMakeLists.txt` 会运行：

```text
scripts/sync-digital-clock.sh
```

数据流：

```text
系统 Digital Clock contents
  -> scripts/sync-digital-clock.sh
  -> applet/contents/config/
  -> applet/contents/ui/*.qml
  -> patches/*.patch
  -> patched Swan Dict applet
```

默认情况下，如果已有生成文件，脚本不会覆盖。要显式同步，需要：

```console
SWAN_DICT_SYNC_DIGITAL_CLOCK_OVERWRITE=1 scripts/sync-digital-clock.sh
```

这样做是为了让目标发行版自己的 Digital Clock QML 与其 `org.kde.plasma.private.digitalclock` 私有模块保持匹配。

### Wayland protocol 代码生成

`src/CMakeLists.txt` 使用 `wayland-scanner` 生成 client protocol 代码：

```text
primary-selection-unstable-v1.xml
ext-data-control-v1.xml
wlr-data-control-unstable-v1.xml
```

数据流：

```text
Wayland protocol XML
  -> wayland-scanner
  -> build/src/*-client-protocol.h
  -> build/src/*-protocol.c
  -> WaylandPrimarySelection
```

`WaylandPrimarySelection` 在运行时根据 compositor 暴露的 global protocol 选择可用后端。

### ECDICT 数据库生成

构建阶段默认启用：

```text
SWAN_DICT_GENERATE_DICTIONARY=ON
```

数据流：

```text
third_party/ECDICT/ecdict.csv
  -> tools/import-ecdict/import_ecdict.py
  -> applet/contents/data/ecdict.sqlite
  -> Translator
```

导入脚本只保留运行时使用的字段：

- `word`
- `sw`
- `phonetic`
- `definition`
- `translation`
- `bnc`
- `frq`
- `exchange`

未使用字段如 `collins`、`oxford`、`tag`、`pos` 不进入 SQLite，以减小数据库体积。

导入时还会做文本规范化：

- 把 CSV 中转义的 `\n`、`\r` 转成真实换行。
- 修正行首无点 POS，例如 `v be...` 规范成 `v. be...`。
- 将硬换行的续行合并回上一条释义。
- 将 `[计]` 这类行首领域标签视为可拆分的释义标记。

## 运行时组件

### SwanDictController.qml

`SwanDictController.qml` 是 QML 层的状态中心。它创建：

- `SelectionWatcher`
- `Translator`

并维护：

- `dictEntry`
- `sentenceTranslation`
- `sentenceTranslationBusy`
- `sentenceTranslationError`
- `sentenceTranslationRequested`

它的职责：

- 监听 `SelectionWatcher.textChanged`。
- 调用 `Translator.lookup()` 更新词典结果。
- 根据配置传递 `maximumSelectionLength` 和 `dateReplacementTranslationMaximumLength`。
- 在点击弹窗展开时暂停 KWin 鼠标点击清理。
- 在弹窗关闭或 selection 改变时取消 DeepSeek 请求。
- 启动可选 KWin helper。

### SelectionWatcher

`SelectionWatcher` 暴露给 QML 的主要属性和方法：

- `text`：当前有效 selected text。
- `interval`：X11 fallback 轮询间隔。
- `kWinMouseClearingSuspended`：弹窗打开时暂停鼠标点击清理。
- `ignoreNextKWinMouseClick()`：点击小组件自身按钮时忽略短时间内的 KWin mouse signal。
- `clearCurrentSelection()`：主动清空当前 UI selection 状态。
- `startKWinMouseHelper()`：通过 D-Bus 请求 KWin 加载 helper effect。

运行时后端：

- Wayland：使用 `WaylandPrimarySelection`。
- X11：使用 `QClipboard::Selection`。

Wayland 下 primary selection provider 有时不会在视觉取消选择时发出空 selection。项目用 KWin helper 的鼠标点击信号作为辅助：用户在选中文本后点击其它地方，helper 发出 signal，`SelectionWatcher` 延迟到双击间隔后清空当前显示结果。延迟使用 `QStyleHints::mouseDoubleClickInterval()`，避免双击选词时日期区域频繁闪烁。

### WaylandPrimarySelection

`WaylandPrimarySelection` 是 in-process primary selection client。它不再依赖外部 `wl-paste` 命令。

它负责：

- 连接 Wayland display。
- 监听 primary selection 相关 global。
- 在 selection 改变时读取 `text/plain` 内容。
- 通过 Qt signal 把文本交给 `SelectionWatcher`。

### Translator

`Translator` 暴露给 QML 的主要属性：

- `databaseUrl`
- `maximumSelectionLength`
- `dateReplacementMaximumLength`

主要方法：

- `lookup(selectedText)`：同步查询本地 SQLite，返回 `QVariantMap`。
- `translateSentenceWithDeepSeek(selectedText, apiKey)`：异步调用 DeepSeek API。
- `cancelSentenceTranslation()`：取消当前 DeepSeek 请求。

`lookup()` 返回的数据会被 QML 当作普通 JavaScript object 使用。常见字段包括：

- `query`：原始查询文本。
- `matchedWord`：命中的词。
- `phonetic`：音标，不存在时为空。
- `translation`：中文释义文本。
- `definition`：英文释义文本。
- `exchange`：原始变形字段。
- `summary`：日期区域使用的紧凑摘要。
- `fullText`：复制或 fallback 展示用的完整文本。
- `translationRows`：已拆 POS 的中文释义行。
- `definitionRows`：已拆 POS 的英文释义行。
- `exchangeRows`：变形表行。
- `isSplitEntry`：是否为多词拆分结果。
- `entries`：多词查询中每个词的完整 entry。

### ClipboardWriter

`ClipboardWriter` 提供：

- `setText(plainText)`
- `setRichText(plainText, htmlText)`

点击弹窗中的复制按钮会同时写入纯文本和 HTML。这样粘贴到纯文本编辑器时得到普通文本，粘贴到支持 rich text 的编辑器时可以保留标题、表格和淡化样式。

### KWin helper

`kwin-helper/swandictmousehelper.cpp` 是一个 KWin effect。它连接：

```text
KWin::input()->pointerButtonStateChanged
```

当鼠标按下时，它发送 session D-Bus signal：

```text
/com/github/LXYan2333/SwanDict/KWinHelper
com.github.LXYan2333.SwanDict.KWinHelper.mouseClicked
```

`SelectionWatcher` 监听该 signal。helper 必须是 best-effort：没有安装、加载失败、非 KWin 环境都不能阻止小组件运行。

## 运行时数据流

### 单词选中到日期区域

```text
用户在应用中选中单词
  -> compositor / X11 selection owner
  -> SelectionWatcher
  -> SwanDictController.updateDictEntry()
  -> Translator.lookup()
  -> SQLite entries 表
  -> dictEntry.summary
  -> DigitalClock.qml 日期替换文本
```

日期区域只显示紧凑结果。它会使用 `dateReplacementTranslationMaximumLength` 截断，并用 `…` 表示省略。为了避免面板宽度随着选中文本变化，日期和翻译共用固定宽度配置。

### 多词选中到本地词典结果

```text
用户选中多个词
  -> SelectionWatcher.text
  -> Translator.lookup()
  -> normalizeQuery()
  -> limitedQueryRespectingWordBoundary()
  -> splitQueryWords()
  -> lookupCandidate() per word
  -> buildSplitEntry()
  -> QML dictEntry.entries
```

如果 selection 超过 `maximumSelectionLength`：

- 查询不会硬切断当前单词。
- 会延伸到当前单词末尾。
- UI 会显示红色加粗 warning heading。

多词时：

- 日期区域仍保持简短。
- 悬停 tooltip 展示每个词的完整本地词典内容。
- 点击 popup 展示每个词的完整本地词典内容。

### 悬停 tooltip 数据流

```text
dictEntry
  -> Tooltip.qml patched entry point
  -> DictionaryTooltip.qml
  -> Kirigami/Plasma labels
```

`DictionaryTooltip.qml` 只负责展示本地词典结果，不触发 DeepSeek。这样悬停不会消耗 API token。

tooltip 主要展示：

- 单词 heading。
- 音标，格式为 `/phonetic/`。
- 中文释义 POS 行。
- 英文释义 POS 行。
- exchange 变形表。
- selection exceed warning。

### 点击 popup 数据流

```text
用户点击小组件
  -> main.qml / Digital Clock popup
  -> SwanDictController.expanded = true
  -> DictionaryPopup.qml
  -> dictEntry / sentenceTranslation state
```

弹窗打开时：

- `SwanDictController.expanded` 为 true。
- `SelectionWatcher.kWinMouseClearingSuspended` 为 true，避免点击弹窗内部按钮导致 selection 被清掉。
- 点击 copy 或 DeepSeek 按钮时，QML 也会短时间调用 `ignoreNextKWinMouseClick()`。

弹窗关闭时：

- 清理 DeepSeek 结果和错误状态。
- 取消未完成 DeepSeek 请求。
- 主动清空当前 UI selection 状态，让日期恢复正常。

### DeepSeek 句子翻译数据流

DeepSeek 只在满足以下条件时可用：

- 当前 selection 是多词结果。
- 配置启用 DeepSeek sentence translation。
- 配置中存在 API key。
- 用户在点击弹窗中手动按下翻译按钮。

数据流：

```text
DictionaryPopup.qml button
  -> SwanDictController.requestSentenceTranslation()
  -> Translator.translateSentenceWithDeepSeek()
  -> QNetworkAccessManager POST https://api.deepseek.com/chat/completions
  -> sentenceTranslationReady / sentenceTranslationFailed
  -> DictionaryPopup.qml
```

它不会：

- 在 hover 时触发。
- 在 popup 打开时自动触发。
- 在 selection 改变后继续使用旧结果。

API key 存在 applet 配置中，不存在 KWallet 中。

### 复制按钮数据流

```text
用户点击复制按钮
  -> DictionaryPopup.copyEntry()
  -> copyTextForEntry()
  -> copyHtmlForEntry()
  -> ClipboardWriter.setRichText()
  -> QClipboard
```

复制内容包括：

- 单词。
- 音标。
- 中文释义。
- 英文释义。
- exchange 变形表。

纯文本编辑器会得到普通文本。富文本编辑器可以得到 HTML heading、表格和淡化 POS 样式。

## SQLite 查询和词条选择

SQLite 表：

```sql
CREATE TABLE entries (
    word TEXT PRIMARY KEY,
    sw TEXT NOT NULL,
    phonetic TEXT,
    definition TEXT,
    translation TEXT,
    bnc INTEGER,
    frq INTEGER,
    exchange TEXT
) WITHOUT ROWID;
```

索引：

```sql
CREATE INDEX entries_sw_idx ON entries(sw);
```

`sw` 是简化搜索 key。运行时查询会生成 candidate words，例如：

- 原词小写。
- 去掉标点后的词。
- 可能的词形候选。

`Translator` 根据候选词和 `sw` 查找词条，并把原始字段转换成更适合 QML 展示的结构。

## POS、translation、definition 和 exchange

导入脚本负责尽量把 ECDICT 中不规范的 POS 行修正成统一格式。运行时不应该再大规模修补这类数据问题。

`Translator` 在构建 entry 时会把 translation 和 definition 拆成行：

```text
pos -> text
```

QML 中 POS 使用低透明度展示，释义正文正常展示。

`exchange` 原始格式类似：

```text
d:perceived/p:perceived/3:perceives/i:perceiving
```

运行时转换为 `exchangeRows`，并使用中文标签：

- `p`：过去式（did）
- `d`：过去分词（done）
- `i`：现在分词（doing）
- `3`：第三人称单数（does）
- `r`：形容词比较级（-er）
- `t`：形容词最高级（-est）
- `s`：名词复数形式
- `0`：Lemma
- `1`：Lemma 的变换形式

exchange 使用独立两列表格，不与 translation/definition 的 POS 列对齐。

## 配置数据流

配置项定义在 patched `applet/contents/config/main.xml` 中，UI 在 `configTranslation.qml` 中。

主要配置：

- `maximumSelectionLength`：本地词典处理选区长度上限，默认 `128`。
- `dateReplacementTranslationMaximumLength`：日期替换文本最大长度，默认 `10`。
- `dateReplacementFixedWidth`：日期和翻译共用固定宽度，默认 `70`。
- `startKWinMouseHelper`：启动时尝试加载 KWin helper，默认开启。
- `enableDeepSeekSentenceTranslation`：是否允许手动 DeepSeek 句子翻译。
- `deepSeekApiKey`：DeepSeek API key。

配置流向：

```text
configTranslation.qml
  -> applet configuration object
  -> SwanDictController.configuration
  -> SelectionWatcher / Translator / DictionaryPopup
```

## i18n 数据流

翻译 catalog：

```text
plasma_applet_com.github.LXYan2333.swan-dict
```

QML 使用：

```qml
i18nd("plasma_applet_com.github.LXYan2333.swan-dict", "Text")
```

C++ 使用 `KLocalizedString`：

```cpp
i18n("Text")
```

翻译文件位于：

```text
po/zh_CN/plasma_applet_com.github.LXYan2333.swan-dict.po
```

安装由顶层 CMake 的 `ki18n_install(po)` 完成。

## 安装和运行路径

正式安装需要系统级安装，原因是 Plasma 面板进程需要在默认 QML import path 中找到原生 QML 插件：

```text
com/github/LXYan2333/SwanDict/
```

`src/CMakeLists.txt` 会把插件安装到：

```text
${KDE_INSTALL_LIBDIR}/qt6/qml/com/github/LXYan2333/SwanDict
```

applet package 通过 `plasma_install_package()` 安装。

开发测试可以临时使用：

```console
QML_IMPORT_PATH=build/src plasmoidviewer -a applet
```

这只影响当前命令，不影响正在运行的 Plasma panel。

## OBS 和发行版构建

OBS 构建需要包含：

- 项目源码。
- patch files。
- `third_party/ECDICT/ecdict.csv`。
- Arch fallback Digital Clock QML/config。

不应包含：

- `build/`
- `applet/contents/data/ecdict.sqlite`
- 本机生成的临时二进制包。

自动更新 OBS source 的脚本：

```text
packaging/obs/update-obs-sources.sh
```

GitHub Actions workflow：

```text
.github/workflows/obs.yml
```

当 push `v*` tag 或手动触发 workflow 时，GitHub Actions 会生成 source artifacts 并提交到 OBS。

## 常见开发任务

### 修改项目自有 QML

直接修改：

```text
applet/contents/ui/DictionaryTooltip.qml
applet/contents/ui/DictionaryPopup.qml
applet/contents/ui/SwanDictController.qml
applet/contents/ui/configTranslation.qml
```

不需要运行 `regenerate-patches.sh`。

### 修改上游 Digital Clock 接入点

修改生成文件后运行：

```console
scripts/regenerate-patches.sh
```

这会更新 `patches/*.patch`。注意不要让无关 patch 文件因为时间戳或格式噪声被污染。

### 修改 C++ 插件

修改 `src/` 后重新构建：

```console
cmake --build build
```

源码树测试：

```console
QML_IMPORT_PATH=build/src plasmoidviewer -a applet
```

### 修改 ECDICT importer

可以用 mini CSV 快速验证：

```console
python3 tools/import-ecdict/import_ecdict.py \
  --source third_party/ECDICT/ecdict.mini.csv \
  --output /tmp/ecdict.sqlite
```

正式数据库由 CMake 生成：

```console
cmake --build build
```

### 修改 KWin helper

需要重新构建并系统安装：

```console
cmake --build build
sudo cmake --install build
```

加载 helper：

```console
qdbus6 org.kde.KWin /Effects loadEffect swandictmousehelper
```

helper 必须保持可选。不要让 applet 因 helper 不存在而失败。

## 设计约束

- 不再依赖外部 `wl-paste` 进程读取 primary selection。
- DeepSeek 不应在 hover 或 popup 打开时自动调用。
- 词典数据库生成应尽量在 importer 阶段完成规范化，避免运行时补丁式修复数据。
- Digital Clock 上游文件通过 patch workflow 维护，不直接把复制文件当作项目 canonical source。
- KWin helper 是平台特定增强功能，但不能成为运行必需依赖。
- UI 应优先使用 KDE/Kirigami/Plasma 组件。
