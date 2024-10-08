#pragma once

#include "snes9xInstance.hpp"
#include <string>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/exceptions.hpp>

#define _INVERSE_FRAME_RATE 66667

struct stepData_t
{
  jaffar::input_t inputData;
  std::string inputString;
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
    _fullStateSize = _emu->getStateSize();  

    // Getting input decoder
    auto inputParser = _emu->getInputParser();

    // Building sequence information
    for (size_t i = 0; i < sequence.size(); i++)
    {
    // Adding new step
      stepData_t step;
      step.inputString = sequence[i];
      step.inputData = inputParser->parseInputString(step.inputString);

      // Serializing state
      uint8_t stateData[_fullStateSize];
      jaffarCommon::serializer::Contiguous s(stateData, _fullStateSize);
      _emu->serializeState(s);
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
      _emu->advanceState(step.inputData);
    }

    // Adding last step with no input
    stepData_t step;
    step.inputString = "<End Of Sequence>";
    step.inputData = _stepSequence.rbegin()->inputData;
    step.stateData = (uint8_t *)malloc(_fullStateSize);
    jaffarCommon::serializer::Contiguous s(step.stateData, _fullStateSize);

    _emu->serializeState(s);
    step.hash = _emu->getStateHash();

    // Adding the step into the sequence
    _stepSequence.push_back(step);
  }

  // Function to render frame
  void renderFrame(const size_t stepId)
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Else we load the requested step
    const auto stateData = getStateData(stepId);
    jaffarCommon::deserializer::Contiguous d(stateData, _fullStateSize);
    _emu->deserializeState(d);

    // Updating image
    _emu->updateRenderer();
  }

  size_t getSequenceLength() const
  {
    return _stepSequence.size();
  }

  const std::string getInputString(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.inputString;
  }

  const jaffar::input_t getInputData(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) JAFFAR_THROW_RUNTIME("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.inputData;
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
