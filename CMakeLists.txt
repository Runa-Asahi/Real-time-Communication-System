cmake_minimum_required(VERSION 3.20)
project(Ciallo VERSION 1.0.1 DESCRIPTION "Ciallo 即时通讯（课程设计）" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# 默认情况下 CMake 把 -I/-isystem 收进 @includes_CXX.rsp 响应文件，而 VS Code 的
# C/C++ 扩展（cpptools）与部分 clangd 版本不会展开响应文件，结果是整份工程（尤其 Qt
# 头文件）在编辑器里全报"无法打开源文件"。这里强制把包含路径写进命令行本身。
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES OFF)
# 扩展开关：GUI 目标默认关闭，基础版（数字菜单课设交付）不需要 Qt。
option(FANLX_BUILD_GUI "构建 Qt 图形客户端 fanlx_gui" OFF)
# 控制台目标开关。关掉后只跳过「控制台菜单客户端」与它的入口 fanlx_app，
# 网络/持久化/聊天等库与 GUI 完全不受影响（GUI 不经过 fanlx_console，见下方依赖图）。
# 注意 fanlx_app 同时是**服务端的唯一实现**（--im-server）与 --init-demo 的提供者，
# 关掉后需临时用 -DFANLX_BUILD_CLI=ON 重新配置才能起服务端或重建演示数据。
option(FANLX_BUILD_CLI "构建控制台客户端 fanlx_console 与入口 fanlx_app" ON)
# SQLite 以系统 winsqlite3.dll 的 C API 接入；仓库内只保存官方 sqlite3.h（公共领域）。
# 不把本机盘符写死：DLL 位置可通过 -DFANLX_SQLITE_DLL=... 覆盖。
set(FANLX_SQLITE_DLL "C:/Windows/System32/winsqlite3.dll" CACHE FILEPATH "提供 SQLite C API 的系统 DLL")
if(MSVC)
  add_compile_options(/W4 /utf-8)
endif()
if(MINGW)
  # 避免 PATH 中 Anaconda/Dev-C++ 的旧 DLL 抢先加载 GCC 15 所需符号。
  # 静态链接 MinGW 运行库；Windows 系统 DLL 仍由系统提供。
  add_link_options(-static)
endif()
add_library(nlohmann_json INTERFACE)
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
target_include_directories(nlohmann_json INTERFACE third_party)

# --- SQLite 依赖：由系统 DLL 现场生成导入库，仓库不保存任何第三方二进制 ---------
if(NOT EXISTS "${FANLX_SQLITE_DLL}")
  message(FATAL_ERROR "找不到 SQLite DLL：${FANLX_SQLITE_DLL}（用 -DFANLX_SQLITE_DLL=路径 指定）")
endif()
find_program(FANLX_GENDEF_EXE NAMES gendef REQUIRED)
find_program(FANLX_DLLTOOL_EXE NAMES dlltool REQUIRED)
set(FANLX_SQLITE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/sqlite")
set(FANLX_SQLITE_DEF "${FANLX_SQLITE_GENERATED_DIR}/winsqlite3.def")
set(FANLX_SQLITE_IMPLIB "${FANLX_SQLITE_GENERATED_DIR}/libwinsqlite3.a")
file(MAKE_DIRECTORY "${FANLX_SQLITE_GENERATED_DIR}")
# 必须先进入输出目录：gendef 只按相对路径写文件，不支持绝对输出路径。
add_custom_command(OUTPUT "${FANLX_SQLITE_DEF}"
  COMMAND "${FANLX_GENDEF_EXE}" -o winsqlite3.def "${FANLX_SQLITE_DLL}"
  WORKING_DIRECTORY "${FANLX_SQLITE_GENERATED_DIR}"
  DEPENDS "${FANLX_SQLITE_DLL}"
  COMMENT "导出 ${FANLX_SQLITE_DLL} 的 C API 符号表")
# -D 决定导入库记录的目标 DLL 名（运行时由系统加载），导入库本身入库为构建产物。
add_custom_command(OUTPUT "${FANLX_SQLITE_IMPLIB}"
  COMMAND "${FANLX_DLLTOOL_EXE}" -d "${FANLX_SQLITE_DEF}" -l "${FANLX_SQLITE_IMPLIB}"
          -D "winsqlite3.dll"
  DEPENDS "${FANLX_SQLITE_DEF}"
  COMMENT "由符号表生成 SQLite 导入库")
add_custom_target(fanlx_sqlite_importlib DEPENDS "${FANLX_SQLITE_IMPLIB}")
# 把导入库作为一个 IMPORTED 目标链接：路径与依赖都由生成规则保证，不使用 -l 搜索。
add_library(fanlx_sqlite INTERFACE)
target_include_directories(fanlx_sqlite INTERFACE third_party/sqlite)
target_link_libraries(fanlx_sqlite INTERFACE "${FANLX_SQLITE_IMPLIB}")
add_dependencies(fanlx_sqlite fanlx_sqlite_importlib)

# --- 持久化层实现（源文件按层归入 src/persistence，头文件在 include/persistence） ----
add_library(fanlx_persistence
  src/persistence/DemoSeedFanLX.cpp
  src/persistence/PasswordHashFanLX.cpp
  src/persistence/SnapshotCodecFanLX.cpp
  src/persistence/SchemaValidatorFanLX.cpp
  src/persistence/PreparedCommitFanLX.cpp
  src/persistence/DataDirectoryLockFanLX.cpp
  src/persistence/AtomicFileWriterFanLX.cpp
  src/persistence/NamedFilePublisherFanLX.cpp
  src/persistence/JsonFileRepositoryFanLX.cpp
  src/persistence/TransactionCoordinatorFanLX.cpp
  src/persistence/PersistenceApplicationFanLX.cpp)
target_include_directories(fanlx_persistence PUBLIC include)
# bcrypt：Windows 自带 CNG（PBKDF2-HMAC-SHA256 口令哈希与随机盐），不是第三方库。
target_link_libraries(fanlx_persistence PUBLIC nlohmann_json::nlohmann_json bcrypt)
if(MINGW)
  target_compile_options(fanlx_persistence PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# --- 聊天记录仓储（SQLite）：客户端与服务端消息库、迁移与数据库工作线程 -------------
add_library(fanlx_chat
  src/db/SqliteHandleFanLX.cpp
  src/db/ChatSchemaFanLX.cpp
  src/db/ChatSchemaStoreFanLX.cpp
  src/db/SqliteChatRepositoryFanLX.cpp
  src/db/SqliteMessageStoreFanLX.cpp
  src/db/ChatMigrationFanLX.cpp
  src/db/DatabaseWorkerFanLX.cpp)
target_include_directories(fanlx_chat PUBLIC include)
target_link_libraries(fanlx_chat PUBLIC fanlx_persistence fanlx_sqlite Threads::Threads)
if(MINGW)
  target_compile_options(fanlx_chat PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
find_package(Threads REQUIRED)
# 网络层：帧编解码、TcpPeer（含每连接待发送缓冲）、监听、IM 客户端与服务端各自独立文件，
# 以及服务端的账号登记/读穿服务（ServerAccountRegistrarFanLX，对应"注册后无需重启服务端"）。
add_library(fanlx_network
  src/network/MessageCodecFanLX.cpp
  src/network/MessageMailboxFanLX.cpp
  src/network/TcpPeerFanLX.cpp
  src/network/TcpListenerFanLX.cpp
  src/network/ImClientFanLX.cpp
  src/network/ImServerFanLX.cpp
  src/network/DirectLinkFanLX.cpp
  src/network/ServerAccountRegistrarFanLX.cpp
  src/network/PeerAccountResolutionFanLX.cpp
  src/network/JsonStoreFanLX.cpp
  src/network/FriendRequestBookFanLX.cpp
  src/network/TemporaryChatQuotaFanLX.cpp)
target_include_directories(fanlx_network PUBLIC include)
target_link_libraries(fanlx_network PUBLIC ws2_32 Threads::Threads fanlx_persistence fanlx_chat
  PRIVATE nlohmann_json::nlohmann_json)
if(MINGW)
  target_compile_options(fanlx_network PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
# --- 控制台客户端（可选）：FANLX_BUILD_CLI=ON 时构建 ------------------------------
# fanlx_console 只被 fanlx_app（控制台入口）与 network_tests / console_tests 使用；
# fanlx_gui_core 链的是 fanlx_network，**不经过**本目标，因此下方 GUI 段不受影响。
if(FANLX_BUILD_CLI)
add_library(fanlx_console src/ui/ConsoleAppFanLX.cpp src/ui/QQChatSessionFanLX.cpp)
target_link_libraries(fanlx_console PUBLIC fanlx_persistence fanlx_network)
if(MINGW)
  target_compile_options(fanlx_console PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(network_tests tests/network_tests.cpp)
target_link_libraries(network_tests PRIVATE fanlx_console)
if(MINGW)
  target_compile_options(network_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_options(network_tests PRIVATE -municode)
endif()
add_test(NAME network_behavior COMMAND network_tests)
set_tests_properties(network_behavior PROPERTIES TIMEOUT 40)
add_test(NAME im_behavior COMMAND network_tests --im)
set_tests_properties(im_behavior PROPERTIES TIMEOUT 90)
add_executable(p2p_tests tests/p2p_tests.cpp)
target_link_libraries(p2p_tests PRIVATE fanlx_network iphlpapi)
if(MINGW)
  target_compile_options(p2p_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME p2p_behavior COMMAND p2p_tests)
set_tests_properties(p2p_behavior PROPERTIES TIMEOUT 120)
add_executable(fanlx_app src/main.cpp)
target_include_directories(fanlx_app PRIVATE include)
target_link_libraries(fanlx_app PRIVATE fanlx_console)
if(MINGW)
  target_link_options(fanlx_app PRIVATE -municode)
  target_compile_options(fanlx_app PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
endif() # FANLX_BUILD_CLI
enable_testing()
add_executable(integration_tests tests/integration_tests.cpp)
target_link_libraries(integration_tests PRIVATE fanlx_persistence)
if(MINGW)
  target_compile_options(integration_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME integration_behavior COMMAND integration_tests)
set_tests_properties(integration_behavior PROPERTIES TIMEOUT 60)
if(FANLX_BUILD_CLI)
  add_test(NAME smoke COMMAND fanlx_app --demo)
endif()
add_executable(cache_tests tests/cache_tests.cpp)
target_include_directories(cache_tests PRIVATE include)
# AccountManagerFanLX 的口令校验实现在 fanlx_persistence（PasswordHashFanLX.cpp）。
target_link_libraries(cache_tests PRIVATE fanlx_persistence)
if(MINGW)
  target_compile_options(cache_tests PRIVATE -UNDEBUG)
endif()
add_test(NAME cache_behavior COMMAND cache_tests)
add_executable(group_tests tests/group_tests.cpp)
target_include_directories(group_tests PRIVATE include)
add_test(NAME group_behavior COMMAND group_tests)
add_executable(persistence_tests tests/persistence_tests.cpp)
target_link_libraries(persistence_tests PRIVATE fanlx_persistence)
if(MINGW)
  target_compile_options(persistence_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  target_link_options(persistence_tests PRIVATE -municode)
endif()
add_test(NAME persistence_behavior COMMAND persistence_tests)
add_executable(password_hash_tests tests/password_hash_tests.cpp)
target_link_libraries(password_hash_tests PRIVATE fanlx_persistence)
if(MINGW)
  target_compile_options(password_hash_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME password_hash_behavior COMMAND password_hash_tests)
add_executable(query_cache_tests tests/query_cache_tests.cpp)
target_link_libraries(query_cache_tests PRIVATE fanlx_persistence)
if(MINGW)
  target_compile_options(query_cache_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME query_cache_behavior COMMAND query_cache_tests)
if(FANLX_BUILD_CLI)
add_executable(console_tests tests/console_tests.cpp)
target_link_libraries(console_tests PRIVATE fanlx_console)
if(MINGW)
  target_compile_options(console_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME console_behavior COMMAND console_tests)
endif() # FANLX_BUILD_CLI
add_executable(chat_history_tests tests/chat_history_tests.cpp)
target_link_libraries(chat_history_tests PRIVATE fanlx_chat)
if(MINGW)
  target_compile_options(chat_history_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME chat_history_behavior COMMAND chat_history_tests)
set_tests_properties(chat_history_behavior PROPERTIES TIMEOUT 180)
# 十万条规模测量单独运行：ctest -L perf（默认不跑，避免拖慢日常回归）。
add_test(NAME chat_history_performance COMMAND chat_history_tests --perf)
set_tests_properties(chat_history_performance PROPERTIES TIMEOUT 900 LABELS perf)
add_executable(cache_benchmark benchmarks/cache_benchmark.cpp)
target_link_libraries(cache_benchmark PRIVATE fanlx_persistence)
if(MINGW)
  target_compile_options(cache_benchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
add_executable(pool_benchmark benchmarks/pool_benchmark.cpp)
target_include_directories(pool_benchmark PRIVATE include)
if(MINGW)
  target_compile_options(pool_benchmark PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# --- 可视化客户端（可选）：FANLX_BUILD_GUI=ON 时才需要 Qt -------------------------
if(FANLX_BUILD_GUI)
  # Qt 路径通过 CMAKE_PREFIX_PATH 注入，不在仓库里写死盘符；
  # 默认回退到常见安装位置只是为了本机方便，找不到就明确报错。
  if(NOT DEFINED CMAKE_PREFIX_PATH OR CMAKE_PREFIX_PATH STREQUAL "")
    foreach(_candidate "E:/Qt/6.11.1/mingw_64" "C:/Qt/6.11.1/mingw_64" "D:/Qt/6.11.1/mingw_64")
      if(EXISTS "${_candidate}/lib/cmake/Qt6/Qt6Config.cmake")
        list(APPEND CMAKE_PREFIX_PATH "${_candidate}")
      endif()
    endforeach()
  endif()
  find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets)
  # 不使用 CMAKE_AUTOMOC：Qt 的 AutoMoc 通过带管道捕获的子进程调用 moc，
  # 在受限环境（无法创建命名管道）下会报 "libuv process spawn failed"。
  # 改用 qt6_wrap_cpp：它直接调用 moc 并把生成的 moc_*.cpp 加入源文件，行为等价且可审查。
  set(FANLX_GUI_SOURCES
    src/gui/ChatControllerFanLX.cpp
    src/gui/ConversationModelFanLX.cpp
    src/gui/LoginDialogFanLX.cpp
    src/gui/HistoryDialogFanLX.cpp
    src/gui/DomainControllerFanLX.cpp
    src/gui/AccountPageFanLX.cpp
    src/gui/FriendPageFanLX.cpp
    src/gui/GroupPageFanLX.cpp
    src/gui/GroupListDelegateFanLX.cpp
    src/gui/RegisterDialogFanLX.cpp
    src/gui/MainWindowFanLX.cpp
    src/gui/AppThemeFanLX.cpp)
  set(FANLX_GUI_HEADERS
    include/gui/ChatControllerFanLX.h
    include/gui/ConversationModelFanLX.h
    include/gui/LoginDialogFanLX.h
    include/gui/HistoryDialogFanLX.h
    include/gui/DomainControllerFanLX.h
    include/gui/AccountPageFanLX.h
    include/gui/FriendPageFanLX.h
    include/gui/GroupPageFanLX.h
    include/gui/RegisterDialogFanLX.h
    include/gui/MainWindowFanLX.h)
  qt6_wrap_cpp(FANLX_GUI_MOC ${FANLX_GUI_HEADERS})
  # 群页外观表，以及会话列表的好友/群图标（resources/icons/111.png、222.png）。
  # 静态库里的 qrc 会被链接器丢掉，除非 MainWindow 里调用 Q_INIT_RESOURCE(app)。
  qt6_add_resources(FANLX_APP_QSS ${CMAKE_CURRENT_SOURCE_DIR}/resources/app.qrc)
  add_library(fanlx_gui_core ${FANLX_GUI_SOURCES} ${FANLX_GUI_MOC} ${FANLX_APP_QSS})
  target_include_directories(fanlx_gui_core PUBLIC include)
  target_link_libraries(fanlx_gui_core PUBLIC fanlx_network Qt6::Core Qt6::Gui Qt6::Widgets)
  if(MINGW)
    target_compile_options(fanlx_gui_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_executable(fanlx_gui src/gui/main_gui.cpp)
  target_link_libraries(fanlx_gui PRIVATE fanlx_gui_core)
  # Ciallo 图标与版本信息（MinGW 用 windres 编译 .rc）。目标名保持 fanlx_gui，脚本与文档不受影响；
  # 发布包里另行复制为 Ciallo.exe。
  if(WIN32)
    enable_language(RC)
    target_sources(fanlx_gui PRIVATE resources/ciallo.rc)
  endif()
  if(MINGW)
    target_compile_options(fanlx_gui PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  # 刻意**不**设 WIN32_EXECUTABLE（Windows 子系统）。
  # 那会把入口点换成 WinMain，需要 Qt 的 libQt6EntryPoint.a 提供转发；而该库引用
  # msvcrt 导出的 __argc/__argv，在当前的 GCC 15.1 / MinGW-w64 工具链下链接失败：
  #   undefined reference to `__imp___argc'
  # 所以仍按控制台子系统链接（入口 mainCRTStartup -> main，不依赖任何入口库），
  # 由 main_gui.cpp 在启动最早处隐藏自己独占的控制台窗口。
  # 控制器/模型层不需要窗口系统即可测试（离屏平台），便于回归。
  add_executable(gui_tests tests/gui_tests.cpp)
  target_link_libraries(gui_tests PRIVATE fanlx_gui_core)
  if(MINGW)
    target_compile_options(gui_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME gui_behavior COMMAND gui_tests)
  set_tests_properties(gui_behavior PROPERTIES TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # GUI 领域层回归：账号与服务 / 好友 / 群 三个页签背后的门面与页面装配（课设要求 1~6 条款覆盖）。
  add_executable(gui_domain_tests tests/gui_domain_tests.cpp)
  target_link_libraries(gui_domain_tests PRIVATE fanlx_gui_core)
  if(MINGW)
    target_compile_options(gui_domain_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  add_test(NAME gui_domain_behavior COMMAND gui_domain_tests)
  set_tests_properties(gui_domain_behavior PROPERTIES TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

  # 三客户端界面截图工具：在隔离目录内自建数据与服务端，离屏渲染窗口为 PNG（交付证据）。
  add_executable(gui_screenshot tools/gui_screenshot.cpp)
  target_link_libraries(gui_screenshot PRIVATE fanlx_gui_core)
  add_test(NAME gui_three_client_behavior COMMAND gui_screenshot --out ${CMAKE_BINARY_DIR}/gui-screenshots)
  set_tests_properties(gui_three_client_behavior PROPERTIES TIMEOUT 180 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
  if(MINGW)
    target_compile_options(gui_screenshot PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endif()
