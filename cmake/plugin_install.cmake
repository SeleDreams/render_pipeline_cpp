install(TARGETS ${PROJECT_NAME} DESTINATION ${RPPLUGIN_INSTALL_DIR})
install(TARGETS ${RPPLUGIN_ID} EXPORT ${TARGET_EXPORT_NAME})
export(EXPORT ${TARGET_EXPORT_NAME}
    NAMESPACE ${TARGET_NAMESPACE}
    FILE "${PROJECT_BINARY_DIR}/${TARGET_EXPORT_NAME}.cmake"
)

install(FILES "${PROJECT_SOURCE_DIR}/config.yaml" DESTINATION ${RPPLUGIN_INSTALL_DIR})
foreach(directory_name "include" "resources" "shader")
    if(EXISTS "${PROJECT_SOURCE_DIR}/${directory_name}")
        install(DIRECTORY "${PROJECT_SOURCE_DIR}/${directory_name}" DESTINATION ${RPPLUGIN_INSTALL_DIR})
    endif()
endforeach()

install(FILES ${PACKAGE_CONFIG_FILE} ${PACKAGE_VERSION_CONFIG_FILE} DESTINATION ${PACKAGE_CMAKE_INSTALL_DIR})
install(EXPORT ${TARGET_EXPORT_NAME} NAMESPACE ${TARGET_NAMESPACE} DESTINATION ${PACKAGE_CMAKE_INSTALL_DIR})
