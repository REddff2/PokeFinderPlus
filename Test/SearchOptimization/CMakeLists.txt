find_package(Qt6 REQUIRED COMPONENTS Test)

function(search_check target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE PokeFinderCore Qt6::Core Threads::Threads)
    target_compile_definitions(${target} PRIVATE POKEFINDER_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endfunction()

search_check(SearchOptimizationCheck SearchOptimizationCheck.cpp BaselineEventGenerator5.cpp)
search_check(WildOptimizationCheck WildOptimizationCheck.cpp BaselineWildGenerator5.cpp BaselineWildSearcher5.cpp)
search_check(WildBulkCheck WildBulkCheck.cpp Milestone2WildGenerator5.cpp Milestone2WildSearcher5.cpp)
search_check(WildPhase3Check WildPhase3Check.cpp Phase2WildGenerator5.cpp)
search_check(WildIVDomainCheck WildIVDomainCheck.cpp BaselineWildGenerator5.cpp)
add_test(NAME SearchOptimizationEquivalence COMMAND SearchOptimizationCheck --verify)
add_test(NAME WildOptimizationEquivalence COMMAND WildOptimizationCheck --verify)
add_test(NAME WildBulkEquivalence COMMAND WildBulkCheck --verify)
add_test(NAME WildPhase3Equivalence COMMAND WildPhase3Check "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/wild-results.json")

# Counters are compiled only into these verification executables.
search_check(SearchOptimizationCounts SearchOptimizationCheck.cpp BaselineEventGenerator5.cpp
    ${CMAKE_SOURCE_DIR}/Core/Gen5/Generators/EventGenerator5.cpp
    ${CMAKE_SOURCE_DIR}/Core/Parents/Filters/StateFilter.cpp)
search_check(WildOptimizationCounts WildOptimizationCheck.cpp BaselineWildGenerator5.cpp BaselineWildSearcher5.cpp
    ${CMAKE_SOURCE_DIR}/Core/Gen5/Generators/WildGenerator5.cpp
    ${CMAKE_SOURCE_DIR}/Core/Gen5/Searchers/IVSearcher5.cpp
    ${CMAKE_SOURCE_DIR}/Core/Parents/Filters/StateFilter.cpp)
foreach(target IN ITEMS SearchOptimizationCounts WildOptimizationCounts)
    target_compile_definitions(${target} PRIVATE POKEFINDER_SEARCH_INSTRUMENTATION)
endforeach()

foreach(target IN ITEMS SearchSettingsCheck WildOptimizationUiCheck WildBulkUiCheck)
    add_executable(${target} ${target}.cpp)
    target_link_libraries(${target} PRIVATE PokeFinderForm PokeFinderModel PokeFinderPlusPluginManager PokeFinderCore Qt6::Widgets Qt6::Test)
    add_test(NAME ${target} COMMAND ${target} "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()

# Optional cache fixtures let the range tests verify genuine narrow-range positives
# and the actual form's normal cache-fast priority without a user's profile store.
set(SMART_SEARCH_IV_CACHE "" CACHE FILEPATH "IV cache fixture for legal-range verification")
set(SMART_SEARCH_SHA_CACHE "" CACHE FILEPATH "SHA cache fixture for dispatch verification")
if(SMART_SEARCH_IV_CACHE)
    add_test(NAME WildIVDomain COMMAND WildIVDomainCheck "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/wild-results.json"
        "${SMART_SEARCH_IV_CACHE}" "${CMAKE_CURRENT_BINARY_DIR}/iv-domain.json")
endif()
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/DispatchProbeWild5.cpp"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/GenerateDispatchProbe.py"
        "${CMAKE_SOURCE_DIR}/Form/Gen5/Wild5.cpp" "${CMAKE_CURRENT_BINARY_DIR}/DispatchProbeWild5.cpp"
    DEPENDS GenerateDispatchProbe.py ${CMAKE_SOURCE_DIR}/Form/Gen5/Wild5.cpp VERBATIM)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/DispatchProbeWild5.cpp" PROPERTIES SKIP_AUTOUIC ON SKIP_AUTOMOC ON)
add_executable(WildIVRangeProbe WildIVRangeProbe.cpp "${CMAKE_CURRENT_BINARY_DIR}/DispatchProbeWild5.cpp")
target_include_directories(WildIVRangeProbe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/Form/PokeFinderForm_autogen/include)
target_link_libraries(WildIVRangeProbe PRIVATE PokeFinderForm PokeFinderModel PokeFinderPlusPluginManager PokeFinderCore Qt6::Widgets Qt6::Test)
if(SMART_SEARCH_IV_CACHE AND SMART_SEARCH_SHA_CACHE)
    add_test(NAME WildIVRangeDispatch COMMAND WildIVRangeProbe --dispatch "${CMAKE_CURRENT_BINARY_DIR}"
        "${SMART_SEARCH_IV_CACHE}" "${SMART_SEARCH_SHA_CACHE}")
endif()
