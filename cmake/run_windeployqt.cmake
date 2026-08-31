if(NOT EXISTS "${WINDEPLOYQT_EXECUTABLE}")
    message(FATAL_ERROR "windeployqt was not found at ${WINDEPLOYQT_EXECUTABLE}")
endif()

if(NOT EXISTS "${APPLICATION_FILE}")
    message(FATAL_ERROR "Application was not found at ${APPLICATION_FILE}")
endif()

set(_configuration_argument --release)
if(BUILD_CONFIGURATION STREQUAL "Debug")
    set(_configuration_argument --debug)
endif()

execute_process(
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                ${_configuration_argument}
                --no-translations
                --no-compiler-runtime
                "${APPLICATION_FILE}"
        RESULT_VARIABLE _deploy_result
)

if(NOT _deploy_result EQUAL 0)
    message(FATAL_ERROR "windeployqt failed with code ${_deploy_result}")
endif()

