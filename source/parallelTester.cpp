// Parallel determinism / thread-safety stress test for the quickerSnes9x core.
//
// Spawns N worker threads, each owning an independent EmuInstance, and replays
// the same input sequence on every worker concurrently. If all mutable
// emulation state is correctly thread-local, every worker must reach a state
// whose hash equals the single-threaded reference. Any divergence proves a
// shared-state leak (a data race) between worker threads.
//
// Usage: quickerSnes9xParallelTester <script> [--threads N] [--cycleType Simple|Rerecord|Full]

#include "argparse/argparse.hpp"
#include <jaffarCommon/json.hpp>
#include <jaffarCommon/serializers/contiguous.hpp>
#include <jaffarCommon/deserializers/contiguous.hpp>
#include <jaffarCommon/hash.hpp>
#include <jaffarCommon/string.hpp>
#include <jaffarCommon/file.hpp>
#include "snes9xInstance.hpp"
#include <atomic>
#include <thread>
#include <vector>
#include <string>
#include <cstdio>
#include <chrono>

namespace
{
struct RunInputs
{
  nlohmann::json config;
  std::string romData;
  std::vector<std::string> sequence;
  std::vector<std::string> disabledBlocks;
  std::string cycleType;
};

// Replay the sequence on a fresh instance and return the final state hash.
jaffarCommon::hash::hash_t runOne(const RunInputs &in)
{
  snes9x::EmuInstance e(in.config);
  e.loadROM(in.romData);
  for (const auto &b : in.disabledBlocks) e.disableStateBlock(b);
  e.disableRendering();

  const auto inputParser = e.getInputParser();
  std::vector<jaffar::input_t> decoded;
  decoded.reserve(in.sequence.size());
  for (const auto &s : in.sequence) decoded.push_back(inputParser->parseInputString(s));

  const bool full = in.cycleType == "Full";
  const bool reload = in.cycleType == "Rerecord" || full;
  const size_t stateSize = e.getStateSize();
  std::vector<uint8_t> scratch(stateSize);

  for (const auto &input : decoded)
  {
    if (full) e.advanceState(input);
    if (reload)
    {
      jaffarCommon::serializer::Contiguous s(scratch.data(), stateSize);
      e.serializeState(s);
      jaffarCommon::deserializer::Contiguous d(scratch.data(), stateSize);
      e.deserializeState(d);
    }
    e.advanceState(input);
  }
  return e.getStateHash();
}
} // namespace

int main(int argc, char *argv[])
{
  argparse::ArgumentParser program("parallelTester", "1.0");
  program.add_argument("scriptFile").help("Path to the test script file.").required();
  program.add_argument("--threads").help("Number of concurrent worker threads.").default_value(std::string("256"));
  program.add_argument("--cycleType").help("'Simple', 'Rerecord', or 'Full'.").default_value(std::string("Simple"));
  try { program.parse_args(argc, argv); }
  catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  const auto scriptFilePath = program.get<std::string>("scriptFile");
  const size_t numThreads = std::stoul(program.get<std::string>("--threads"));

  RunInputs in;
  in.cycleType = program.get<std::string>("--cycleType");

  std::string configRaw;
  if (!jaffarCommon::file::loadStringFromFile(configRaw, scriptFilePath)) JAFFAR_THROW_LOGIC("Cannot read script: %s\n", scriptFilePath.c_str());
  in.config = nlohmann::json::parse(configRaw);

  const auto romPath = jaffarCommon::json::getString(in.config, "Rom File");
  if (!jaffarCommon::file::loadStringFromFile(in.romData, romPath)) JAFFAR_THROW_LOGIC("Cannot read ROM: %s\n", romPath.c_str());
  in.disabledBlocks = jaffarCommon::json::getArray<std::string>(in.config, "Disable State Blocks");

  std::string seqRaw;
  const auto seqPath = jaffarCommon::json::getString(in.config, "Sequence File");
  if (!jaffarCommon::file::loadStringFromFile(seqRaw, seqPath)) JAFFAR_THROW_LOGIC("Cannot read sequence: %s\n", seqPath.c_str());
  in.sequence = jaffarCommon::string::split(seqRaw, ' ');

  printf("[] Parallel determinism test: %lu threads, cycleType '%s', %lu inputs\n",
         numThreads, in.cycleType.c_str(), in.sequence.size());
  fflush(stdout);

  // Single-threaded reference.
  const auto reference = runOne(in);
  printf("[] Reference hash (1 thread): %s\n", jaffarCommon::hash::hashToString(reference).c_str());
  fflush(stdout);

  // Concurrent workers.
  std::vector<jaffarCommon::hash::hash_t> hashes(numThreads);
  std::vector<std::thread> workers;
  std::atomic<size_t> started{0};
  workers.reserve(numThreads);
  auto t0 = std::chrono::high_resolution_clock::now();
  for (size_t t = 0; t < numThreads; t++)
    workers.emplace_back([&, t]() {
      started.fetch_add(1);
      // spin until all threads are live, to maximize overlap on any shared state
      while (started.load() < numThreads) { /* busy wait */ }
      hashes[t] = runOne(in);
    });
  for (auto &w : workers) w.join();
  auto tf = std::chrono::high_resolution_clock::now();
  double sec = std::chrono::duration_cast<std::chrono::nanoseconds>(tf - t0).count() * 1.0e-9;
  double aggInputs = (double)numThreads * (double)in.sequence.size();
  printf("[] Aggregate throughput (%lu threads): %.1f inputs/s over %.2fs (%.1f inputs/s/thread)\n",
         numThreads, aggInputs / sec, sec, aggInputs / sec / (double)numThreads);

  size_t mismatches = 0;
  for (size_t t = 0; t < numThreads; t++)
    if (hashes[t] != reference)
    {
      if (mismatches < 8)
        printf("[FAIL] thread %lu hash %s != reference\n", t, jaffarCommon::hash::hashToString(hashes[t]).c_str());
      mismatches++;
    }

  if (mismatches != 0)
  {
    printf("[FAIL] %lu / %lu worker threads diverged from the reference — shared-state leak.\n", mismatches, numThreads);
    return 1;
  }
  printf("[OK] All %lu concurrent workers matched the reference. No shared-state leak detected.\n", numThreads);
  return 0;
}
