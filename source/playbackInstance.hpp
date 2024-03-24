#pragma once

#include "snes9xInstance.hpp"
#include <string>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>

#define _INVERSE_FRAME_RATE 66667

struct stepData_t
{
  std::string input;
  uint8_t *stateData;
  jaffarCommon::hash::hash_t hash;
};

class PlaybackInstance
{
  public:

  // Initializes the playback module instance
  PlaybackInstance(snes9x::EmuInstance *emu, const std::vector<std::string> &sequence, const size_t storeInterval = 1) :
   _emu(emu),
   _storeInterval(storeInterval)
  {
    // Getting full state size
    _fullStateSize = _emu->getFullStateSize();  

    // Building sequence information
    for (size_t i = 0; i < sequence.size(); i++)
    {
      // Adding new step
      stepData_t step;
      step.input = sequence[i];

      // Serializing state
      uint8_t stateData[_fullStateSize];
      jaffarCommon::serializer::Contiguous s(stateData, _fullStateSize);
      _emu->serializeFullState(s);
      step.hash = _emu->getStateHash();

      // Only save data if within store interval
      if (i % storeInterval == 0)
      {
        step.stateData = (uint8_t *)malloc(_fullStateSize);
        memcpy(step.stateData, stateData, _fullStateSize);
      }

      // Adding the step into the sequence
      _stepSequence.push_back(step);

      // Advance state based on the input received
      _emu->advanceState(step.input);
    }

    // Adding last step with no input
    stepData_t step;
    step.input = "<End Of Sequence>";
    step.stateData = (uint8_t *)malloc(_fullStateSize);
    jaffarCommon::serializer::Contiguous s(step.stateData, _fullStateSize);

    _emu->serializeFullState(s);
    step.hash = _emu->getStateHash();

    // Adding the step into the sequence
    _stepSequence.push_back(step);
  }

  // Function to render frame
  void renderFrame(const size_t stepId)
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting closer step interval to the one requested
    size_t newStepId = stepId;
    if (stepId != _stepSequence.size()-1) newStepId = (stepId / _storeInterval) * _storeInterval;

    // If its the first step, then simply reset
    if (newStepId == 0) _emu->doHardReset();

    // Else we load the requested step
    const auto stateData = getStateData(newStepId);
    jaffarCommon::deserializer::Contiguous d(stateData, _fullStateSize);
    _emu->deserializeFullState(d);

    // Updating image
     doRendering = true;
    _emu->advanceState(getStateInput(newStepId == 0 ? 0 : newStepId-1));
     doRendering = false;
  }

  size_t getSequenceLength() const
  {
    return _stepSequence.size();
  }

  const std::string getInput(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.input;
  }

  const uint8_t *getStateData(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.stateData;
  }

  const jaffarCommon::hash::hash_t getStateHash(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.hash;
  }

  const std::string getStateInput(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.input;
  }

  private:
  
  // Internal sequence information
  std::vector<stepData_t> _stepSequence;

  // Pointer to the contained emulator instance
  snes9x::EmuInstance *const _emu;

  // State storage interval
  const size_t _storeInterval;

  // Full size of the game state
  size_t _fullStateSize;
};
