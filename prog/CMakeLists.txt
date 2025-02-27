cmake_minimum_required(VERSION 3.14)

set(PROJECT_NAME QtIOP-01)

set(CMAKE_AUTOUIC_SEARCH_PATHS res/)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Устанавливаем версию языка C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(${PROJECT_NAME} VERSION 1.0 DESCRIPTION "TIOP-01")

# Найти необходимые пакеты
find_package(Qt5 COMPONENTS Core Gui Widgets SerialPort REQUIRED)
if(NOT Qt5_FOUND)
    message(FATAL_ERROR "Qt5 не найден. Проверьте установку и переменную CMAKE_PREFIX_PATH.")
endif()

find_package(OpenCV REQUIRED)
if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV не найден. Проверьте установку.")
endif()

include_directories(inc)
include_directories(/usr/include/opencv4)

# Добавляем исходные файлы
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
)

# Добавляем заголовочные файлы
set(HEADERS
    inc/mainwindow.h
)

# Добавляем формы Qt Designer
set(FORMS
    res/mainwindow.ui
)

add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES} ${FORMS})
target_link_libraries(${PROJECT_NAME} Qt5::Core Qt5::Gui Qt5::Widgets Qt5::SerialPort)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
target_include_directories(${PROJECT_NAME} PRIVATE inc)
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
foreach(dir ${dirs})
  message(STATUS "dir='${dir}'")
endforeach()

install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX} CONFIGURATIONS ${CMAKE_BUILD_TYPE})


