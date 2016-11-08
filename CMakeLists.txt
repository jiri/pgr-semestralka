cmake_minimum_required(VERSION 3.2)

set(CMAKE_CXX_STANDARD 14)

project(semestralka)

# Add libraries
find_package(OpenGL REQUIRED)
find_package(GLEW   REQUIRED STATIC)
find_package(glfw3  REQUIRED)
find_package(glm    REQUIRED)
find_package(Boost  REQUIRED COMPONENTS system filesystem)

# Add ImGui
add_library(
  imgui STATIC
  imgui/src/imgui.cpp
  imgui/src/imgui_draw.cpp
  imgui/src/imgui_demo.cpp
  imgui/src/imgui_impl_glfw_gl3.cpp
)

target_include_directories(
  imgui PUBLIC
  imgui/inc/
)

# Add SOIL
add_library(soil STATIC
  SOIL/src/image_helper.c
  SOIL/src/stb_image_aug.c
  SOIL/src/image_DXT.c
  SOIL/src/SOIL.c
)

if (APPLE)
  target_link_libraries(soil "-framework CoreFoundation")
endif ()

target_include_directories(soil PUBLIC SOIL/src/)

# Add targets
add_executable(
  ${PROJECT_NAME}
  src/main.cpp
)

target_include_directories(
  ${PROJECT_NAME} PUBLIC
  ${OPENGL_INCLUDE_DIRS}
  ${GLEW_INCLUDE_DIRS}
  ${GLFW_INCLUDE_DIRS}
  ${GLM_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
)

target_link_libraries(
  ${PROJECT_NAME} PUBLIC
  ${OPENGL_LIBRARIES}
  ${GLEW_LIBRARIES}
  glfw
  ${GLM_LIBRARIES}
  ${Boost_LIBRARIES}
  imgui
  soil
)

# Copy resources
add_custom_target(
  copy_shaders
  ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shd
    ${CMAKE_CURRENT_BINARY_DIR}/shd
  COMMENT "Copying shaders" VERBATIM
)

add_dependencies(${PROJECT_NAME} copy_shaders)

add_custom_target(
  copy_resources
  ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/res
    ${CMAKE_CURRENT_BINARY_DIR}/res
  COMMENT "Copying resources" VERBATIM
)

add_dependencies(${PROJECT_NAME} copy_resources)
