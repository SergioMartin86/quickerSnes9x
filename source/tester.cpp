#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/serializers/differential.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/deserializers/differential.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/file.hpp>
#include "snes9xInstance.hpp"
#include <chrono>
#include <sstream>
#include <vector>
#include <string>


int main(int argc, char *argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("tester", "1.0");

  program.add_argument("scriptFile")
    .help("Path to the test script file to run.")
    .required();

  program.add_argument("--cycleType")
    .help("Specifies the emulation actions to be performed per each input. Possible values: 'Simple': performs only advance state, 'Rerecord': performs load/advance/save, and 'Full': performs load/advance/save/advance.")
    .default_value(std::string("Simple"));

  program.add_argument("--hashOutputFile")
    .help("Path to write the hash output to.")
    .default_value(std::string(""));

  // Try to parse arguments
  try { program.parse_args(argc, argv); } catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  // Getting test script file path
  const auto scriptFilePath = program.get<std::string>("scriptFile");

  // Getting path where to save the hash output (if any)
  const auto hashOutputFile = program.get<std::string>("--hashOutputFile");

  // Getting reproduce flag
  const auto cycleType = program.get<std::string>("--cycleType");

  // Loading script file
  std::string configJsRaw;
  if (jaffarCommon::file::loadStringFromFile(configJsRaw, scriptFilePath) == false) JAFFAR_THROW_LOGIC("Could not find/read script file: %s\n", scriptFilePath.c_str());

  // Parsing script
  const auto configJs = nlohmann::json::parse(configJsRaw);

  // Getting rom file path
  const auto romFilePath = jaffarCommon::json::getString(configJs, "Rom File");

  // Getting initial state file path
  const auto initialStateFilePath = jaffarCommon::json::getString(configJs, "Initial State File");

  // Getting sequence file path
  const auto sequenceFilePath = jaffarCommon::json::getString(configJs, "Sequence File");

  // Getting expected ROM SHA1 hash
  const auto expectedROMSHA1 = jaffarCommon::json::getString(configJs, "Expected ROM SHA1");

  // Parsing disabled blocks in lite state serialization
  const auto stateDisabledBlocks = jaffarCommon::json::getArray<std::string>(configJs, "Disable State Blocks");
  std::string stateDisabledBlocksOutput;
  for (const auto& entry : stateDisabledBlocks) stateDisabledBlocksOutput += entry + std::string(" ");
  
  // Getting Controller 1 type
  const auto controller1Type = jaffarCommon::json::getString(configJs, "Controller 1 Type");

  // Getting Controller 2 type
  const auto controller2Type = jaffarCommon::json::getString(configJs, "Controller 2 Type");

  // Creating emulator instance
  auto e = snes9x::EmuInstance();

  // Setting controller types
  e.setController1Type(controller1Type);
  e.setController2Type(controller2Type);

  // Disabling requested blocks from light state serialization
  for (const auto& block : stateDisabledBlocks) e.disableLiteStateBlock(block);

  // Loading ROM File
  e.loadROMFile(romFilePath);

  // If an initial state is provided, load it now
  if (initialStateFilePath != "") e.loadStateFile(initialStateFilePath);

  // Disable rendering
  e.disableRendering();

  // Getting lite state size
  const auto liteStateSize = e.getLiteStateSize();

  // Getting actual ROM SHA1
  auto romSHA1 = e.getRomSHA1();

  // Checking with the expected SHA1 hash
  if (romSHA1 != expectedROMSHA1) JAFFAR_THROW_LOGIC("Wrong ROM SHA1. Found: '%s', Expected: '%s'\n", romSHA1.c_str(), expectedROMSHA1.c_str());

  // Loading sequence file
  std::string sequenceRaw;
  if (jaffarCommon::file::loadStringFromFile(sequenceRaw, sequenceFilePath) == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from input sequence file: %s\n", sequenceFilePath.c_str());

  // Building sequence information
  const auto sequence = jaffarCommon::string::split(sequenceRaw, ' ');

  // Getting sequence lenght
  const auto sequenceLength = sequence.size();

  // Getting emulation core name
  std::string emulationCoreName = e.getCoreName();

  // Printing test information
  jaffarCommon::logger::log("[] -----------------------------------------\n");
  jaffarCommon::logger::log("[] Running Script:          '%s'\n", scriptFilePath.c_str());
  jaffarCommon::logger::log("[] Cycle Type:              '%s'\n", cycleType.c_str());
  jaffarCommon::logger::log("[] Emulation Core:          '%s'\n", emulationCoreName.c_str());
  jaffarCommon::logger::log("[] ROM File:                '%s'\n", romFilePath.c_str());
  jaffarCommon::logger::log("[] Controller Types:        '%s' / '%s'\n", controller1Type.c_str(), controller2Type.c_str());
  //jaffarCommon::logger::log("[] ROM SHA1:                '%s'\n", romSHA1.c_str());
  jaffarCommon::logger::log("[] Sequence File:           '%s'\n", sequenceFilePath.c_str());
  jaffarCommon::logger::log("[] Sequence Length:         %lu\n", sequenceLength);
  jaffarCommon::logger::log("[] State Size:              %lu bytes - Disabled Blocks:  [ %s ]\n", e.getLiteStateSize(), stateDisabledBlocksOutput.c_str());
  jaffarCommon::logger::log("[] ********** Running Test **********\n");

  fflush(stdout);

  // Serializing initial state
  uint8_t *currentState = (uint8_t *)malloc(liteStateSize);
  e.serializeLiteState(currentState);

  // Check whether to perform each action
  bool doPreAdvance = cycleType == "Full";
  bool doDeserialize = cycleType == "Rerecord" || cycleType == "Full";
  bool doSerialize = cycleType == "Rerecord" || cycleType == "Full";

  // Actually running the sequence
  auto t0 = std::chrono::high_resolution_clock::now();
  for (const std::string &input : sequence)
  {
    if (doPreAdvance == true) e.advanceState(input);
    if (doDeserialize == true) e.deserializeLiteState(currentState);
    e.advanceState(input);
    if (doSerialize == true) e.serializeLiteState(currentState);
  }
  auto tf = std::chrono::high_resolution_clock::now();

  // Calculating running time
  auto dt = std::chrono::duration_cast<std::chrono::nanoseconds>(tf - t0).count();
  double elapsedTimeSeconds = (double)dt * 1.0e-9;

  // Calculating final state hash
  const auto finalStateHash = e.getStateHash();

  // Creating hash string
  char hashStringBuffer[256];
  jaffarCommon::logger::log(hashStringBuffer, "0x%lX%lX", finalStateHash.first, finalStateHash.second);

  // Printing time information
  jaffarCommon::logger::log("[] Elapsed time:            %3.3fs\n", (double)dt * 1.0e-9);
  jaffarCommon::logger::log("[] Performance:             %.3f inputs / s\n", (double)sequenceLength / elapsedTimeSeconds);
  jaffarCommon::logger::log("[] Final State Hash:        %s\n", hashStringBuffer);

  // If saving hash, do it now
  if (hashOutputFile != "") jaffarCommon::file::saveStringToFile(std::string(hashStringBuffer), hashOutputFile.c_str());

  // If reached this point, everything ran ok
  return 0;
}
