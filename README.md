# 图书馆借阅管理系统

基于 **Qt 6 + MySQL** 的图书借阅管理程序：图书录入与维护、借阅登记、归还办理、
借阅/归还流水总览。

项目经过一次完整的结构重构：从「所有代码平铺在根目录、界面与 SQL 混在一起」的形态，
整理为 **core / data / service / ui 四层**，并补齐了配置化、测试与文档。

---

## 一、快速开始

### 1. 准备数据库

```bash
mysql -u root -p < sql/schema.sql
```

脚本是幂等的，会创建 `book_system` 库及 `book` / `lend_records` / `return_records` 三张表。

> 如果沿用旧库也无需手工建表：程序启动时会自动补上缺失的 `book.available` 列，
> 并按现有借阅记录回填可借数量。

### 2. 配置连接

编辑根目录的 `config.ini`：

```ini
[Database]
HostName=127.0.0.1
Port=3306
UserName=root
Password=你的密码
DatabaseName=book_system
ConnectTimeout=5
```

程序会先找**可执行文件同目录**的 `config.ini`，找不到就自该目录**逐级向上**最多 4 层查找，
因此直接在 `build/` 里运行也能读到项目根目录的配置。

也可以用环境变量临时覆盖，便于连接测试库而无需改文件：

| 变量 | 说明 |
| --- | --- |
| `LIBMS_DB_HOST` / `LIBMS_DB_PORT` | 主机与端口 |
| `LIBMS_DB_USER` / `LIBMS_DB_PASSWORD` | 账号与密码 |
| `LIBMS_DB_NAME` | 数据库名 |

### 3. 构建

**Qt Creator**：直接用 `CMakeLists.txt` 打开项目即可（推荐 Kit：Desktop Qt 6.x MinGW 64-bit）。

**命令行（CMake ≥ 3.21）**：

```bash
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build        # 可选：部署 Qt 运行库与 config.ini
```

> 运行时需要 `qsqlmysql.dll` 驱动以及 `libmysql.dll`。
> Qt 官方 MinGW 版通常已自带；用 `windeployqt` 或 `cmake --install` 可一并部署。

### 4. 测试

测试需要可用的 MySQL（凭据同样取自 `config.ini`）。

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| 测试 | 覆盖内容 |
| --- | --- |
| `library_service_tests` | ISBN/姓名校验规则；录入、修改、删除的业务约束；借出与归还的事务完整性（库存扣减与回冲、超量拦截、重复归还拦截） |
| `gui_smoke_tests` | 真实构造整个界面；主菜单到 5 个功能页的**导航是否正确**；未选中行时操作按钮为禁用；运行期无 Qt 告警 |

两个测试都会把结果同时写入 `test-results.txt` / `gui-smoke-results.txt`，便于排查。

---

## 二、架构

```
                ┌──────────────────────────────────────────────┐
   UI 层         │ MainWindow ─ HomePage / AddBookPage /        │  只做展示与交互
  (src/ui)       │ ManageBooksPage / LendBookPage /             │  不写 SQL
                │ ReturnBookPage / OverviewPage                │
                │ dialogs/  widgets/  models/                  │
                └───────────────────┬──────────────────────────┘
                                    │ 调用 LibraryService，拿到 Result
                ┌───────────────────▼──────────────────────────┐
   业务层        │ LibraryService                               │  校验规则
  (src/service) │  · 输入校验（ISBN、姓名、数量区间）           │  事务边界
                │  · 事务（借出/归还全成功或全回滚）            │  不弹窗
                └───────────────────┬──────────────────────────┘
                                    │ 调用 Repository
                ┌───────────────────▼──────────────────────────┐
   数据层        │ BookRepository / LoanRepository / Database    │  只写 SQL
  (src/data)    │  · 预处理语句绑定参数，杜绝 SQL 注入          │  行 ↔ 结构体映射
                │  · Database 负责连接与表结构幂等升级          │
                └───────────────────┬──────────────────────────┘
                                    │
                ┌───────────────────▼──────────────────────────┐
   领域层        │ Book / LendRecord / ReturnRecord             │  纯数据
  (src/core)    │ Result<T> / VoidResult                       │  无 Qt Widgets 依赖
                │ AppConfig / TimeUtil / SearchMode            │
                └──────────────────────────────────────────────┘
```

**约定**

- 数据层与业务层**从不弹窗**，统一返回 `Result<T>`；由界面决定如何呈现。因此业务逻辑可被测试复用。
- 界面层**不出现 SQL**，也不持有 `QSqlDatabase`。
- 写操作全部使用预处理语句绑定参数。
- 借出、归还这类跨表操作由业务层开启事务，避免出现「扣了库存却没有借阅记录」的中间状态。

### 目录结构

```
├── CMakeLists.txt
├── config.ini                 # 运行配置（数据库连接）
├── .clang-format              # 代码风格（4 空格、110 列）
├── resources/
│   ├── resources.qrc
│   └── styles/app.qss         # 全局样式表
├── sql/
│   └── schema.sql             # 建表脚本（幂等）
├── src/
│   ├── main.cpp
│   ├── core/                  # 领域模型与基础设施
│   │   ├── Result.h           # Result<T> / VoidResult
│   │   ├── Book.h  LendRecord.h  ReturnRecord.h
│   │   ├── SearchMode.h  TimeUtil.h
│   │   ├── AppConfig.h/.cpp   # config.ini + 环境变量
│   ├── data/                  # 数据访问层
│   │   ├── Database.h/.cpp    # 连接与表结构升级
│   │   ├── BookRepository.h/.cpp
│   │   └── LoanRepository.h/.cpp
│   ├── service/
│   │   └── LibraryService.h/.cpp
│   └── ui/
│       ├── MainWindow.h/.cpp/.ui   # 组装对象图、页面栈与导航
│       ├── PageId.h                # 具名页面标识
│       ├── models/                 # 表格模型（强类型，非 QSqlQueryModel）
│       ├── widgets/                # PageHeader 工具
│       ├── dialogs/                # BookEditDialog、ReturnBookDialog
│       └── pages/                  # 6 个页面（每个含 .h/.cpp/.ui）
├── tests/
│   ├── TestRunner.h/.cpp
│   ├── test_library_service.cpp
│   └── test_gui_smoke.cpp
└── tools/
    └── build.ps1              # 不经 CMake 的直连编译脚本（沙箱/应急用）
```

---

## 三、功能说明

| 页面 | 能力 |
| --- | --- |
| 添加图书 | 录入 ISBN（13 位数字）、书名、作者、馆藏总量；ISBN 唯一性由数据库与业务层双重保证 |
| 管理已有图书 | 下拉选择「按书名模糊」或「按 ISBN 精确」检索；双击行或点按钮修改；删除需二次确认 |
| 借阅图书 | 登记借阅人与数量；右侧列出馆藏，双击自动填入 ISBN；库存不足时拒绝借出 |
| 归还图书 | 仅列出**尚未还清**的借阅记录；支持部分归还；归还数量上限即未还数量 |
| 出借与归还总览 | 标签页分别展示全部借阅记录与归还流水，顶部给出汇总统计 |

**库存语义**：`book.total` 是馆藏总量，`book.available` 是当前可借数量，
借出时 `available -= n`，归还时 `available += n`，约束恒为 `0 <= available <= total`。
更新写语句带条件（`available + delta BETWEEN 0 AND total`），因此并发下也不会超借或超还。

---

## 四、本次重构的主要改动

### 结构

| 之前 | 之后 |
| --- | --- |
| 14 个源文件平铺在根目录 | `src/{core,data,service,ui}` 分层，界面文件与源码同目录 |
| `mysql.h/.cpp` 里的 `class database` 既管连接又管 SQL，还接收 `QTableView*` / `QMainWindow*` | `Database` 只管连接与表结构；`BookRepository` / `LoanRepository` 只管 SQL 与行映射 |
| 界面回调直接调 SQL 相关方法 | 统一经 `LibraryService`，业务规则集中可测 |
| `mainwindow.cpp` 158 行 lambda 堆叠在构造函数里 | 拆成 6 个独立页面类，各自维护 `.ui`；`MainWindow` 只负责组装与导航 |
| 返回按钮、标题文案在 5 个页面重复硬编码 | `PageHeader` 统一处理 |
| `return_button.h/.cpp` 完全未被使用（include 被注释） | 已删除 |
| `Dialog::accepted()` 槽函数带返回值（Qt 中永不生效） | 消除，改为明确的 `validateAndAccept()` |
| `widgetwheel` / `qwheelstacked` 用滚轮在两个输入框间隐式切换检索方式 | 删除该控件，改为显式下拉框，用户可直接看到当前检索方式 |
| `PageHeader` 曾是自定义控件但从未被实例化 | 收敛为 `PageHeader::applyTo()` 工具函数，不再有无用信号 |

### 缺陷修复

1. **导航错位**：原先 `jump3`（借阅图书）与 `jump4`（归还图书）都指向索引 4，
   导致「归还图书」打开的是借阅页、「出借总览」打开的是归还页。
   现改为按 `PageId` 具名寻址，并由 GUI 冒烟测试逐页断言。
2. **内存泄漏**：`new QSqlQuery`、`new QSqlQueryModel` 在多个分支里从未释放。
   现在查询对象一律栈上构造，模型使用强类型表格模型并指定父对象。
3. **借阅信息不完整仍继续执行**：原先弹出「借书信息不完整」后并未返回，
   依旧会插入一条空记录。现在校验失败即返回。
4. **搜索条件静默失效**：原先书名与 ISBN 同时填写时会走到 `else` 分支，
   既不绑定参数也不提示，返回一个未执行的查询。现在检索方式显式二选一，并对 ISBN 做格式校验。
5. **硬编码凭据**：数据库账号密码原先写死在源码里，`config.ini` 形同虚设。
   现在由 `AppConfig` 读取，支持环境变量覆盖。
6. **无端的安全感**：原先查询失败只在 `qDebug` 里留一行日志，界面显示空表格。
   现在所有失败路径都返回可读的中文原因，由界面提示。
7. **弹窗错位**：`delbook` 用 `QTableView*` 作为 `QMessageBox` 的父窗口，
   现在由页面负责提示，父窗口正确。

### 功能增强

- `book.available` 可借数量：借出扣减、归还回冲，删除图书前校验是否有未归还记录。
- 管理页新增 **修改** 按钮（原先只能靠双击，无任何提示）。
- 删除图书增加二次确认。
- 归还支持**部分归还**，并拦截「已还清后重复归还」。
- 归还记录关联借阅记录，展示借阅人、ISBN、借出时间等上下文。
- 全局样式表 `resources/styles/app.qss`，统一配色、控件与表格外观。
- 启动时若数据库不可用，明确告知原因后退出，而不是进入一个所有操作都会失败的界面。

---

## 五、备注

- `_legacy/` 保存了重构前的原始源文件，仅作对照与回退参考，不参与构建，可随时删除。
- `tools/build.ps1` 是不经 CMake 的直连编译脚本（自行驱动 `uic`/`moc`/`rcc` 与 `g++`），
  用于 CMake 配置阶段不可用的环境；日常开发请使用 Qt Creator 或 `cmake`。
