#pragma once

#include "../snes9xInstanceBase.hpp"
#include <string>
#include <vector>
#include "unix.hpp"
#include "snes/snes.hpp"
#include <lightStateConfig.h>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>

extern thread_local bool doRendering;

namespace snes9x
{
  
class EmuInstance : public EmuInstanceBase
{
 public:

 uint8_t* _baseMem;
 uint8_t* _apuMem;

 EmuInstance(const nlohmann::json &config) : EmuInstanceBase(config)
 {
  int argc = 1;
  char bin[] = "./snes9x";
  char* argv[1] = { bin };

  doRendering = false;
  initSnes9x(argc, argv);
 }

  virtual bool loadROMImpl(const std::string &romData) override
  {
    uint32 saved_flags = CPU.Flags;

    Memory.LoadROMMem ((const uint8*) romData.data(), romData.size());

    CPU.Flags = saved_flags;
    Settings.StopEmulation = FALSE;

    _baseMem = Memory.RAM;
    _apuMem = SNES::smp.apuram;
    
    return true;
  }

  void initializeVideoOutput() override
  {
    S9xInitInputDevices();
    S9xInitDisplay(0, NULL);
    S9xSetupDefaultKeymap();
    S9xTextMode();
    S9xGraphicsMode();
    S9xSetTitle(String);
    doRendering = true;
  }

  void finalizeVideoOutput() override
  {
    S9xDeinitDisplay();
  }

  void enableRendering() override
  {
  }

  void disableRendering() override
  {
  }

  void serializeState(jaffarCommon::serializer::Base& s) const override
  {
    S9xFreezeToStream(s, _optionalBlocks);
  }

  void deserializeState(jaffarCommon::deserializer::Base& d) override
  {
    S9xUnfreezeFromStream(d,_optionalBlocks);
  }

  void updateRenderer() override
  {
     S9xStartScreenRefresh();
     S9xUpdateScreen();
     S9xEndScreenRefresh();
  }
  
  size_t getStateSizeImpl() const override
  {
    jaffarCommon::serializer::Contiguous s;
    S9xFreezeToStream(s, _optionalBlocks);
    return s.getOutputSize();
  }

  inline size_t getDifferentialStateSizeImpl() const override { return 0; }

  inline void setStateRAMSize(const size_t stateRAMSize) { _optionalBlocks._stateRAMSize = stateRAMSize; }

  void enableStateBlockImpl(const std::string& block)
  { 
    bool recognizedBlock = false;
    
    if (block == "PPU") { _optionalBlocks._enablePPUBlock = true; recognizedBlock = true; }
    if (block == "DMA") { _optionalBlocks._enableDMABlock = true; recognizedBlock = true; }
    if (block == "VRA") { _optionalBlocks._enableVRABlock = true; recognizedBlock = true; }
    if (block == "RAM") { _optionalBlocks._enableRAMBlock = true; recognizedBlock = true; }
    if (block == "SRA") { _optionalBlocks._enableSRABlock = true; recognizedBlock = true; }
    if (block == "FIL") { _optionalBlocks._enableFILBlock = true; recognizedBlock = true; }
    if (block == "SND") { _optionalBlocks._enableSNDBlock = true; recognizedBlock = true; }
    if (block == "CTL") { _optionalBlocks._enableCTLBlock = true; recognizedBlock = true; }
    if (block == "TIM") { _optionalBlocks._enableTIMBlock = true; recognizedBlock = true; }

    if (recognizedBlock == false) { fprintf(stderr, "Unrecognized block type: %s\n", block.c_str()); exit(-1);}
  };


  void disableStateBlockImpl(const std::string& block)
  { 
    bool recognizedBlock = false;
    
    if (block == "PPU") { _optionalBlocks._enablePPUBlock = false; recognizedBlock = true; }
    if (block == "DMA") { _optionalBlocks._enableDMABlock = false; recognizedBlock = true; }
    if (block == "VRA") { _optionalBlocks._enableVRABlock = false; recognizedBlock = true; }
    if (block == "RAM") { _optionalBlocks._enableRAMBlock = false; recognizedBlock = true; }
    if (block == "SRA") { _optionalBlocks._enableSRABlock = false; recognizedBlock = true; }
    if (block == "FIL") { _optionalBlocks._enableFILBlock = false; recognizedBlock = true; }
    if (block == "SND") { _optionalBlocks._enableSNDBlock = false; recognizedBlock = true; }
    if (block == "CTL") { _optionalBlocks._enableCTLBlock = false; recognizedBlock = true; }
    if (block == "TIM") { _optionalBlocks._enableTIMBlock = false; recognizedBlock = true; }

    if (recognizedBlock == false) { fprintf(stderr, "Unrecognized block type: %s\n", block.c_str()); exit(-1);}
  };

  uint8_t* getRAM() const
  {
    return (uint8_t*)Memory.RAM;
  }

  size_t getRAMSize() const
  {
    return 0x20000;
  }

  uint8_t* getSRAM() const
  {
    return (uint8_t*)Memory.SRAM;
  }

  size_t getSRAMSize() const
  {
    return 0x20000;
  }

  void doSoftReset() override
  {
    S9xSoftReset();
  }
  
  void doHardReset() override
  {
    S9xReset();
  }

  std::string getCoreName() const override { return "quickerSnes9x"; }


  void advanceStateImpl(const jaffar::port_t controller1, const jaffar::port_t controller2) override
  {
    MovieSetJoypad(0, controller1);
    MovieSetJoypad(1, controller2);
    S9xMainLoop();
  }

  private:

  optionalBlocks_t _optionalBlocks;
};

} // namespace snes9x