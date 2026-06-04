if(NATIVE_AUDIO_AVAILABLE)
    if(APPLE)
        set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
            MacAudioEngine.cpp
            MacAudioDevice.cpp
        )
    elseif(WIN32)
        set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
            WASAPIAudioEngine.cpp
            WASAPIAudioDevice.cpp
        )
    elseif(LINUX)
        if (DBUS_FOUND)
            set(RTKIT_FILES rtkit.c)
        endif (DBUS_FOUND)

        set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
            PulseAudioDevice.cpp
            PulseAudioEngine.cpp
	    ${RTKIT_FILES}
        )
    endif()
else()
    set(AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES
        PortAudioDevice.cpp
        PortAudioEngine.cpp
        PortAudioInterface.cpp
    )
endif(NATIVE_AUDIO_AVAILABLE)

add_library(fdv_audio STATIC
    AudioDeviceSpecification.cpp
    AudioEngineFactory.cpp
    IAudioDevice.cpp
    IAudioEngine.cpp
    TciAudioDevice.cpp
    TciSampleConvert.cpp
    TciDeviceNaming.cpp
    ${AUDIO_ENGINE_LIBRARY_SPECIFIC_FILES}
)

target_link_libraries(fdv_audio PUBLIC fdv_util)

if(APPLE AND NATIVE_AUDIO_AVAILABLE)
    target_link_libraries(fdv_audio PUBLIC
        "-framework AVFoundation"
        "-framework CoreAudio")
elseif(WIN32 AND NATIVE_AUDIO_AVAILABLE)
    target_link_libraries(fdv_audio PRIVATE uuid avrt winmm ksuser)
elseif(LINUX AND NATIVE_AUDIO_AVAILABLE AND DBUS_FOUND)
    target_include_directories(fdv_audio PRIVATE ${DBUS_INCLUDE_DIRS})
    target_compile_definitions(fdv_audio PRIVATE -DUSE_RTKIT)
    target_link_libraries(fdv_audio PRIVATE ${DBUS_LIBRARIES})
endif()

target_compile_options(fdv_audio PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})

if(UNITTEST)
    macro(DefineAudioUnitTest utName)
        add_executable(${utName} test/${utName}.cpp)
        target_link_libraries(${utName} PRIVATE fdv_audio fdv_rig_control fdv_util)
        target_include_directories(${utName} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_BINARY_DIR}/..)
        add_test(NAME audio_${utName} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${utName})
    endmacro()

    # Individual DefineAudioUnitTest(...) calls live alongside their
    # test source files (added by Tasks 6, 15 of the TCI port plan).
    DefineAudioUnitTest(TciAudioFrameTest)
    DefineAudioUnitTest(TciDeviceNamingTest)
endif(UNITTEST)

if(BOOTSTRAP_WXWIDGETS)
    add_dependencies(fdv_audio wx::core wx::base wx::aui wx::html wx::net wx::adv wx::propgrid wx::xrc)
    target_compile_definitions(fdv_audio PRIVATE ${WXBUILD_BUILD_DEFS})
    target_include_directories(fdv_audio PRIVATE ${WXBUILD_INCLUDES})
endif(BOOTSTRAP_WXWIDGETS)
