// Perturbation-confirmer: decide whether a WRAM region is gameplay-INERT.
//
// Motivation: some WRAM bytes are written by the game's sound engine (music
// counters, SFX queues, APU handshake mirrors). They vary between otherwise
// gameplay-identical states, bloating the dedup frontier and triggering false
// "desync" flags — yet they never influence gameplay. This tool proves (or
// refutes) that for a candidate region, so it can be safely excluded from the
// dedup hash / validation.
//
// Oracle (sound & conservative): a region R is INERT iff scrambling R at a
// state and then replaying never causes ANY byte OUTSIDE R (minus regions you
// already know are audio, via --ignore) to differ from the unperturbed
// continuation. This catches direct leaks AND audio round-trips (R -> APU ->
// port read -> CPU stores outside R). If even one external byte diverges for any
// scramble at any checkpoint, R is reported RELEVANT with evidence.
//
// Usage:
//   quickerSnes9xPerturbConfirm <script> --region off:len [--region ...]
//        [--ignore off:len ...] [--checkpoints N] [--tail K] [--seed S]
//   offsets/lengths are WRAM-relative; accept decimal or 0x-hex.

#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/file.hpp>
#include "snes9xInstance.hpp"
#include <algorithm>
#include <vector>
#include <string>
#include <cstdio>
#include <cstdint>

namespace
{
struct Region { size_t off, len; };

size_t parseNum(const std::string &s)
{
  return (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) ? std::stoul(s, nullptr, 16) : std::stoul(s);
}

Region parseRegion(const std::string &s)
{
  auto colon = s.find(':');
  if (colon == std::string::npos) JAFFAR_THROW_LOGIC("Region '%s' must be off:len\n", s.c_str());
  return { parseNum(s.substr(0, colon)), parseNum(s.substr(colon + 1)) };
}

// Merge overlapping/adjacent regions (sorted) and clamp to [0, ramSize).
std::vector<Region> normalize(std::vector<Region> rs, size_t ramSize)
{
  for (auto &r : rs) { if (r.off > ramSize) r.off = ramSize; if (r.off + r.len > ramSize) r.len = ramSize - r.off; }
  std::sort(rs.begin(), rs.end(), [](const Region &a, const Region &b) { return a.off < b.off; });
  std::vector<Region> out;
  for (const auto &r : rs)
  {
    if (!out.empty() && r.off <= out.back().off + out.back().len)
      out.back().len = std::max(out.back().off + out.back().len, r.off + r.len) - out.back().off;
    else out.push_back(r);
  }
  return out;
}

// Hash of WRAM with the (sorted, merged) excluded regions removed.
jaffarCommon::hash::hash_t oracleHash(const uint8_t *ram, size_t ramSize, const std::vector<Region> &excl)
{
  MetroHash128 h;
  size_t pos = 0;
  for (const auto &r : excl)
  {
    if (r.off > pos) h.Update(ram + pos, r.off - pos);
    pos = std::max(pos, r.off + r.len);
  }
  if (pos < ramSize) h.Update(ram + pos, ramSize - pos);
  jaffarCommon::hash::hash_t out;
  h.Finalize(reinterpret_cast<uint8_t *>(&out));
  return out;
}
} // namespace

int main(int argc, char *argv[])
{
  argparse::ArgumentParser program("perturbConfirm", "1.0");
  program.add_argument("scriptFile").required();
  program.add_argument("--region").default_value(std::string("")).help("Candidate WRAM region(s) off:len, comma-separated (scrambled + excluded from oracle).");
  program.add_argument("--ignore").default_value(std::string("")).help("Known-audio WRAM region(s) off:len, comma-separated (excluded from oracle, not scrambled).");
  program.add_argument("--checkpoints").default_value(std::string("32"));
  program.add_argument("--tail").default_value(std::string("160")); // steps replayed after each scramble (0 = to end)
  program.add_argument("--seed").default_value(std::string("1"));
  try { program.parse_args(argc, argv); }
  catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  const size_t numCkpt = std::stoul(program.get<std::string>("--checkpoints"));
  const size_t tailArg = std::stoul(program.get<std::string>("--tail"));
  const uint64_t seed  = std::stoul(program.get<std::string>("--seed"));

  std::vector<Region> candidate, ignore;
  for (const auto &s : jaffarCommon::string::split(program.get<std::string>("--region"), ',')) if (!s.empty()) candidate.push_back(parseRegion(s));
  for (const auto &s : jaffarCommon::string::split(program.get<std::string>("--ignore"), ',')) if (!s.empty()) ignore.push_back(parseRegion(s));
  if (candidate.empty()) JAFFAR_THROW_LOGIC("At least one --region is required (--region off:len[,off:len...])\n");

  // Build instance (FULL state, rendering off) and decode the reference sequence.
  std::string configRaw;
  if (!jaffarCommon::file::loadStringFromFile(configRaw, program.get<std::string>("scriptFile"))) JAFFAR_THROW_LOGIC("Cannot read script\n");
  const auto config = nlohmann::json::parse(configRaw);
  snes9x::EmuInstance e(config);
  std::string romData;
  if (!jaffarCommon::file::loadStringFromFile(romData, jaffarCommon::json::getString(config, "Rom File"))) JAFFAR_THROW_LOGIC("Cannot read ROM\n");
  e.loadROM(romData);
  e.disableRendering();

  std::string seqRaw;
  if (!jaffarCommon::file::loadStringFromFile(seqRaw, jaffarCommon::json::getString(config, "Sequence File"))) JAFFAR_THROW_LOGIC("Cannot read sequence\n");
  const auto parser = e.getInputParser();
  std::vector<jaffar::input_t> seq;
  for (const auto &s : jaffarCommon::string::split(seqRaw, ' ')) seq.push_back(parser->parseInputString(s));

  uint8_t *ram         = e.getRAM();
  const size_t ramSize = e.getRAMSize();
  candidate            = normalize(candidate, ramSize);
  std::vector<Region> exclOracle = normalize([&]{ auto v = candidate; v.insert(v.end(), ignore.begin(), ignore.end()); return v; }(), ramSize);

  size_t candBytes = 0; for (auto &r : candidate) candBytes += r.len;
  const size_t stateSize = e.getStateSize();

  // Snapshot full state at evenly-spaced checkpoints along the reference run.
  const size_t C = std::min(numCkpt, seq.size());
  if (C == 0) JAFFAR_THROW_LOGIC("Empty sequence\n");
  std::vector<std::vector<uint8_t>> ckpt(C, std::vector<uint8_t>(stateSize));
  std::vector<size_t> ckptStep(C);
  {
    size_t next = 0;
    for (size_t step = 0; step < seq.size(); step++)
    {
      if (next < C && step == (next * seq.size()) / C)
      {
        jaffarCommon::serializer::Contiguous s(ckpt[next].data(), stateSize);
        e.serializeState(s);
        ckptStep[next] = step;
        next++;
      }
      e.advanceState(seq[step]);
    }
  }

  printf("[] Perturbation confirm: %lu candidate byte(s) over %lu checkpoints, tail %lu, %lu seq inputs\n",
         candBytes, C, tailArg, seq.size());
  printf("[] Candidate region(s) scrambled + excluded from the gameplay oracle:\n");
  for (auto &r : candidate) printf("[]   WRAM[0x%05lx .. 0x%05lx)  (%lu bytes)\n", r.off, r.off + r.len, r.len);
  fflush(stdout);

  // Scramble patterns: constant fills + a seeded random fill per (checkpoint,pattern).
  const uint8_t fills[3] = {0x00, 0xFF, 0x5A};
  const int numPatterns  = 4; // fills[0..2] + one random

  std::vector<uint8_t> work(stateSize);
  std::vector<jaffarCommon::hash::hash_t> baseOracle; // unperturbed tail oracle per step

  bool relevant = false;
  size_t scramblesRun = 0;

  for (size_t c = 0; c < C && !relevant; c++)
  {
    const size_t tail = tailArg ? std::min(tailArg, seq.size() - ckptStep[c]) : (seq.size() - ckptStep[c]);
    if (tail == 0) continue;

    // Unperturbed baseline oracle for this checkpoint's tail.
    { jaffarCommon::deserializer::Contiguous d(ckpt[c].data(), stateSize); e.deserializeState(d); }
    baseOracle.assign(tail, {});
    for (size_t t = 0; t < tail; t++)
    {
      e.advanceState(seq[ckptStep[c] + t]);
      baseOracle[t] = oracleHash(ram, ramSize, exclOracle);
    }

    for (int p = 0; p < numPatterns && !relevant; p++)
    {
      // Restore + scramble candidate region(s).
      { jaffarCommon::deserializer::Contiguous d(ckpt[c].data(), stateSize); e.deserializeState(d); }
      uint64_t rng = seed * 0x9E3779B97F4A7C15ULL + c * 1000003ULL + p;
      for (const auto &r : candidate)
        for (size_t i = 0; i < r.len; i++)
        {
          uint8_t v;
          if (p < 3) v = fills[p];
          else { rng = rng * 6364136223846793005ULL + 1; v = (uint8_t)(rng >> 33); }
          ram[r.off + i] = v;
        }
      scramblesRun++;

      // Replay the tail; flag if any out-of-region byte diverges.
      for (size_t t = 0; t < tail; t++)
      {
        e.advanceState(seq[ckptStep[c] + t]);
        if (oracleHash(ram, ramSize, exclOracle) != baseOracle[t])
        {
          relevant = true;
          const char *pat = p < 3 ? (p == 0 ? "0x00" : p == 1 ? "0xFF" : "0x5A") : "random";
          printf("[FAIL] RELEVANT: scrambling the region with %s at checkpoint step %lu changed gameplay state\n",
                 pat, ckptStep[c]);
          printf("        first out-of-region divergence at tail step +%lu (abs step %lu)\n", t, ckptStep[c] + t);
          break;
        }
      }
    }
  }

  if (relevant)
  {
    printf("[FAIL] Region is NOT gameplay-inert — do NOT exclude it from the hash.\n");
    return 1;
  }
  printf("[OK] INERT: %lu scrambles across %lu checkpoints (tail up to %lu) never leaked outside the region.\n",
         scramblesRun, C, tailArg);
  printf("[OK] Safe to exclude this region from the dedup hash / desync validation (for the tested trajectory).\n");
  return 0;
}
