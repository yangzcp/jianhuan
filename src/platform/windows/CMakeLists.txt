add_library(jianhuan_tsf SHARED
  guids.cpp
  dllmain.cpp
  class_factory.cpp
  text_service.cpp
  candidate_window.cpp
  edit_session.cpp
  registration.cpp
  langbar.cpp
  jianhuan_tsf.def
)
target_link_libraries(jianhuan_tsf PRIVATE jianhuan_core ole32 oleaut32 user32 gdi32 uuid advapi32 gdiplus)
target_include_directories(jianhuan_tsf PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(jianhuan_tsf PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(jianhuan_tsf PRIVATE /utf-8)
endif()
set_target_properties(jianhuan_tsf PROPERTIES PREFIX "")

add_executable(jianhuan_register register.cpp registration.cpp guids.cpp)
target_link_libraries(jianhuan_register PRIVATE ole32 oleaut32 uuid advapi32)
target_include_directories(jianhuan_register PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(jianhuan_register PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(jianhuan_register PRIVATE /utf-8)
endif()

add_executable(jianhuan_settings settings_dialog.cpp jianhuan.rc "${CMAKE_CURRENT_SOURCE_DIR}/../../core/settings.cpp")
target_link_libraries(jianhuan_settings PRIVATE user32 gdi32 ole32)
target_include_directories(jianhuan_settings PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include")
target_compile_definitions(jianhuan_settings PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(jianhuan_settings PRIVATE /utf-8)
endif()
set_target_properties(jianhuan_settings PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_options(jianhuan_settings PRIVATE /ENTRY:wmainCRTStartup)

add_executable(jianhuan_tray tray.cpp jianhuan.rc "${CMAKE_CURRENT_SOURCE_DIR}/../../core/settings.cpp")
target_link_libraries(jianhuan_tray PRIVATE user32 gdi32 ole32 shell32)
target_include_directories(jianhuan_tray PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../../core/include")
target_compile_definitions(jianhuan_tray PRIVATE UNICODE _UNICODE)
if(MSVC)
  target_compile_options(jianhuan_tray PRIVATE /utf-8)
endif()
set_target_properties(jianhuan_tray PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_options(jianhuan_tray PRIVATE /ENTRY:wmainCRTStartup)