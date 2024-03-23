#pragma once

#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/file.hpp>
#include "controller.hpp"

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "gfx.h"
#include "snapshot.h"
#include "controls.h"
#include "cheats.h"
#include "movie.h"
#include "display.h"
#include "conffile.h"
#include "statemanager.h"
#include "snapshot.h"

namespace snes9x
{

class EmuInstanceBase
{
  public:

  EmuInstanceBase() = default;
  virtual ~EmuInstanceBase() = default;

  inline void advanceState(const std::string &move)
  {
    bool isInputValid = _controller.parseInputString(move);
    if (isInputValid == false) ("Move provided cannot be parsed: '%s'\n", move.c_str());

    // Parsing power
    if (_controller.getPowerButtonState() == true) JAFFAR_THROW_RUNTIME("Power button pressed, but not supported: '%s'\n", move.c_str());

    // Parsing reset
    if (_controller.getResetButtonState() == true) doSoftReset();

    // Parsing Controllers
    const auto controller1 = _controller.getController1Code();
    const auto controller2 = _controller.getController2Code();

    advanceStateImpl(controller1, controller2);
  }

  inline void setController1Type(const std::string& type)
  {
    bool isTypeRecognized = false;

    if (type == "None") { _controller.setController1Type(Controller::controller_t::none); isTypeRecognized = true; }
    if (type == "Joypad") { _controller.setController1Type(Controller::controller_t::joypad); isTypeRecognized = true; }

    if (isTypeRecognized == false) JAFFAR_THROW_RUNTIME("Input type not recognized: '%s'\n", type.c_str());
  }

  inline void setController2Type(const std::string& type)
  {
    bool isTypeRecognized = false;

    if (type == "None") { _controller.setController2Type(Controller::controller_t::none); isTypeRecognized = true; }
    if (type == "Joypad") { _controller.setController2Type(Controller::controller_t::joypad); isTypeRecognized = true; }
    
    if (isTypeRecognized == false) JAFFAR_THROW_RUNTIME("Input type not recognized: '%s'\n", type.c_str());
  }

  inline std::string getRomSHA1() const { return _romSHA1String; }

  inline jaffarCommon::hash::hash_t getStateHash() const
  {
    MetroHash128 hash;
    
    hash.Update(Memory.RAM, 0x20000);

    jaffarCommon::hash::hash_t result;
    hash.Finalize(reinterpret_cast<uint8_t *>(&result));
    return result;
  }

  inline void enableRendering()
   {
      S9xInitInputDevices();
      S9xInitDisplay(0, NULL);
      S9xSetupDefaultKeymap();
      S9xTextMode();
      S9xGraphicsMode();
      S9xSetTitle(String);

     _doRendering = true; 
   };
  inline void disableRendering() { _doRendering = false; };

  inline void loadStateFile(const std::string &stateFilePath)
  {
    std::string stateData;
    bool status = jaffarCommon::file::loadStringFromFile(stateData, stateFilePath);
    if (status == false) JAFFAR_THROW_RUNTIME("Could not find/read state file: %s\n", stateFilePath.c_str());
    deserializeFullState((uint8_t *)stateData.data());
  }

  inline void saveStateFile(const std::string &stateFilePath) const
  {
    std::string stateData;
    stateData.resize(_fullStateSize);
    serializeFullState((uint8_t *)stateData.data());
    jaffarCommon::file::saveStringToFile(stateData, stateFilePath.c_str());
  }

  inline void loadROMFile(const std::string &romFilePath)
  {
    // Loading ROM data
    bool status = jaffarCommon::file::loadStringFromFile(_romData, romFilePath);
    if (status == false) JAFFAR_THROW_RUNTIME("Could not find/read ROM file: %s\n", romFilePath.c_str());

    // Calculating ROM hash value
    _romSHA1String = jaffarCommon::hash::getSHA1String(_romData);

    // Actually loading rom file
    status = loadROMFileImpl(_romData);
    if (status == false) JAFFAR_THROW_RUNTIME("Could not process ROM file: %s\n", romFilePath.c_str());

    // Detecting full state size
    _fullStateSize = getFullStateSize();

    // Detecting lite state size
    _liteStateSize = getLiteStateSize();
  }

  // Virtual functions

  virtual bool loadROMFileImpl(const std::string &romData) = 0;
  virtual void advanceStateImpl(const Controller::port_t controller1, const Controller::port_t controller2) = 0;
  virtual void serializeFullState(uint8_t *state) const = 0;
  virtual void deserializeFullState(const uint8_t *state) = 0;
  virtual void serializeLiteState(uint8_t *state) const = 0;
  virtual void deserializeLiteState(const uint8_t *state) = 0;
  virtual size_t getFullStateSize() const = 0;
  virtual size_t getLiteStateSize() const = 0;
  virtual void enableLiteStateBlock(const std::string& block) = 0;
  virtual void disableLiteStateBlock(const std::string& block) = 0;

  virtual void doSoftReset() = 0;
  virtual void doHardReset() = 0;
  virtual std::string getCoreName() const = 0;

  protected:

  // Storage for the light state size
  size_t _liteStateSize;

  // Storage for the full state size
  size_t _fullStateSize;

  // Flag to determine whether to enable/disable rendering
  bool _doRendering = true;

  private:
  // Storage for the ROM data
  std::string _romData;

  // SHA1 rom hash
  std::string _romSHA1String;

  // Controller class for input parsing
  Controller _controller;
};

} // namespace snes9x