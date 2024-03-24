#pragma once

#include "../snes9xInstanceBase.hpp"
#include <string>
#include <vector>
#include "unix.hpp"
#include "snes/snes.hpp"

extern thread_local bool doRendering;

namespace snes9x
{

class EmuInstance : public EmuInstanceBase
{
 public:

 uint8_t* _baseMem;
 uint8_t* _apuMem;

 EmuInstance() : EmuInstanceBase()
 {
  int argc = 1;
  char bin[] = "./snes9x";
  char* argv[1] = { bin };

  doRendering = false;
  initSnes9x(argc, argv);
 }

  virtual bool loadROMFileImpl(const std::string &romData) override
  {
    uint32 saved_flags = CPU.Flags;

    Memory.LoadROMMem ((const uint8*) romData.data(), romData.size());

    CPU.Flags = saved_flags;
    Settings.StopEmulation = FALSE;

    _baseMem = Memory.RAM;
    _apuMem = SNES::smp.apuram;
    
    return true;
  }

  void enableRendering() override
  {
    doRendering = true;
    S9xInitInputDevices();
    S9xInitDisplay(0, NULL);
    S9xSetupDefaultKeymap();
    S9xTextMode();
    S9xGraphicsMode();
    S9xSetTitle(String);
  }

  void disableRendering() override
  {
    doRendering = false;
  }

  void serializeFullState(jaffarCommon::serializer::Base& s) const override
  {
    std::string stateData;
    stateData.resize(_fullStateSize);
    memStream stream((uint8_t*)stateData.data(), stateData.size());
    S9xFreezeToStream(&stream);
    s.push(stateData.data(), _fullStateSize);
  }

  void deserializeFullState(jaffarCommon::deserializer::Base& d) override
  {
    std::string stateData;
    stateData.resize(_fullStateSize);
    d.pop(stateData.data(),stateData.size());
    memStream stream((uint8_t*)stateData.data(), stateData.size());
    S9xUnfreezeFromStream(&stream);
  }

  void serializeLiteState(jaffarCommon::serializer::Base& s) const override
  {
    std::string stateData;
    stateData.resize(_liteStateSize);
    memStream stream((uint8_t*)stateData.data(), stateData.size());
    S9xFreezeToStream(&stream);
    s.push(stateData.data(), stateData.size());
  }

  void deserializeLiteState(jaffarCommon::deserializer::Base& d) override
  {
    std::string stateData;
    stateData.resize(_liteStateSize);
    d.pop(stateData.data(),stateData.size());
    memStream stream((uint8_t*)stateData.data(), stateData.size());
    S9xUnfreezeFromStream(&stream);
  }

  size_t getFullStateSize() const override
  {
    return S9xFreezeSize();
  }

  size_t getLiteStateSize() const override
  {
    return S9xFreezeSize();
  }

  inline size_t getDifferentialStateSize() const override { return getFullStateSize(); }

  void enableLiteStateBlock(const std::string& block)
  {
    // Nothing to do here
  }

  void disableLiteStateBlock(const std::string& block)
  {
    // Nothing to do here
  }

  void doSoftReset() override
  {
    S9xSoftReset();
  }
  
  void doHardReset() override
  {
    S9xReset();
  }

  std::string getCoreName() const override { return "snes9x"; }


  virtual void advanceStateImpl(const Controller::port_t controller1, const Controller::port_t controller2)
  {
    MovieSetJoypad(0, controller1);
    MovieSetJoypad(1, controller2);
    S9xMainLoop();
  }

};

} // namespace snes9x