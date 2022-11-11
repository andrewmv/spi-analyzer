cmake_minimum_required (VERSION 3.11)
project(mispi_analyzer)

add_definitions( -DLOGIC2 )

# enable generation of compile_commands.json, helpful for IDEs to locate include files.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# custom CMake Modules are located in the cmake directory.
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(ExternalAnalyzerSDK)

set(SOURCES 
src/MiSpiAnalyzer.cpp
src/MiSpiAnalyzer.h
src/MiSpiAnalyzerResults.cpp
src/MiSpiAnalyzerResults.h
src/MiSpiAnalyzerSettings.cpp
src/MiSpiAnalyzerSettings.h
src/MiSpiSimulationDataGenerator.cpp
src/MiSpiSimulationDataGenerator.h
)

add_analyzer_plugin(mispi_analyzer SOURCES ${SOURCES})
