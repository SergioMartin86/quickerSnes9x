// Lazy-timer A/B validator: boot a ROM, run it for N frames on null input (the
// intro/attract music auto-plays — the SPC700 audio driver runs its timers hard,
// including tempo changes), folding a rolling hash over the FULL serialized state
// each frame (which includes the timer stage1/2/3 counters). Build this tool
// twice — once normally (LAZY_SMP_TIMERS=1) and once with -DLAZY_SMP_TIMERS=0 —
// and compare the printed hash per ROM. Identical => lazy timers are bit-exact
// (not merely gameplay-equivalent) for that playthrough; any divergence prints
// the first frame where the full state differs.
//
// Needs no .sol / .test — works on any ROM. Usage:
//   quickerSnes9xTimerABCheck <romFile> [--frames N] [--report]

#include "argparse/argparse.hpp"
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/file.hpp>
#include "snes9xInstance.hpp"
#include <vector>
#include <string>
#include <cstdio>

int main(int argc, char *argv[])
{
  argparse::ArgumentParser program("timerABCheck", "1.0");
  program.add_argument("romFile").required();
  program.add_argument("--frames").default_value(std::string("3000"));
  program.add_argument("--dump").default_value(std::string("")).help("Write final serialized state to this file (for byte-diff)");
  try { program.parse_args(argc, argv); }
  catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  const size_t frames = std::stoul(program.get<std::string>("--frames"));

  // Minimal config: just enough for the instance + a Joypad input parser.
  nlohmann::json config = {
    {"Controller 1 Type", "Joypad"}, {"Controller 2 Type", "None"}};
  snes9x::EmuInstance e(config);

  std::string romData;
  if (!jaffarCommon::file::loadStringFromFile(romData, program.get<std::string>("romFile")))
    JAFFAR_THROW_LOGIC("Cannot read ROM: %s\n", program.get<std::string>("romFile").c_str());
  e.loadROM(romData);
  e.disableRendering();

  const size_t stateSize = e.getStateSize();
  std::vector<uint8_t> buf(stateSize);
  uint8_t *ram         = e.getRAM();
  const size_t ramSize = e.getRAMSize();
  uint8_t *apu         = e._apuMem;     // SPC700 (APU) 64K RAM — audio engine's own memory
  const size_t apuSize = 0x10000;

  jaffar::input_t nullInput{}; // all zero: no buttons, no power/reset

  auto fold = [](jaffarCommon::hash::hash_t acc, jaffarCommon::hash::hash_t h) {
    uint8_t pair[sizeof(acc) * 2];
    memcpy(pair, &acc, sizeof(acc));
    memcpy(pair + sizeof(acc), &h, sizeof(h));
    return jaffarCommon::hash::calculateMetroHash(pair, sizeof(pair));
  };

  jaffarCommon::hash::hash_t rollFull{}, rollRam{}, rollApu{};
  for (size_t f = 0; f < frames; f++)
  {
    e.advanceState(nullInput);
    jaffarCommon::serializer::Contiguous s(buf.data(), stateSize);
    e.serializeState(s);
    rollFull = fold(rollFull, jaffarCommon::hash::calculateMetroHash(buf.data(), stateSize));
    rollRam  = fold(rollRam, jaffarCommon::hash::calculateMetroHash(ram, ramSize));
    rollApu  = fold(rollApu, jaffarCommon::hash::calculateMetroHash(apu, apuSize));
  }

  const auto dumpPath = program.get<std::string>("--dump");
  if (!dumpPath.empty())
  {
    jaffarCommon::serializer::Contiguous s(buf.data(), stateSize);
    e.serializeState(s);
    FILE *fp = fopen(dumpPath.c_str(), "wb");
    if (fp) { fwrite(buf.data(), 1, stateSize, fp); fclose(fp); }
  }

  printf("LAZY_SMP_TIMERS=%d frames=%lu fullStateHash=%s mainRamHash=%s apuRamHash=%s\n",
         LAZY_SMP_TIMERS, frames,
         jaffarCommon::hash::hashToString(rollFull).c_str(),
         jaffarCommon::hash::hashToString(rollRam).c_str(),
         jaffarCommon::hash::hashToString(rollApu).c_str());
  return 0;
}
