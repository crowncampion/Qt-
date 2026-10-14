cmake_minimum_required(VERSION 3.21)

project(mysql_window
    VERSION 2.0.0
    DESCRIPTION "图书馆借阅管理系统"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# 构建选项
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 源文件已统一使用 UTF-8，显式告知 MSVC 以免中文字符串乱码。
if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Sql)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# 源文件：按分层组织
#   core    领域模型与基础设施（无 Qt Widgets 依赖）
#   data    数据访问层（SQL 与行映射）
#   service 业务层（校验与事务）
#   ui      界面层（页面、对话框、模型、控件）
# ---------------------------------------------------------------------------

set(CORE_SOURCES
    src/core/AppConfig.cpp
    src/core/AppConfig.h
    src/core/Book.h
    src/core/LendRecord.h
    src/core/Result.h
    src/core/ReturnRecord.h
    src/core/SearchMode.h
    src/core/TimeUtil.h
)

set(DATA_SOURCES
    src/data/BookRepository.cpp
    src/data/BookRepository.h
    src/data/Database.cpp
    src/data/Database.h
    src/data/LoanRepository.cpp
    src/data/LoanRepository.h
)

set(SERVICE_SOURCES
    src/service/LibraryService.cpp
    src/service/LibraryService.h
)

set(UI_MODEL_SOURCES
    src/ui/models/BookTableModel.cpp
    src/ui/models/BookTableModel.h
    src/ui/models/LoanRecordTableModel.cpp
    src/ui/models/LoanRecordTableModel.h
    src/ui/models/ReturnRecordTableModel.cpp
    src/ui/models/ReturnRecordTableModel.h
)

set(UI_WIDGET_SOURCES
    src/ui/widgets/PageHeader.cpp
    src/ui/widgets/PageHeader.h
)

set(UI_DIALOG_SOURCES
    src/ui/dialogs/BookEditDialog.cpp
    src/ui/dialogs/BookEditDialog.h
    src/ui/dialogs/BookEditDialog.ui
    src/ui/dialogs/ReturnBookDialog.cpp
    src/ui/dialogs/ReturnBookDialog.h
    src/ui/dialogs/ReturnBookDialog.ui
)

set(UI_PAGE_SOURCES
    src/ui/pages/Page.cpp
    src/ui/pages/Page.h
    src/ui/pages/HomePage.cpp
    src/ui/pages/HomePage.h
    src/ui/pages/HomePage.ui
    src/ui/pages/AddBookPage.cpp
    src/ui/pages/AddBookPage.h
    src/ui/pages/AddBookPage.ui
    src/ui/pages/ManageBooksPage.cpp
    src/ui/pages/ManageBooksPage.h
    src/ui/pages/ManageBooksPage.ui
    src/ui/pages/LendBookPage.cpp
    src/ui/pages/LendBookPage.h
    src/ui/pages/LendBookPage.ui
    src/ui/pages/ReturnBookPage.cpp
    src/ui/pages/ReturnBookPage.h
    src/ui/pages/ReturnBookPage.ui
    src/ui/pages/OverviewPage.cpp
    src/ui/pages/OverviewPage.h
    src/ui/pages/OverviewPage.ui
)

set(UI_SHELL_SOURCES
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.ui
    src/ui/PageId.h
)

# ---------------------------------------------------------------------------
# 可执行目标
# ---------------------------------------------------------------------------

qt_add_executable(mysql_window
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    ${CORE_SOURCES}
    ${DATA_SOURCES}
    ${SERVICE_SOURCES}
    ${UI_MODEL_SOURCES}
    ${UI_WIDGET_SOURCES}
    ${UI_DIALOG_SOURCES}
    ${UI_PAGE_SOURCES}
    ${UI_SHELL_SOURCES}
)

# 让 #include "core/..." 这类包含路径以 src 为根，避免使用相对路径上跳。
target_include_directories(mysql_window PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 样式表等资源；由 qt_add_resources 生成，无需 AUTORCC。
qt_add_resources(mysql_window "app_resources"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/styles/app.qss
)

target_link_libraries(mysql_window PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
)

# config.ini 需要在运行时读取，因此随可执行文件一起安装/部署。
install(TARGETS mysql_window
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES config.ini
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_generate_deploy_app_script(
    TARGET mysql_window
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})

# ---------------------------------------------------------------------------
# 测试
#   启用方式： cmake -S . -B build -DBUILD_TESTING=ON
#   随后：     ctest --test-dir build --output-on-failure
#
#   两个测试都需要可用的 MySQL（凭据取自 config.ini）：
#     library_service_tests  业务规则与借还事务（控制台程序）
#     gui_smoke_tests        界面构造与页面导航（无需显示服务器）
# ---------------------------------------------------------------------------

option(BUILD_TESTING "构建测试目标" OFF)

if(BUILD_TESTING)
    enable_testing()

    # 业务层测试：core + data + service，不含界面。
    qt_add_executable(library_service_tests
        tests/test_library_service.cpp
        tests/TestRunner.cpp
        tests/TestRunner.h
        ${CORE_SOURCES}
        ${DATA_SOURCES}
        ${SERVICE_SOURCES}
    )
    target_include_directories(library_service_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    target_link_libraries(library_service_tests PRIVATE Qt6::Core Qt6::Sql)
    # 控制台子系统，便于直接查看输出。
    set_target_properties(library_service_tests PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)

    add_test(NAME library_service COMMAND library_service_tests)
    set_tests_properties(library_service PROPERTIES
        WORKING_DIRECTORY $<TARGET_FILE_DIR:library_service_tests>)

    # 界面冒烟测试：复用主程序的全部界面源文件，只替换 main.cpp。
    qt_add_executable(gui_smoke_tests
        tests/test_gui_smoke.cpp
        ${UI_MODEL_SOURCES}
        ${UI_WIDGET_SOURCES}
        ${UI_DIALOG_SOURCES}
        ${UI_PAGE_SOURCES}
        ${UI_SHELL_SOURCES}
    )
    target_include_directories(gui_smoke_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(gui_smoke_tests PRIVATE
        Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql)
    set_target_properties(gui_smoke_tests PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF)

    add_test(NAME gui_smoke COMMAND gui_smoke_tests)
    set_tests_properties(gui_smoke PROPERTIES
        WORKING_DIRECTORY $<TARGET_FILE_DIR:gui_smoke_tests>)
endif()
