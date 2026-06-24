// Cross-thread state-transfer test for the quickerSnes9x core.
//
// A serialized state must be portable between threads: if it embeds any
// per-thread affinity (e.g. a raw pointer into one thread's TLS), then loading
// it on a different thread would corrupt emulation. This test proves transfer
// safety by handing states between threads and checking determinism:
//
//   Save (thread A)  ->  Load (thread B)  ->  random inputs  ->  reload  ->  verify
//
// Method:
//   1. Single-threaded reference: replay the sequence, snapshotting a START
//      state every STRIDE inputs (a "checkpoint"). From each checkpoint, apply
//      a deterministic pseudo-random run (inputs sampled from the real sequence,
//      seeded by checkpoint index) to get a canonical END state + hashes.
//   2. Concurrent workers pull checkpoint jobs. For each, a worker:
//        a. loads the START bytes produced by the *main* thread (A->B transfer)
//           and checks the hash,
//        b. replays the same pseudo-random run and checks the END hash,
//        c. publishes its END bytes into a shared, write-once slot,
//        d. loads some *other* worker's published END bytes (B->C transfer)
//           and checks that hash,
//        e. round-trips its own END state (save->load->reload) and re-checks.
//   Any per-thread affinity in the serialized state shows up as a hash mismatch.
//
// Usage: quickerSnes9xTransferTester <script> [--threads N] [--stride S] [--runLen L] [--jobs J]

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
#include <cstdint>

namespace
{
// Tiny deterministic PRNG (splitmix64) — seeded per checkpoint so every thread
// generates the identical "random" run and results are comparable.
struct Rng
{
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed) {}
  uint64_t next()
  {
    uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }
};

struct Inputs
{
  nlohmann::json config;
  std::string romData;
  std::vector<jaffar::input_t> seq; // decoded reference sequence (input sample pool)
  size_t stride;                    // checkpoint spacing
  size_t runLen;                    // pseudo-random run length per checkpoint
};

// Build a configured, ROM-loaded instance with rendering off.
//
// NOTE: this test deliberately uses the FULL save state (no "Disable State
// Blocks"). Thread-portability is a property of the core's serialized state
// itself — whether it embeds any per-thread affinity. Honoring a game's
// disabled blocks would instead test state *self-containment*: a reduced state
// only restores the serialized regions, so loading it onto an instance with
// different residual memory and then applying ARBITRARY (random) inputs can
// legitimately diverge if the game reads an un-serialized region. That is a
// separate concern, validated per-solution by the base-vs-quicker equivalence
// test on the actual input sequence — not a thread-safety question.
snes9x::EmuInstance *makeInstance(const Inputs &in)
{
  auto *e = new snes9x::EmuInstance(in.config);
  e->loadROM(in.romData);
  e->disableRendering();
  return e;
}

// The deterministic pseudo-random run from checkpoint c: runLen inputs sampled
// from the real sequence, seeded by c. Same for every thread.
void applyRun(snes9x::EmuInstance *e, const Inputs &in, size_t c)
{
  Rng rng(0xC0FFEE00ULL + c);
  for (size_t i = 0; i < in.runLen; i++)
    e->advanceState(in.seq[rng.next() % in.seq.size()]);
}
} // namespace

int main(int argc, char *argv[])
{
  argparse::ArgumentParser program("transferTester", "1.0");
  program.add_argument("scriptFile").help("Path to the test script file.").required();
  program.add_argument("--threads").help("Concurrent worker threads.").default_value(std::string("256"));
  program.add_argument("--stride").help("Inputs between checkpoints.").default_value(std::string("64"));
  program.add_argument("--runLen").help("Pseudo-random inputs applied per checkpoint.").default_value(std::string("48"));
  program.add_argument("--jobs").help("Total transfer jobs across all workers.").default_value(std::string("4096"));
  try { program.parse_args(argc, argv); }
  catch (const std::runtime_error &err) { JAFFAR_THROW_LOGIC("%s\n%s", err.what(), program.help().str().c_str()); }

  const size_t numThreads = std::stoul(program.get<std::string>("--threads"));
  const size_t numJobs    = std::stoul(program.get<std::string>("--jobs"));

  Inputs in;
  in.stride = std::stoul(program.get<std::string>("--stride"));
  in.runLen = std::stoul(program.get<std::string>("--runLen"));

  std::string configRaw;
  if (!jaffarCommon::file::loadStringFromFile(configRaw, program.get<std::string>("scriptFile"))) JAFFAR_THROW_LOGIC("Cannot read script\n");
  in.config = nlohmann::json::parse(configRaw);
  const auto romPath = jaffarCommon::json::getString(in.config, "Rom File");
  if (!jaffarCommon::file::loadStringFromFile(in.romData, romPath)) JAFFAR_THROW_LOGIC("Cannot read ROM: %s\n", romPath.c_str());

  std::string seqRaw;
  if (!jaffarCommon::file::loadStringFromFile(seqRaw, jaffarCommon::json::getString(in.config, "Sequence File"))) JAFFAR_THROW_LOGIC("Cannot read sequence\n");
  {
    auto *e = makeInstance(in);
    const auto parser = e->getInputParser();
    for (const auto &s : jaffarCommon::string::split(seqRaw, ' ')) in.seq.push_back(parser->parseInputString(s));
    delete e;
  }

  // ---- Phase 1: single-threaded reference (checkpoints + canonical hashes) ----
  auto *ref = makeInstance(in);
  const size_t stateSize = ref->getStateSize();
  const size_t numCkpt = (in.seq.size() / in.stride);
  if (numCkpt == 0) JAFFAR_THROW_LOGIC("Sequence too short for stride %lu\n", in.stride);

  std::vector<uint8_t> startStates(numCkpt * stateSize); // main-thread START bytes (read-only to workers)
  std::vector<jaffarCommon::hash::hash_t> startHash(numCkpt), endHash(numCkpt);

  std::vector<uint8_t> scratch(stateSize);
  size_t produced = 0;
  for (size_t step = 0; step < in.seq.size() && produced < numCkpt; step++)
  {
    if (step % in.stride == 0)
    {
      // snapshot START
      jaffarCommon::serializer::Contiguous s(startStates.data() + produced * stateSize, stateSize);
      ref->serializeState(s);
      startHash[produced] = ref->getStateHash();
      // compute canonical END by running the pseudo-random run on a clone
      auto *clone = makeInstance(in);
      jaffarCommon::deserializer::Contiguous d(startStates.data() + produced * stateSize, stateSize);
      clone->deserializeState(d);
      applyRun(clone, in, produced);
      endHash[produced] = clone->getStateHash();
      delete clone;
      produced++;
    }
    ref->advanceState(in.seq[step]);
  }
  delete ref;
  printf("[] Transfer test: %lu threads, %lu jobs, %lu checkpoints (stride %lu, runLen %lu), state %lu B\n",
         numThreads, numJobs, numCkpt, in.stride, in.runLen, stateSize);
  fflush(stdout);

  // ---- Phase 2: concurrent cross-thread transfer ----
  std::vector<uint8_t> endStates(numCkpt * stateSize);     // worker-published END bytes (write-once per slot)
  std::vector<std::atomic<bool>> published(numCkpt);
  for (auto &p : published) p.store(false);
  std::atomic<size_t> jobCounter{0};
  std::atomic<size_t> mismatches{0};
  std::atomic<size_t> crossWorkerChecks{0};

  auto worker = [&]() {
    auto *e = makeInstance(in);
    std::vector<uint8_t> local(stateSize);
    size_t j;
    while ((j = jobCounter.fetch_add(1)) < numJobs)
    {
      const size_t c = j % numCkpt;

      // (a) A->B transfer: load main thread's START bytes
      { jaffarCommon::deserializer::Contiguous d(startStates.data() + c * stateSize, stateSize); e->deserializeState(d); }
      if (e->getStateHash() != startHash[c]) { mismatches.fetch_add(1); continue; }

      // (b) deterministic random run -> canonical END
      applyRun(e, in, c);
      if (e->getStateHash() != endHash[c]) { mismatches.fetch_add(1); continue; }

      // (c) publish END bytes (write-once; all workers produce identical bytes)
      if (!published[c].load(std::memory_order_acquire))
      {
        jaffarCommon::serializer::Contiguous s(endStates.data() + c * stateSize, stateSize);
        e->serializeState(s);
        published[c].store(true, std::memory_order_release);
      }

      // (d) B->C transfer: load some other worker's published END bytes
      const size_t c2 = (c + 1 + (j % (numCkpt > 1 ? numCkpt - 1 : 1))) % numCkpt;
      if (published[c2].load(std::memory_order_acquire))
      {
        jaffarCommon::deserializer::Contiguous d(endStates.data() + c2 * stateSize, stateSize);
        e->deserializeState(d);
        crossWorkerChecks.fetch_add(1);
        if (e->getStateHash() != endHash[c2]) { mismatches.fetch_add(1); continue; }

        // (e) round-trip the transferred state (save->reload) — must be stable
        { jaffarCommon::serializer::Contiguous s(local.data(), stateSize); e->serializeState(s); }
        { jaffarCommon::deserializer::Contiguous d2(local.data(), stateSize); e->deserializeState(d2); }
        if (e->getStateHash() != endHash[c2]) { mismatches.fetch_add(1); continue; }
      }
    }
    delete e;
  };

  std::vector<std::thread> workers;
  workers.reserve(numThreads);
  for (size_t t = 0; t < numThreads; t++) workers.emplace_back(worker);
  for (auto &w : workers) w.join();

  printf("[] Completed %lu jobs; %lu cross-worker (B->C) transfers verified.\n", numJobs, crossWorkerChecks.load());
  if (mismatches.load() != 0)
  {
    printf("[FAIL] %lu transfer mismatch(es) — a serialized state is not thread-portable.\n", mismatches.load());
    return 1;
  }
  printf("[OK] All cross-thread state transfers reproduced the reference. States are thread-portable.\n");
  return 0;
}
