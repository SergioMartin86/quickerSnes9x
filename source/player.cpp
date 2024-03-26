#include <jaffarCommon/file.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/logger.hpp>
#include <jaffarCommon/json.hpp>
#include "argparse/argparse.hpp"
#include "snes9xInstance.hpp"
#include "playbackInstance.hpp"

int main(int argc, char *argv[])
{
  // Parsing command line arguments
  argparse::ArgumentParser program("player", "1.0");

  program.add_argument("scriptFile")
    .help("Path to the test script file to run.")
    .required();

  program.add_argument("sequenceFile")
    .help("Path to the input sequence file (.sol) to reproduce.")
    .required();

  program.add_argument("--reproduce")
    .help("Plays the entire sequence without interruptions and exit at the end.")
    .default_value(false)
    .implicit_value(true);

  program.add_argument("--storeInterval")
    .help("Every how many frames to store the state (to save memory)")
    .default_value(std::string("1"));

  program.add_argument("--disableRender")
    .help("Do not render game window.")
    .default_value(false)
    .implicit_value(true);


  // Try to parse arguments
  try { program.parse_args(argc, argv); } catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  // Getting test script file path
  const auto scriptFilePath = program.get<std::string>("scriptFile");

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
  std::string sequenceFilePath = program.get<std::string>("sequenceFile");

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

  // Getting reproduce flag
  bool isReproduce = program.get<bool>("--reproduce");

  // Getting reproduce flag
  bool disableRender = program.get<bool>("--disableRender");

  // Getting store interval
  size_t storeInterval = std::stoi(program.get<std::string>("--storeInterval"));

  // Loading sequence file
  std::string inputSequence;
  auto status = jaffarCommon::file::loadStringFromFile(inputSequence, sequenceFilePath.c_str());
  if (status == false) JAFFAR_THROW_LOGIC("[ERROR] Could not find or read from sequence file: %s\n", sequenceFilePath.c_str());

  // Building sequence information
  const auto sequence = jaffarCommon::string::split(inputSequence, ' ');

  // Initializing terminal
  jaffarCommon::logger::initializeTerminal();

  // Printing provided parameters
  jaffarCommon::logger::log("[] Rom File Path:      '%s'\n", romFilePath.c_str());
  jaffarCommon::logger::log("[] Sequence File Path: '%s'\n", sequenceFilePath.c_str());
  jaffarCommon::logger::log("[] Sequence Length:    %lu\n", sequence.size());
  jaffarCommon::logger::log("[] State File Path:    '%s'\n", initialStateFilePath.empty() ? "<Boot Start>" : initialStateFilePath.c_str());
  jaffarCommon::logger::log("[] Generating Sequence...\n");

  jaffarCommon::logger::refreshTerminal();

  // Creating emulator instance 
  auto e = snes9x::EmuInstance();

  // Setting controller types
  e.setController1Type(controller1Type);
  e.setController2Type(controller2Type);
  
  // Loading ROM File
  std::string romFileData;
  if (jaffarCommon::file::loadStringFromFile(romFileData, romFilePath) == false) JAFFAR_THROW_LOGIC("Could not rom file: %s\n", romFilePath.c_str());
  e.loadROM(romFileData);

  // Calculating ROM SHA1
  auto romSHA1 = jaffarCommon::hash::getSHA1String(romFileData);

  // Checking with the expected SHA1 hash
  if (romSHA1 != expectedROMSHA1) JAFFAR_THROW_LOGIC("Wrong ROM SHA1. Found: '%s', Expected: '%s'\n", romSHA1.c_str(), expectedROMSHA1.c_str());

  // If an initial state is provided, load it now
  if (initialStateFilePath != "")
  {
    std::string stateFileData;
    if (jaffarCommon::file::loadStringFromFile(stateFileData, initialStateFilePath) == false) JAFFAR_THROW_LOGIC("Could not initial state file: %s\n", initialStateFilePath.c_str());
    jaffarCommon::deserializer::Contiguous d(stateFileData.data());
    e.deserializeState(d);
  }

  // Creating playback instance
  auto p = PlaybackInstance(&e, sequence, storeInterval);

  // Getting state size
  auto stateSize = e.getStateSize();

  // Flag to continue running playback
  bool continueRunning = true;

  // Variable for current step in view
  ssize_t sequenceLength = p.getSequenceLength();
  ssize_t currentStep = 0;

  // Flag to display frame information
  bool showFrameInfo = true;

  // If rendering enabled, then initailize it now
  if (disableRender == false) e.enableRendering();

  // Interactive section
  while (continueRunning)
  {
    // Updating display
    if (disableRender == false) p.renderFrame(currentStep);

    // Getting input
    const auto &input = p.getStateInput(currentStep);

    // Getting state hash
    const auto hash = p.getStateHash(currentStep);

    // Getting state data
    const auto stateData = p.getStateData(currentStep);

    // Printing data and commands
    if (showFrameInfo)
    {
      jaffarCommon::logger::clearTerminal();

      jaffarCommon::logger::log("[] ----------------------------------------------------------------\n");
      jaffarCommon::logger::log("[] Current Step #: %lu / %lu\n", currentStep + 1, sequenceLength);
      jaffarCommon::logger::log("[] Input:          %s\n", input.c_str());
      jaffarCommon::logger::log("[] State Hash:     0x%lX%lX\n", hash.first, hash.second);

      // Only print commands if not in reproduce mode
      if (isReproduce == false) jaffarCommon::logger::log("[] Commands: n: -1 m: +1 | h: -10 | j: +10 | y: -100 | u: +100 | k: -1000 | i: +1000 | s: quicksave | p: play | q: quit\n");

      jaffarCommon::logger::refreshTerminal();
    }

    // Resetting show frame info flag
    showFrameInfo = true;

    // Get command
    auto command = jaffarCommon::logger::getKeyPress();

    // Advance/Rewind commands
    if (command == 'n') currentStep = currentStep - 1;
    if (command == 'm') currentStep = currentStep + 1;
    if (command == 'h') currentStep = currentStep - 10;
    if (command == 'j') currentStep = currentStep + 10;
    if (command == 'y') currentStep = currentStep - 100;
    if (command == 'u') currentStep = currentStep + 100;
    if (command == 'k') currentStep = currentStep - 1000;
    if (command == 'i') currentStep = currentStep + 1000;

    // Correct current step if requested more than possible
    if (currentStep < 0) currentStep = 0;
    if (currentStep >= sequenceLength) currentStep = sequenceLength - 1;

    // Quicksave creation command
    if (command == 's')
    {
      // Storing state file
      std::string saveFileName = "quicksave.state";

      std::string saveData;
      saveData.resize(stateSize);
      memcpy(saveData.data(), stateData, stateSize);
      if (jaffarCommon::file::saveStringToFile(saveData, saveFileName.c_str()) == false) JAFFAR_THROW_RUNTIME("[ERROR] Could not save state file: %s\n", saveFileName.c_str());
      jaffarCommon::logger::log("[] Saved state to %s\n", saveFileName.c_str());

      // Do no show frame info again after this action
      showFrameInfo = false;
    }

    // Start playback from current point
    if (command == 'p') isReproduce = true;

    // Start playback from current point
    if (command == 'q') continueRunning = false;
  }

  // If rendering enabled, then finalize it now
  if (disableRender == false) e.enableRendering();

  // Ending ncurses window
  jaffarCommon::logger::finalizeTerminal();
}
