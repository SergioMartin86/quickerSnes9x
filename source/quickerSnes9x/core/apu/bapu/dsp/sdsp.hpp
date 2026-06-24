#include "SPC_DSP.h"
#include <stdio.h>

class DSP : public Processor
{
  public:

  inline uint8 read(uint8 addr)
  {
    synchronize();
    return spc_dsp.read(addr);
  }

  inline void synchronize(void)
  {
    if (clock)
    {
      spc_dsp.run(clock);
      clock = 0;
    }
  }

  inline void write(uint8 addr, uint8 data)
  {
    synchronize();
    spc_dsp.write(addr, data);
  }

  void save_state(uint8 **);
  void load_state(uint8 **);

  void power();
  void reset();

  // Constant-initialized so the thread_local `dsp` lands in .tbss with no TLS
  // init guard. `dsp.clock` is touched in the innermost SMP tick (smp/core.cpp),
  // a different TU from dsp's definition, so a guarded access there costs ~4% of
  // total emulation time. A constexpr ctor removes the guard. The real DSP state
  // is set up later by power()/reset() at APU init, exactly as before.
  constexpr DSP() : Processor{0, 0}, spc_dsp{} {}

  SPC_DSP spc_dsp;
};

extern thread_local DSP dsp;
