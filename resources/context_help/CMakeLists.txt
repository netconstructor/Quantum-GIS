FILE(GLOB HELP_FILES *-*) 

INSTALL(FILES ${HELP_FILES} DESTINATION ${QGIS_DATA_DIR}/resources/context_help)

