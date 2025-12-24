cmake_minimum_required(VERSION 3.16)

project(Crawler VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql Network)

# ==================== 第三方库配置 ====================
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(CURL_DIR "${THIRD_PARTY_DIR}/curl-8.17.0_5-win64-mingw")
set(NLOHMANN_JSON_DIR "${THIRD_PARTY_DIR}/nlohmann-json-develop")
# WebView2 SDK（可选，Windows专用，MinGW不链接静态库，仅用于头文件）
set(WEBVIEW2_SDK_DIR "${THIRD_PARTY_DIR}/WebView2SDK")
set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_SDK_DIR}/build/native/include")
set(WEBVIEW2_RUNTIME_DLL "${WEBVIEW2_SDK_DIR}/runtimes/win-x64/native/WebView2Loader.dll")

# Include db and tasks directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
if (EXISTS "${WEBVIEW2_INCLUDE_DIR}")
    message(STATUS "WebView2 headers found: ${WEBVIEW2_INCLUDE_DIR}")
    include_directories(${WEBVIEW2_INCLUDE_DIR})
endif()

# 常量定义头文件
set(CONSTANTS_HEADERS
        constants/network_types.h
        constants/db_types.h
)

# 配置管理模块
set(CONFIG_SOURCES
        config/config_manager.h
        config/config_manager.cpp
)

# 主应用源文件
set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        db/sqlinterface.h 
        db/sqlinterface.cpp
        tasks/sql_task.h
        tasks/sql_task.cpp
        tasks/internet_task.h
        tasks/internet_task.cpp
        tasks/crawler_task.h
        tasks/crawler_task.cpp
)

# 网络爬虫源文件
set(NETWORK_SOURCES
        network/job_crawler.h
        network/job_crawler_parser.cpp
        network/job_crawler_network.cpp
        network/job_crawler_utils.cpp
        network/job_crawler_printer.cpp
        network/crawl_nowcode.h
        network/crawl_nowcode.cpp
        network/crawl_zhipin.h
        network/crawl_zhipin.cpp
        # network/webview2_browser.h (已废弃)
        # network/webview2_browser.cpp (已废弃)
        network/webview2_browser_wrl.h
        network/webview2_browser_wrl.cpp
)

# 测试源文件
set(TEST_SOURCES
        test/test.h
        # 单元测试
        test/test_internet_task.cpp
        test/test_sql_task.cpp
        # 集成测试
        test/test_crawler_task.cpp
        # Cookie获取测试
        # test/test_webview2_cookie.cpp (已废弃)
        test/test_webview2_cookie_wrl.cpp
        # 批量爬取集成测试
        test/test_batch_crawl.cpp
        # [Deprecated] 以下测试已被新架构替代，可删除
        # test/test_sql.cpp
        # test/test_job_crawler.cpp
        # test/test_integration.cpp
)

# （注意：复制WebView2运行时DLL的命令需要在目标创建之后再执行）

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Crawler
        MANUAL_FINALIZATION
        ${CONSTANTS_HEADERS}
        ${CONFIG_SOURCES}
        ${PROJECT_SOURCES}
        ${NETWORK_SOURCES}
        ${TEST_SOURCES}
    )

    # WebView2 静态库目录和链接（仅Qt6下）
    target_link_directories(Crawler PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include/WebView2SDK/build/native/x64
    )
    target_link_libraries(Crawler PRIVATE WebView2LoaderStatic)
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Crawler APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(Crawler SHARED
            ${CONSTANTS_HEADERS}
            ${PROJECT_SOURCES}
            ${NETWORK_SOURCES}
            ${TEST_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Crawler
            ${CONSTANTS_HEADERS}
            ${PROJECT_SOURCES}
            ${NETWORK_SOURCES}
            ${TEST_SOURCES}
        )
    endif()
endif()

# ==================== 包含目录 ====================
target_include_directories(Crawler PRIVATE
    ${NLOHMANN_JSON_DIR}/include
    ${CURL_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/wil/include
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/winrt"
)

# ==================== 链接库 ====================
target_link_libraries(Crawler PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets 
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

# Windows特定配置
if(WIN32)
    # 添加编译定义
    target_compile_definitions(Crawler PRIVATE
        CURL_STATICLIB
        _WIN32_WINNT=0x0600
    )

    # 链接系统库
    target_link_libraries(Crawler PRIVATE
        ws2_32
        wldap32
        crypt32
        normaliz
        advapi32
        ole32
        oleaut32
        uuid
    )

    # 复制WebView2运行时DLL到可执行目录（如果存在）。
    # 注意：仅复制DLL，MinGW不直接链接MSVC格式的 .lib 文件。
    if (EXISTS "${WEBVIEW2_RUNTIME_DLL}")
        add_custom_command(TARGET Crawler POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E echo "Post-build: copying WebView2Loader.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WEBVIEW2_RUNTIME_DLL}" "$<TARGET_FILE_DIR:Crawler>"
            VERBATIM
        )
    else()
        message(STATUS "WebView2 runtime DLL not found at: ${WEBVIEW2_RUNTIME_DLL}")
    endif()

    # 检查CURL库文件是否存在
    if(EXISTS "${CURL_DIR}/lib/libcurl.dll.a")
        # 链接CURL库
        target_link_libraries(Crawler PRIVATE
            "${CURL_DIR}/lib/libcurl.dll.a"
        )

        # 如果存在DLL，复制到输出目录
        if(EXISTS "${CURL_DIR}/bin/libcurl-x64.dll")
            add_custom_command(TARGET Crawler POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy
                "${CURL_DIR}/bin/libcurl-x64.dll"
                "$<TARGET_FILE_DIR:Crawler>/libcurl-x64.dll"
                COMMENT "Copying libcurl-x64.dll"
            )
        endif()
    endif()
    
    # 复制配置文件到输出目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.json")
        add_custom_command(TARGET Crawler POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/config.json"
            "$<TARGET_FILE_DIR:Crawler>/config.json"
            COMMENT "Copying config.json"
        )
    endif()
endif()


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Crawler)
endif()
set_target_properties(Crawler PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Crawler
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Crawler)
endif()
