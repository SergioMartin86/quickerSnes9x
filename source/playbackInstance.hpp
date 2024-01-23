#pragma once

#include "emuInstance.hpp"
#include <string>
#include <unistd.h>
#include <utils.hpp>

#define _INVERSE_FRAME_RATE 66667

struct stepData_t
{
  std::string input;
  uint8_t *stateData;
  hash_t hash;
};

class PlaybackInstance
{
  public:
  void addStep(const std::string &input)
  {
    stepData_t step;
    step.input = input;
    step.stateData = (uint8_t *)malloc(_emu->getFullStateSize());
    _emu->serializeFullState(step.stateData);
    step.hash = _emu->getStateHash();

    // Adding the step into the sequence
    _stepSequence.push_back(step);
  }

  // Initializes the playback module instance
  PlaybackInstance(EmuInstance *emu, const std::vector<std::string> &sequence, const std::string &overlayPath = "") : _emu(emu)
  {
    // Enabling emulation rendering
    _emu->enableRendering();

    // Building sequence information
    for (const auto &input : sequence)
    {
      // Adding new step
      addStep(input);

      // Advance state based on the input received
      _emu->advanceState(input);
    }

    // Adding last step with no input
    addStep("<End Of Sequence>");
  }

  // Function to render frame
  void renderFrame(const size_t stepId)
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) EXIT_WITH_ERROR("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Since we do not store the blit information (too much memory), we need to load the previous frame and re-run the input

    // If its the first step, then simply reset
    if (stepId == 0) _emu->doHardReset();

    // Else we load the previous frame
    if (stepId > 0)
    {
      const auto stateData = getStateData(stepId - 1);
      _emu->deserializeFullState(stateData);
      _emu->advanceState(getStateInput(stepId - 1));
    }

    // Updating image
     doRendering = true;
    S9xMainLoop();
     doRendering = false;
  }

  size_t getSequenceLength() const
  {
    return _stepSequence.size();
  }

  const std::string getInput(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) EXIT_WITH_ERROR("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.input;
  }

  const uint8_t *getStateData(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) EXIT_WITH_ERROR("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.stateData;
  }

  const hash_t getStateHash(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) EXIT_WITH_ERROR("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.hash;
  }

  const std::string getStateInput(const size_t stepId) const
  {
    // Checking the required step id does not exceed contents of the sequence
    if (stepId > _stepSequence.size()) EXIT_WITH_ERROR("[Error] Attempting to render a step larger than the step sequence");

    // Getting step information
    const auto &step = _stepSequence[stepId];

    // Returning step input
    return step.input;
  }

  private:
  
  // Internal sequence information
  std::vector<stepData_t> _stepSequence;

  // Pointer to the contained emulator instance
  EmuInstance *const _emu;
};
