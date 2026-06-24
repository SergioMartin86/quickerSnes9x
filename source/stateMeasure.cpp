// Save-state size analysis for the quickerSnes9x core.
//
// Reports where the state bytes actually go and how much is reducible:
//   * live WRAM extent  — the highest WRAM offset that ever changes during the
//     run (bytes above it are static for this sequence => candidate RAM trim),
//   * full vs differential state size — the differential size is what a search
//     DB actually pays per stored state.
//
// Usage: quickerSnes9xStateMeasure <script>

#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/serializers/differential.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/file.hpp>
#include "snes9xInstance.hpp"
#include <vector>
#include <string>
#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{
  argparse::ArgumentParser program("stateMeasure", "1.0");
  program.add_argument("scriptFile").help("Path to the test script file.").required();
  try { program.parse_args(argc, argv); }
  catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  std::string configRaw;
  if (!jaffarCommon::file::loadStringFromFile(configRaw, program.get<std::string>("scriptFile"))) JAFFAR_THROW_LOGIC("Cannot read script\n");
  const auto config = nlohmann::json::parse(configRaw);

  snes9x::EmuInstance e(config);
  std::string romData;
  if (!jaffarCommon::file::loadStringFromFile(romData, jaffarCommon::json::getString(config, "Rom File"))) JAFFAR_THROW_LOGIC("Cannot read ROM\n");
  e.loadROM(romData);
  for (const auto &b : jaffarCommon::json::getArray<std::string>(config, "Disable State Blocks")) e.disableStateBlock(b);
  e.disableRendering();

  std::string seqRaw;
  if (!jaffarCommon::file::loadStringFromFile(seqRaw, jaffarCommon::json::getString(config, "Sequence File"))) JAFFAR_THROW_LOGIC("Cannot read sequence\n");
  const auto parser = e.getInputParser();
  std::vector<jaffar::input_t> seq;
  for (const auto &s : jaffarCommon::string::split(seqRaw, ' ')) seq.push_back(parser->parseInputString(s));

  uint8_t *ram = e.getRAM();
  const size_t ramSize = e.getRAMSize();
  uint8_t *apu = e._apuMem;          // SPC700 (APU) 64K RAM
  const size_t apuSize = 0x10000;

  std::vector<uint8_t> ramInit(ram, ram + ramSize);
  std::vector<uint8_t> apuInit(apu, apu + apuSize);
  size_t ramLive = 0, apuLive = 0;   // one past the highest changed byte

  const size_t stateSize = e.getStateSize();
  std::vector<uint8_t> prev(stateSize);
  { jaffarCommon::serializer::Contiguous s(prev.data(), stateSize); e.serializeState(s); }

  std::vector<uint8_t> diffBuf(stateSize * 2 + 1024);
  std::vector<uint8_t> cur(stateSize);
  size_t maxStepDiff = 0;            // largest state[i] vs state[i-1] differential

  for (const auto &input : seq)
  {
    e.advanceState(input);
    for (size_t i = ramSize; i > ramLive; i--) if (ram[i - 1] != ramInit[i - 1]) { ramLive = i; break; }
    for (size_t i = apuSize; i > apuLive; i--) if (apu[i - 1] != apuInit[i - 1]) { apuLive = i; break; }

    // step-to-step differential = what a search DB actually stores per new state
    auto s = jaffarCommon::serializer::Differential(diffBuf.data(), diffBuf.size(), prev.data(), stateSize, /*zlib*/ true);
    e.serializeState(s);
    if (s.getOutputSize() > maxStepDiff) maxStepDiff = s.getOutputSize();
    { jaffarCommon::serializer::Contiguous cs(cur.data(), stateSize); e.serializeState(cs); }
    std::swap(prev, cur);
  }

  printf("[] ----- State size analysis -----\n");
  printf("[] Sequence length:          %lu inputs\n", seq.size());
  printf("[] Full state size:          %lu bytes\n", stateSize);
  printf("[] WRAM:                     %lu / %lu bytes live (%.1f%% static tail)\n",
         ramLive, ramSize, 100.0 * (double)(ramSize - ramLive) / (double)ramSize);
  printf("[] APU RAM:                  %lu / %lu bytes live (%.1f%% static tail)\n",
         apuLive, apuSize, 100.0 * (double)(apuSize - apuLive) / (double)apuSize);
  printf("[] Max step-to-step diff:    %lu bytes (true per-state search-DB footprint)\n", maxStepDiff);
  printf("[] Diff compression ratio:   %.0fx vs full state\n", (double)stateSize / (double)(maxStepDiff ? maxStepDiff : 1));
  return 0;
}
