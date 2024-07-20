add_executable(resedit resedit.cpp)

set(IMGUI_LIBS imgui imgui-sdl2 imgui-opengl3)
target_link_libraries(resedit PUBLIC common miniposix ${IMGUI_LIBS} ${VANILLA_LIBS} ${STATIC_LIBS})