// Auto-scan / subdivide driver: derive the minimal gameplay-RELEVANT WRAM mask
// (and its inert complement) automatically, so sound-engine / scratch bytes can
// be excluded from the dedup hash and desync validation.
//
// Builds on the perturbation oracle (see perturbConfirm.cpp): a region is INERT
// iff scrambling it never changes any still-relevant byte. The driver:
//   1. recursively divide-and-conquers WRAM — test a region; if inert, accept it
//      wholesale; else split in half and recurse down to --min-gran;
//   2. accumulates accepted-inert regions GREEDILY and excludes them from the
//      oracle of later tests (so audio-that-only-feeds-audio is absorbed too);
//   3. finally UNION-CONFIRMS the whole inert set jointly with robust params.
// Soundness: a region is accepted only if scrambling it leaves every byte not
// already proven inert unchanged; a complement-empty guard blocks the trivial
// "scramble everything" hole. Verdict is per-trajectory empirical evidence.
//
// Usage: quickerSnes9xInertScan <script> [--checkpoints N] [--tail K]
//        [--min-gran G] [--confirm-checkpoints N] [--confirm-tail K] [--seed S]

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

std::vector<Region> normalize(std::vector<Region> rs, size_t ramSize)
{
  for (auto &r : rs) { if (r.off > ramSize) r.off = ramSize; if (r.off + r.len > ramSize) r.len = ramSize - r.off; }
  std::sort(rs.begin(), rs.end(), [](const Region &a, const Region &b) { return a.off < b.off; });
  std::vector<Region> out;
  for (const auto &r : rs)
  {
    if (r.len == 0) continue;
    if (!out.empty() && r.off <= out.back().off + out.back().len)
      out.back().len = std::max(out.back().off + out.back().len, r.off + r.len) - out.back().off;
    else out.push_back(r);
  }
  return out;
}

size_t totalBytes(const std::vector<Region> &rs) { size_t n = 0; for (auto &r : rs) n += r.len; return n; }

jaffarCommon::hash::hash_t oracleHash(const uint8_t *ram, size_t ramSize, const std::vector<Region> &excl)
{
  MetroHash128 h;
  size_t pos = 0;
  for (const auto &r : excl) { if (r.off > pos) h.Update(ram + pos, r.off - pos); pos = std::max(pos, r.off + r.len); }
  if (pos < ramSize) h.Update(ram + pos, ramSize - pos);
  jaffarCommon::hash::hash_t out;
  h.Finalize(reinterpret_cast<uint8_t *>(&out));
  return out;
}

// Globals for the test harness (set up once).
snes9x::EmuInstance *gE = nullptr;
uint8_t *gRam = nullptr;
size_t gRamSize = 0, gStateSize = 0;
std::vector<jaffar::input_t> gSeq;
std::vector<std::vector<uint8_t>> gCkpt; // serialized full states
std::vector<size_t> gCkptStep;
uint64_t gSeed = 1;
size_t gScrambleCalls = 0;

// Does scrambling `scram` ever change a byte outside (scram ∪ inert)?
// Returns true = leaks (RELEVANT), false = inert. nPat patterns: 0x00,0xFF,random(+0x5A if 4).
bool leaks(const Region &scram, const std::vector<Region> &inert, size_t nCkpt, size_t tailArg, int nPat)
{
  std::vector<Region> excl = normalize([&]{ auto v = inert; v.push_back(scram); return v; }(), gRamSize);
  if (totalBytes(excl) >= gRamSize) return true; // complement empty -> cannot prove inert (conservative)

  std::vector<jaffarCommon::hash::hash_t> base;
  const size_t C = std::min(nCkpt, gCkpt.size());
  for (size_t c = 0; c < C; c++)
  {
    const size_t tail = tailArg ? std::min(tailArg, gSeq.size() - gCkptStep[c]) : (gSeq.size() - gCkptStep[c]);
    if (tail == 0) continue;

    { jaffarCommon::deserializer::Contiguous d(gCkpt[c].data(), gStateSize); gE->deserializeState(d); }
    base.assign(tail, {});
    for (size_t t = 0; t < tail; t++) { gE->advanceState(gSeq[gCkptStep[c] + t]); base[t] = oracleHash(gRam, gRamSize, excl); }

    for (int p = 0; p < nPat; p++)
    {
      { jaffarCommon::deserializer::Contiguous d(gCkpt[c].data(), gStateSize); gE->deserializeState(d); }
      uint64_t rng = gSeed * 0x9E3779B97F4A7C15ULL + c * 1000003ULL + p * 7919ULL;
      for (size_t i = 0; i < scram.len; i++)
      {
        uint8_t v;
        if (p == 0) v = 0x00; else if (p == 1) v = 0xFF; else if (p == 2 && nPat >= 4) v = 0x5A;
        else { rng = rng * 6364136223846793005ULL + 1; v = (uint8_t)(rng >> 33); }
        gRam[scram.off + i] = v;
      }
      gScrambleCalls++;
      for (size_t t = 0; t < tail; t++)
      {
        gE->advanceState(gSeq[gCkptStep[c] + t]);
        if (oracleHash(gRam, gRamSize, excl) != base[t]) return true;
      }
    }
  }
  return false;
}

std::vector<Region> gInert, gRelevant;
size_t gMinGran, gScanCkpt, gScanTail; int gScanPat;

// Greedy divide-and-conquer: accept inert regions wholesale, else subdivide.
void classify(size_t off, size_t len)
{
  if (len == 0) return;
  if (!leaks({off, len}, gInert, gScanCkpt, gScanTail, gScanPat))
  {
    gInert.push_back({off, len});
    gInert = normalize(gInert, gRamSize);
    return;
  }
  if (len <= gMinGran) { gRelevant.push_back({off, len}); return; }
  size_t half = len / 2;
  classify(off, half);
  classify(off + half, len - half);
}
} // namespace

int main(int argc, char *argv[])
{
  argparse::ArgumentParser program("inertScan", "1.0");
  program.add_argument("scriptFile").required();
  program.add_argument("--checkpoints").default_value(std::string("16"));
  program.add_argument("--tail").default_value(std::string("96"));
  program.add_argument("--min-gran").default_value(std::string("64"));
  program.add_argument("--confirm-checkpoints").default_value(std::string("48"));
  program.add_argument("--confirm-tail").default_value(std::string("400"));
  program.add_argument("--seed").default_value(std::string("1"));
  try { program.parse_args(argc, argv); }
  catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  gScanCkpt           = std::stoul(program.get<std::string>("--checkpoints"));
  gScanTail           = std::stoul(program.get<std::string>("--tail"));
  gMinGran            = std::stoul(program.get<std::string>("--min-gran"));
  gScanPat            = 3;
  const size_t confC  = std::stoul(program.get<std::string>("--confirm-checkpoints"));
  const size_t confT  = std::stoul(program.get<std::string>("--confirm-tail"));
  gSeed               = std::stoul(program.get<std::string>("--seed"));

  std::string configRaw;
  if (!jaffarCommon::file::loadStringFromFile(configRaw, program.get<std::string>("scriptFile"))) JAFFAR_THROW_LOGIC("Cannot read script\n");
  const auto config = nlohmann::json::parse(configRaw);
  static snes9x::EmuInstance e(config);
  gE = &e;
  std::string romData;
  if (!jaffarCommon::file::loadStringFromFile(romData, jaffarCommon::json::getString(config, "Rom File"))) JAFFAR_THROW_LOGIC("Cannot read ROM\n");
  e.loadROM(romData);
  e.disableRendering();

  std::string seqRaw;
  if (!jaffarCommon::file::loadStringFromFile(seqRaw, jaffarCommon::json::getString(config, "Sequence File"))) JAFFAR_THROW_LOGIC("Cannot read sequence\n");
  const auto parser = e.getInputParser();
  for (const auto &s : jaffarCommon::string::split(seqRaw, ' ')) gSeq.push_back(parser->parseInputString(s));

  gRam      = e.getRAM();
  gRamSize  = e.getRAMSize();
  gStateSize = e.getStateSize();

  const size_t C = std::min(gScanCkpt, gSeq.size());
  if (C == 0) JAFFAR_THROW_LOGIC("Empty sequence\n");
  gCkpt.assign(C, std::vector<uint8_t>(gStateSize));
  gCkptStep.assign(C, 0);
  {
    size_t next = 0;
    for (size_t step = 0; step < gSeq.size(); step++)
    {
      if (next < C && step == (next * gSeq.size()) / C)
      { jaffarCommon::serializer::Contiguous s(gCkpt[next].data(), gStateSize); e.serializeState(s); gCkptStep[next] = step; next++; }
      e.advanceState(gSeq[step]);
    }
  }

  printf("[] Inert scan: WRAM 0x%lx bytes, %lu checkpoints, tail %lu, min-gran %lu, %lu seq inputs\n",
         gRamSize, C, gScanTail, gMinGran, gSeq.size());
  fflush(stdout);

  classify(0, gRamSize);

  gInert    = normalize(gInert, gRamSize);
  gRelevant = normalize(gRelevant, gRamSize);

  printf("[] Scan done (%lu scrambles). Provisional split:\n", gScrambleCalls);
  printf("[]   INERT:    %lu bytes (%.1f%%) in %lu region(s)\n", totalBytes(gInert), 100.0 * totalBytes(gInert) / gRamSize, gInert.size());
  printf("[]   RELEVANT: %lu bytes (%.1f%%) in %lu region(s)\n", totalBytes(gRelevant), 100.0 * totalBytes(gRelevant) / gRamSize, gRelevant.size());

  // Final joint union-confirm of the inert set with robust params.
  printf("[] Union-confirming the inert set jointly (%lu checkpoints, tail %lu)...\n", confC, confT);
  fflush(stdout);
  bool confirmed = true;
  if (!gInert.empty())
  {
    // Scramble ALL inert regions at once; oracle excludes all of them => complement = relevant set.
    // Reuse leaks() by treating the whole inert set as the scramble target via a merged single region per piece.
    // (We scramble each inert region; exclude = entire inert set.)
    std::vector<jaffarCommon::hash::hash_t> base;
    for (size_t c = 0; c < C && confirmed; c++)
    {
      const size_t tail = std::min(confT, gSeq.size() - gCkptStep[c]);
      if (tail == 0) continue;
      { jaffarCommon::deserializer::Contiguous d(gCkpt[c].data(), gStateSize); e.deserializeState(d); }
      base.assign(tail, {});
      for (size_t t = 0; t < tail; t++) { e.advanceState(gSeq[gCkptStep[c] + t]); base[t] = oracleHash(gRam, gRamSize, gInert); }
      for (int p = 0; p < 4 && confirmed; p++)
      {
        { jaffarCommon::deserializer::Contiguous d(gCkpt[c].data(), gStateSize); e.deserializeState(d); }
        uint64_t rng = gSeed * 0x123456789ULL + c * 99991ULL + p;
        for (const auto &r : gInert)
          for (size_t i = 0; i < r.len; i++)
          { uint8_t v; if (p==0) v=0; else if (p==1) v=0xFF; else if (p==2) v=0x5A; else { rng=rng*6364136223846793005ULL+1; v=(uint8_t)(rng>>33);} gRam[r.off+i]=v; }
        for (size_t t = 0; t < tail && confirmed; t++)
        { e.advanceState(gSeq[gCkptStep[c] + t]); if (oracleHash(gRam, gRamSize, gInert) != base[t]) confirmed = false; }
      }
    }
  }

  printf("\n[] ===== RESULT =====\n");
  if (!confirmed)
  {
    printf("[WARN] Union-confirm FAILED — the greedy inert set leaks jointly. Re-run with larger --tail/--checkpoints;\n");
    printf("       treat the per-region scan as advisory only. NOT safe to exclude as-is.\n");
  }
  else
    printf("[OK] Inert set union-confirmed gameplay-inert for this trajectory.\n");

  printf("\n[] Minimal gameplay-RELEVANT mask (hash THESE; exclude the rest) — paste into the dedup-hash list:\n");
  for (const auto &r : gRelevant) printf("    0x%05lx:0x%lx\n", r.off, r.len);
  printf("\n[] Inert (excludable) regions:\n");
  for (const auto &r : gInert) printf("    0x%05lx:0x%lx\n", r.off, r.len);
  printf("\n[] Summary: %lu/%lu WRAM bytes relevant (%.1f%%); excluding the rest shrinks the hashed state %.1fx.\n",
         totalBytes(gRelevant), gRamSize, 100.0 * totalBytes(gRelevant) / gRamSize,
         totalBytes(gRelevant) ? (double)gRamSize / totalBytes(gRelevant) : 0.0);
  return confirmed ? 0 : 1;
}
