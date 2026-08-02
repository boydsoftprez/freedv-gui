set(FDV_GUI_UTIL_SOURCES
    LabelOverrideAccessible.cpp
    NameOverrideAccessible.cpp
    TabLayoutSerializer.cpp
    wxListViewComboPopup.cpp
    WindowPositionRestore.cpp)

if(APPLE)
    list(APPEND FDV_GUI_UTIL_SOURCES MacStaticBox.mm)
endif()

add_library(fdv_gui_util STATIC ${FDV_GUI_UTIL_SOURCES})

if(APPLE)
    # Keyword form is required: upstream #1435 added a keyword-signature
    # target_link_libraries() for fdv_util below, and CMake rejects mixing the
    # plain and keyword signatures on one target. PUBLIC matches what the plain
    # signature did (populates both LINK_LIBRARIES and INTERFACE_LINK_LIBRARIES).
    target_link_libraries(fdv_gui_util PUBLIC "-framework AppKit")
endif()

target_compile_options(fdv_gui_util PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})
target_link_libraries(fdv_gui_util PUBLIC fdv_util)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_gui_util wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_gui_util PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_gui_util PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
