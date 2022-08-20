cmake_minimum_required(VERSION 3.12)
project(cpp_app)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../exe")

include_directories("${PROJECT_SOURCE_DIR}/allegro/include")
LINK_DIRECTORIES("${PROJECT_SOURCE_DIR}/allegro/lib")

file(GLOB sourcefiles "src/*.h" "src/*.cpp")

add_executable(${PROJECT_NAME} ${sourcefiles} ${RC_FILE})


target_link_libraries(${PROJECT_NAME} 
"${PROJECT_SOURCE_DIR}/allegro/lib/liballeg.so"
"${PROJECT_SOURCE_DIR}/allegro/lib/liballeggl.so"
"${PROJECT_SOURCE_DIR}/allegro/lib/libjpgalleg.so"
"${PROJECT_SOURCE_DIR}/allegro/lib/liblogg.so"
)
