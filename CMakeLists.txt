cmake_minimum_required(VERSION 3.10)
project(OpenCVTest)
set(CMAKE_CXX_STANDARD 17)
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)
pkg_check_modules(Tesseract REQUIRED tesseract)

find_package(OpenGL REQUIRED)

find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})


add_executable(cvnip main.cpp)
target_include_directories(cvnip PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${Tesseract_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
)

target_link_libraries(cvnip
    ${OpenCV_LIBS}
    ${Tesseract_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${X11_LIBRARIES}
    OpenGL::GL
    Xrender
    Xext
    Xft
    Xfixes
    m
    
)