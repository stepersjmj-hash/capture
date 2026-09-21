cmake_minimum_required(VERSION 3.16)
project(Mcapture VERSION 1.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg Network)

add_executable(mcapture
    src/main.cpp
    src/App.cpp
    src/App.h
    src/Annotation.h
    src/Canvas.cpp
    src/Canvas.h
    src/Defaults.h
    src/CaptureOverlay.cpp
    src/CaptureOverlay.h
    src/ClipboardWatch.cpp
    src/ClipboardWatch.h
    src/EditorWindow.cpp
    src/EditorWindow.h
    src/HelpDialog.cpp
    src/HelpDialog.h
    src/HotKey.cpp
    src/HotKey.h
    src/Icons.h
    src/Keys.h
    src/Log.h
    src/Platform.h
    src/SettingsDialog.cpp
    src/SettingsDialog.h
    src/Theme.h
    src/Updater.cpp
    src/Updater.h
    assets.qrc
)

target_compile_definitions(mcapture PRIVATE APP_VERSION="${PROJECT_VERSION}")
target_link_libraries(mcapture PRIVATE Qt6::Widgets Qt6::Svg Qt6::Network)

if(WIN32)
    target_compile_definitions(mcapture PRIVATE NOMINMAX)
    target_link_libraries(mcapture PRIVATE user32 shell32)
    # 릴리즈 실행 파일 심볼 제거 (Apple ld는 -s를 지원하지 않음 — 맥은 dist 스크립트에서 strip)
    target_link_options(mcapture PRIVATE $<$<CONFIG:Release>:-s>)
    set_target_properties(mcapture PROPERTIES WIN32_EXECUTABLE ON OUTPUT_NAME Mcapture)
    # 실행 파일 아이콘: assets/app.ico 는 `Mcapture --export-ico assets/app.svg assets/app.ico` 로 생성 (README 참고)
    if(EXISTS ${CMAKE_SOURCE_DIR}/assets/app.ico)
        enable_language(RC)
        target_sources(mcapture PRIVATE app.rc)
    endif()
endif()

if(APPLE)
    enable_language(OBJCXX)
    target_sources(mcapture PRIVATE src/Platform.mm)
    target_link_libraries(mcapture PRIVATE "-framework AppKit" "-framework Carbon")
    set(MACOSX_BUNDLE_BUNDLE_NAME "Mcapture")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.stepersjmj.mcapture")
    set(MACOSX_BUNDLE_ICON_FILE "app.icns")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set_target_properties(mcapture PROPERTIES
        OUTPUT_NAME Mcapture
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/macos/Info.plist.in)
    # 앱 아이콘: assets/app.icns 는 `Mcapture --export-icns assets/app.svg assets/app.icns` 로 생성
    if(EXISTS ${CMAKE_SOURCE_DIR}/assets/app.icns)
        target_sources(mcapture PRIVATE assets/app.icns)
        set_source_files_properties(${CMAKE_SOURCE_DIR}/assets/app.icns
                                    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    endif()
endif()
