#pragma once

#include "../snes9xInstanceBase.hpp"
#include <string>
#include <vector>
#include "unix.hpp"
#include "snes/snes.hpp"
#include <lightStateConfig.h>

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
    S9xFreezeToStreamLite(&stream);
  }

  void deserializeLiteState(const uint8_t *state) override
  {
    // Loading state object into the emulator
    memStream stream(state, _liteStateSize);
    S9xUnfreezeFromStreamLite(&stream);
  }

  size_t getFullStateSize() const override
  {
    return S9xFreezeSize();
  }

  size_t getLiteStateSize() const override
  {
    return S9xFreezeSizeLite();
  }


  void enableLiteStateBlock(const std::string& block)
  { 
    bool recognizedBlock = false;
    
    if (block == "PPU") { _enablePPUBlock = true; recognizedBlock = true; }
    if (block == "DMA") { _enableDMABlock = true; recognizedBlock = true; }
    if (block == "VRA") { _enableVRABlock = true; recognizedBlock = true; }
    if (block == "RAM") { _enableRAMBlock = true; recognizedBlock = true; }
    if (block == "SRA") { _enableSRABlock = true; recognizedBlock = true; }
    if (block == "FIL") { _enableFILBlock = true; recognizedBlock = true; }
    if (block == "SND") { _enableSNDBlock = true; recognizedBlock = true; }
    if (block == "CTL") { _enableCTLBlock = true; recognizedBlock = true; }
    if (block == "TIM") { _enableTIMBlock = true; recognizedBlock = true; }

    if (recognizedBlock == false) { fprintf(stderr, "Unrecognized block type: %s\n", block.c_str()); exit(-1);}
  };


  void disableLiteStateBlock(const std::string& block)
  { 
    bool recognizedBlock = false;
    
    if (block == "PPU") { _enablePPUBlock = false; recognizedBlock = true; }
    if (block == "DMA") { _enableDMABlock = false; recognizedBlock = true; }
    if (block == "VRA") { _enableVRABlock = false; recognizedBlock = true; }
    if (block == "RAM") { _enableRAMBlock = false; recognizedBlock = true; }
    if (block == "SRA") { _enableSRABlock = false; recognizedBlock = true; }
    if (block == "FIL") { _enableFILBlock = false; recognizedBlock = true; }
    if (block == "SND") { _enableSNDBlock = false; recognizedBlock = true; }
    if (block == "CTL") { _enableCTLBlock = false; recognizedBlock = true; }
    if (block == "TIM") { _enableTIMBlock = false; recognizedBlock = true; }

    if (recognizedBlock == false) { fprintf(stderr, "Unrecognized block type: %s\n", block.c_str()); exit(-1);}
  };


  void doSoftReset() override
  {
    S9xSoftReset();
  }
  
  void doHardReset() override
  {
    S9xReset();
  }

  std::string getCoreName() const override { return "quickerSnes9x"; }


  virtual void advanceStateImpl(const Controller::port_t controller1, const Controller::port_t controller2)
  {
    MovieSetJoypad(0, controller1);
    MovieSetJoypad(1, controller2);
    S9xMainLoop();
  }

};

} // namespace snes9x