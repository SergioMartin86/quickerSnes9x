template <unsigned cycle_frequency>
void SMP::Timer<cycle_frequency>::tick()
{
  if (++stage1_ticks < cycle_frequency) return;

  stage1_ticks = 0;
  if (enable == false) return;

  if (++stage2_ticks != target) return;

  stage2_ticks = 0;
  stage3_ticks = (stage3_ticks + 1) & 15;
}

template <unsigned cycle_frequency>
void SMP::Timer<cycle_frequency>::tick(unsigned clocks)
{
  stage1_ticks += clocks;
  if (stage1_ticks < cycle_frequency) return;

  stage1_ticks -= cycle_frequency;
  if (enable == false) return;

  if (++stage2_ticks != target) return;

  stage2_ticks = 0;
  stage3_ticks = (stage3_ticks + 1) & 15;
}

// Bit-exact closed-form catch-up of `clocks` cycles, equivalent to calling
// tick() `clocks` times. enable/target are constant between observation points
// (they only change via mmio_write, which flushes first), so they are fixed
// across the whole interval.
template <unsigned cycle_frequency>
void SMP::Timer<cycle_frequency>::tick_batch(unsigned clocks)
{
  // stage1 is a mod-cycle_frequency divider; it advances (and resets) regardless
  // of enable, exactly as in the per-cycle path.
  unsigned total     = (unsigned)stage1_ticks + clocks;
  unsigned rollovers = total / cycle_frequency;
  stage1_ticks       = (uint8)(total % cycle_frequency);
  if (enable == false || rollovers == 0) return;

  // Replicate `rollovers` iterations of:
  //   if (++stage2_ticks != target) continue;  stage2_ticks = 0;  stage3_ticks = (stage3_ticks+1)&15;
  // stage2_ticks is a uint8 (wraps at 256); a "hit" is when the post-increment
  // value equals target. target == 0 therefore means a full 256-step period.
  unsigned period   = target ? (unsigned)target : 256u;
  unsigned firstHit = (uint8)(target - stage2_ticks); // steps to the first hit
  if (firstHit == 0) firstHit = 256u;                 // ++ happens first; equal-at-start => full wrap

  if (rollovers < firstHit) { stage2_ticks = (uint8)(stage2_ticks + rollovers); return; }

  unsigned remaining = rollovers - firstHit;          // increments after the first hit (stage2 now 0)
  unsigned hits      = 1u + remaining / period;
  stage2_ticks       = (uint8)(remaining % period);
  stage3_ticks       = (stage3_ticks + hits) & 15;
}
