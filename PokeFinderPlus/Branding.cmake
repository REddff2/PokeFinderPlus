# Branding is scoped to the fork's executable targets; upstream assets stay intact.
target_sources(PokeFinder PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/Branding.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/Branding.hpp"
)

qt_add_resources(PokeFinder PokeFinderPlusBranding
    PREFIX "/PokeFinderPlus"
    BASE "${CMAKE_CURRENT_LIST_DIR}/resources"
    FILES "${CMAKE_CURRENT_LIST_DIR}/resources/pokefinder-plus.ico"
)

if (WIN32)
    set(POKEFINDER_PLUS_ICON "${CMAKE_CURRENT_LIST_DIR}/resources/pokefinder-plus.ico")
    set(POKEFINDER_PLUS_RC "${CMAKE_BINARY_DIR}/PokeFinderPlus/branding.rc")
    configure_file("${CMAKE_CURRENT_LIST_DIR}/branding.rc.in" "${POKEFINDER_PLUS_RC}" @ONLY)
    target_sources(PokeFinder PRIVATE "${POKEFINDER_PLUS_RC}")
    target_sources(PokeFinderPlusLauncher PRIVATE "${POKEFINDER_PLUS_RC}")
    set_source_files_properties("${POKEFINDER_PLUS_RC}" PROPERTIES OBJECT_DEPENDS "${POKEFINDER_PLUS_ICON}")
endif ()
