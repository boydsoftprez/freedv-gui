add_library(fdv_rig_control STATIC
    HamlibRigController.cpp
    SerialPortRigController.cpp
    SerialPortInRigController.cpp
    SerialPortOutRigController.cpp
    TciProtocol.cpp
    TciWebSocketClient.cpp
    TciRigController.cpp
)

target_include_directories(fdv_rig_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)

if(HAMLIB_ADD_DEPENDENCY)
add_dependencies(fdv_rig_control hamlib)
endif(HAMLIB_ADD_DEPENDENCY)

if(WIN32)
# Add OmniRig support on Windows
add_subdirectory(omnirig)
target_link_libraries(fdv_rig_control PUBLIC fdv_rig_control_omnirig)
endif(WIN32)

target_compile_options(fdv_rig_control PRIVATE ${WARNINGS_AS_ERRORS_FLAGS})

if(UNITTEST)
    macro(DefineRigControlUnitTest utName)
        add_executable(${utName} test/${utName}.cpp)
        target_link_libraries(${utName} PRIVATE fdv_rig_control fdv_util)
        target_include_directories(${utName} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/..
            ${CMAKE_CURRENT_BINARY_DIR}/..)
        add_test(NAME rig_control_${utName} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${utName})
    endmacro()

    # Individual DefineRigControlUnitTest(...) calls live alongside their
    # test source files (added by Tasks 5, 7, 10 of the TCI port plan).
    DefineRigControlUnitTest(TciProtocolTest)

    # TCI WS roundtrip smoke test (multi-source: test driver + mock server
    # fixture).  Uses an explicit add_executable because DefineRigControlUnitTest
    # only accepts a single source file.
    add_executable(TciWebSocketClientTest
        test/TciWebSocketClientTest.cpp
        test/mock_tci_server.cpp)
    target_link_libraries(TciWebSocketClientTest PRIVATE fdv_rig_control fdv_util)
    target_include_directories(TciWebSocketClientTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_BINARY_DIR}/..)
    add_test(NAME rig_control_TciWebSocketClientTest
             COMMAND ${CMAKE_CURRENT_BINARY_DIR}/TciWebSocketClientTest)
endif(UNITTEST)
