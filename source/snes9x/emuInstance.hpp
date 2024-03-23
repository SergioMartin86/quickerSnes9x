#pragma once

#include <emuInstanceBase.hpp>
#include <utils.hpp>
#include <string>
#include <vector>
#include "snes/snes.hpp"

extern void initSnes9x();

class EmuInstance : public EmuInstanceBase
{
 public:

 uint8_t* _baseMem;
 uint8_t* _apuMem;

 EmuInstance() : EmuInstanceBase()
 {
  initSnes9x();
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

  void serializeFullState(uint8_t *state) const override
  {
    memStream stream(state, _fullStateSize);
    S9xFreezeToStream(&stream);
  }

  void deserializeFullState(const uint8_t *state) override
  {
    // Loading state object into the emulator
    memStream stream(state, _fullStateSize);
    S9xUnfreezeFromStream(&stream);
  }

  void serializeLiteState(uint8_t *state) const override
  {
    memStream stream(state, _liteStateSize);
    S9xFreezeToStream(&stream);
  }

  void deserializeLiteState(const uint8_t *state) override
  {
    // Loading state object into the emulator
    memStream stream(state, _liteStateSize);
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
