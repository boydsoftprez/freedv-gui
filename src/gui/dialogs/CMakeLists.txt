add_library(fdv_gui_dialogs STATIC
    begin_recording.cpp
    dlg_audiooptions.cpp
    dlg_easy_setup.cpp
    dlg_filter.cpp
    dlg_options.cpp
    dlg_ptt.cpp
    freedv_reporter.cpp
    log_entry.cpp
    monitor_volume_adj.cpp
    tot_warning.cpp)

target_include_directories(fdv_gui_dialogs PRIVATE ${CODEC2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../.. ${CMAKE_CURRENT_BINARY_DIR}/../..)

# fdv_audio is required for the TCI device-name helpers: dlg_audiooptions.cpp
# calls tci::makeTciDeviceName()/tci::isTciDeviceName(), which are defined in
# audio/TciDeviceNaming.cpp. Declaring the dependency here rather than relying
# on the link order in src/CMakeLists.txt -- these are static libraries, so the
# linker resolves left to right, and fdv_audio is listed before fdv_gui_dialogs
# there and so cannot satisfy references made by it.
target_link_libraries(fdv_gui_dialogs PUBLIC samplerate fdv_gui_controls fdv_audio_pipeline_extra fdv_audio)

target_compile_options(fdv_gui_dialogs PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})

if(HAMLIB_ADD_DEPENDENCY)
add_dependencies(fdv_gui_dialogs hamlib)
endif(HAMLIB_ADD_DEPENDENCY)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_gui_dialogs wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_gui_dialogs PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_gui_dialogs PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
