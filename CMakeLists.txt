cmake_minimum_required(VERSION 3.19)
project(RogerPDF LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

qt_add_executable(RogerPDF
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

target_include_directories(RogerPDF PRIVATE
    third_party/mupdf/include
)

target_link_directories(RogerPDF PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/mupdf/build/release
)

target_link_libraries(RogerPDF
    PRIVATE
        Qt::Core
        Qt::Widgets

        mupdf
        mupdf-third
        mupdf-pkcs7
        mupdf-threads
)

install(TARGETS RogerPDF
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET RogerPDF
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
