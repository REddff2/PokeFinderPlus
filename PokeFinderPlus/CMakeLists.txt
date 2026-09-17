add_library(PokeFinderPlusPluginManager STATIC
    PluginApi.hpp
    PluginManager.cpp
    PluginManager.hpp
)

target_include_directories(PokeFinderPlusPluginManager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PokeFinderPlusPluginManager PUBLIC Qt6::Core Qt6::Widgets)

add_library(Annotations SHARED
    Annotations.cpp
    Annotations/AnnotationWindow.cpp
    Annotations/AnnotationWindow.hpp
    Annotations/AnnotationController.cpp
    Annotations/AnnotationController.hpp
    Annotations/AnnotationDelegate.cpp
    Annotations/AnnotationDelegate.hpp
)
target_include_directories(Annotations PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Annotations PRIVATE Qt6::Core Qt6::Widgets)
set_target_properties(Annotations PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

if (WIN32)
    add_executable(PokeFinderPlusLauncher WIN32 PokeFinderPlusLauncher.cpp)
    set_target_properties(PokeFinderPlusLauncher PROPERTIES
        OUTPUT_NAME "PokeFinder+"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    target_link_libraries(PokeFinderPlusLauncher PRIVATE Shell32)
endif ()

add_library(IVTotal SHARED
    IVTotal/IVTotalPlugin.cpp
    IVTotal/IVTotalController.cpp
    IVTotal/IVTotalController.hpp
    IVTotal/IVTotalProxyModel.cpp
    IVTotal/IVTotalProxyModel.hpp
)
target_link_libraries(IVTotal PRIVATE Qt6::Core Qt6::Widgets)
set_target_properties(IVTotal PROPERTIES
    PREFIX ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)
