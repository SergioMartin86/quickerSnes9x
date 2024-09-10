#pragma once

#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>
#include <jaffarCommon/file.hpp>
#include <jaffarCommon/serializers/base.hpp>
#include <jaffarCommon/deserializers/base.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include "inputParser.hpp"

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

  EmuInstanceBase(const nlohmann::json &config)
  {
    _inputParser = std::make_unique<jaffar::InputParser>(config);
  }

  virtual ~EmuInstanceBase() = default;

  inline void advanceState(const jaffar::input_t &input)
  {
    if (input.power) JAFFAR_THROW_RUNTIME("Power button pressed, but not supported");
    if (input.reset == true) doSoftReset();

    advanceStateImpl(input.port1, input.port2);
  }

  inline jaffarCommon::hash::hash_t getStateHash() const
  {
    MetroHash128 hash;
    
    hash.Update(Memory.RAM, 0x20000);

    jaffarCommon::hash::hash_t result;
    hash.Finalize(reinterpret_cast<uint8_t *>(&result));
    return result;
  }

  inline jaffar::InputParser *getInputParser() const { return _inputParser.get(); }

  virtual void initializeVideoOutput() = 0;
  virtual void finalizeVideoOutput() = 0;
  virtual void enableRendering() = 0;
  virtual void disableRendering() = 0;

  inline void loadROM(const std::string &romData)
  {
    // Actually loading rom file
    auto status = loadROMImpl(romData);
    if (status == false) JAFFAR_THROW_RUNTIME("Could not process ROM file");

    _stateSize = getStateSizeImpl();
    _differentialStateSize = getDifferentialStateSizeImpl();
  }

  void enableStateBlock(const std::string& block) 
  {
    enableStateBlockImpl(block);
    _stateSize = getStateSizeImpl();
    _differentialStateSize = getDifferentialStateSizeImpl();
  }

  void disableStateBlock(const std::string& block)
  {
     disableStateBlockImpl(block);
    _stateSize = getStateSizeImpl();
    _differentialStateSize = getDifferentialStateSizeImpl();
  }

  inline size_t getStateSize() const 
  {
    return _stateSize;
  }

  inline size_t getDifferentialStateSize() const
  {
    return _differentialStateSize;
  }

  // Virtual functions

  virtual void updateRenderer() = 0;
  virtual void serializeState(jaffarCommon::serializer::Base& s) const = 0;
  virtual void deserializeState(jaffarCommon::deserializer::Base& d) = 0;

  virtual void doSoftReset() = 0;
  virtual void doHardReset() = 0;
  virtual std::string getCoreName() const = 0;

  protected:

  virtual bool loadROMImpl(const std::string &romData) = 0;
  virtual void advanceStateImpl(const jaffar::port_t controller1, const jaffar::port_t controller2) = 0;

  virtual void enableStateBlockImpl(const std::string& block) {};
  virtual void disableStateBlockImpl(const std::string& block) {};

  virtual size_t getStateSizeImpl() const = 0;
  virtual size_t getDifferentialStateSizeImpl() const = 0;

  // State size
  size_t _stateSize;

  private:

  // Input parser instance
  std::unique_ptr<jaffar::InputParser> _inputParser;

  // Differential state size
  size_t _differentialStateSize;
};

} // namespace snes9x