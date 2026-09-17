cmake_minimum_required(VERSION 3.31)
project(SlotSprites LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
set(CMAKE_AUTORCC ON)
set(POKEFINDER_SOURCE "C:/PokeFinderDev/PokeFinderPlus-src" CACHE PATH "Matching frozen PokeFinder+ source")
add_library(SlotSprites SHARED SlotSpritesPlugin.cpp SlotSpritesController.cpp SlotSpriteStyle.cpp
    ResultIdentity.cpp SpriteResolver.cpp SpriteCache.cpp PokemonControls.cpp AssetInstaller.cpp WinHttpDownload.cpp resources/install.qrc
    vendor/miniz/miniz.c vendor/miniz/miniz_tinfl.c vendor/miniz/miniz_tdef.c vendor/miniz/miniz_zip.c)
target_include_directories(SlotSprites SYSTEM PRIVATE "${POKEFINDER_SOURCE}")
target_include_directories(SlotSprites PRIVATE vendor/miniz)
target_compile_definitions(SlotSprites PRIVATE MINIZ_NO_ARCHIVE_WRITING_APIS MINIZ_NO_ZLIB_COMPATIBLE_NAMES)
target_link_libraries(SlotSprites PRIVATE Qt6::Widgets winhttp)
target_compile_options(SlotSprites PRIVATE /W4 /utf-8)
set_target_properties(SlotSprites PROPERTIES PREFIX "")

option(SLOT_SPRITES_VERIFY "Build local verification host" ON)
if(SLOT_SPRITES_VERIFY)
    add_executable(SlotSpritesExportControlData verification/ExportControlData.cpp)
    target_include_directories(SlotSpritesExportControlData PRIVATE "${POKEFINDER_SOURCE}")
    target_link_libraries(SlotSpritesExportControlData PRIVATE Qt6::Core
        "C:/PokeFinderDev/build-plus-parent/Core/PokeFinderCore.lib"
        "C:/PokeFinderDev/build-plus-parent/Core/External/zstd/lib/zstd_static.lib")
    add_library(SlotSpritesNetworkProbe SHARED verification/EarlyNetworkProbe.cpp)
    target_include_directories(SlotSpritesNetworkProbe PRIVATE "${POKEFINDER_SOURCE}")
    target_link_libraries(SlotSpritesNetworkProbe PRIVATE Qt6::Widgets winhttp)
    add_executable(SlotSpritesSetupVerify verification/SetupVerify.cpp)
    target_include_directories(SlotSpritesSetupVerify PRIVATE "${POKEFINDER_SOURCE}")
    target_link_libraries(SlotSpritesSetupVerify PRIVATE Qt6::Widgets winhttp)
    add_dependencies(SlotSpritesSetupVerify SlotSprites)
    add_library(SlotSpritesHostProbe SHARED verification/HostProbe.cpp verification/ControlProbe.cpp verification/BroadProbe.cpp ResultIdentity.cpp SpriteResolver.cpp)
    target_include_directories(SlotSpritesHostProbe PRIVATE . "${POKEFINDER_SOURCE}")
    target_compile_options(SlotSpritesHostProbe PRIVATE /utf-8)
    target_link_libraries(SlotSpritesHostProbe PRIVATE Qt6::Widgets)
    add_executable(SlotSpritesVerify verification/Verify.cpp verification/Coverage.cpp
        ResultIdentity.cpp SpriteResolver.cpp SpriteCache.cpp
        "${POKEFINDER_SOURCE}/Model/Gen3/WildModel3.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen3/WildModel3.hpp"
        "${POKEFINDER_SOURCE}/Model/Util/LeadDisplay.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen3/PokeSpotModel.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen4/WildModel4.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen4/PokeRadarModel.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen5/WildModel5.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen5/HiddenGrottoModel.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen5/PhenomenonModel.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen5/PickupModel.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen8/WildModel8.cpp"
        "${POKEFINDER_SOURCE}/Model/Gen8/UndergroundModel.cpp"
        "${POKEFINDER_SOURCE}/PokeFinderPlus/Annotations/AnnotationController.cpp"
        "${POKEFINDER_SOURCE}/PokeFinderPlus/Annotations/AnnotationDelegate.cpp"
        "${POKEFINDER_SOURCE}/PokeFinderPlus/IVTotal/IVTotalController.cpp"
        "${POKEFINDER_SOURCE}/PokeFinderPlus/IVTotal/IVTotalProxyModel.cpp")
    target_include_directories(SlotSpritesVerify PRIVATE . "${POKEFINDER_SOURCE}")
    target_compile_options(SlotSpritesVerify PRIVATE /utf-8)
    target_link_libraries(SlotSpritesVerify PRIVATE Qt6::Widgets
        "C:/PokeFinderDev/build-plus-parent/Core/PokeFinderCore.lib"
        "C:/PokeFinderDev/build-plus-parent/Core/External/zstd/lib/zstd_static.lib")
    add_dependencies(SlotSpritesVerify SlotSprites)
endif()
