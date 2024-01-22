#pragma once

#include <emuInstanceBase.hpp>
#include <utils.hpp>
#include <string>
#include <vector>

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "gfx.h"
#include "snapshot.h"
#include "controls.h"
#include "cheats.h"
#include "movie.h"
#include "logger.h"
#include "display.h"
#include "conffile.h"
#include "statemanager.h"

#include "unix.hpp"
#include "snes/snes.hpp"
extern thread_local bool doRendering;

class EmuInstance : public EmuInstanceBase
{
 public:

 uint8_t* _baseMem;
 uint8_t* _apuMem;

 EmuInstance(const nlohmann::json& config) : EmuInstanceBase(config)
 {
  // Checking whether configuration contains the rom file
  if (isDefined(config, "Rom File") == false) EXIT_WITH_ERROR("[ERROR] Configuration file missing 'Rom File' key.\n");
  std::string romFilePath = config["Rom File"].get<std::string>();

  // Checking whether configuration contains the state file
  if (isDefined(config, "State File") == false) EXIT_WITH_ERROR("[ERROR] Configuration file missing 'State File' key.\n");
  std::string stateFilePath = config["State File"].get<std::string>();

  int argc = 2;
  char romPath[4096];
  char bin[] = "./snes9x";
  strcpy(romPath, romFilePath.c_str());
  char* argv[2] = { bin, romPath };

  doRendering = false;
  initSnes9x(argc, argv);

  _baseMem = Memory.RAM;
  _apuMem = SNES::smp.apuram;
//  saveStateFile("boot.state");
//  exit(0);

   loadStateFile(stateFilePath);

//   Printing State size
//  printf("State Size: %u\n", S9xFreezeSizeFast());
//  exit(0);
 }

 void loadStateFile(const std::string& stateFilePath) override
 {
  if (stateFilePath == "") { S9xReset(); return; }

  // Loading state data
  std::string stateData;
  if (loadStringFromFile(stateData, stateFilePath.c_str()) == false) EXIT_WITH_ERROR("Could not find/read state file: '%s'\n", stateFilePath.c_str());

  // Loading state object into the emulator
  memStream stream((uint8_t*)stateData.data(), S9xFreezeSize());
  S9xUnfreezeFromStream(&stream);
 }

 void saveStateFile(const std::string& stateFilePath) const override
 {
  std::string stateBuffer;
  stateBuffer.resize(S9xFreezeSize());

  memStream stream((uint8_t*)stateBuffer.data(), S9xFreezeSize());
  S9xFreezeToStream(&stream);

  saveStringToFile(stateBuffer, stateFilePath.c_str());
 }

 void serializeState(uint8_t* state) const override
 {
#ifdef PREVENT_RENDERING
  memStream stream(state, _STATE_DATA_SIZE_TRAIN);
  S9xFreezeToStreamFast(&stream);
#else
  memStream stream(state, _STATE_DATA_SIZE_PLAY);
  S9xFreezeToStream(&stream);
#endif
 }

 void deserializeState(const uint8_t* state) override
 {
#ifdef PREVENT_RENDERING
  memStream stream(state, _STATE_DATA_SIZE_TRAIN);
  S9xUnfreezeFromStreamFast(&stream);
#else
  memStream stream(state, _STATE_DATA_SIZE_PLAY);
  S9xUnfreezeFromStream(&stream);
#endif
 }

 void advanceState(const INPUT_TYPE move) override
 {
  MovieSetJoypad(0, move);
  S9xMainLoop();
  //printf("move %u -> %u\n", move, *((uint16*)&_baseMem[0x0272]));
 }

};
