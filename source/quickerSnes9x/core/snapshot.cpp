/***********************************************************************************
  See snapshot.h for the copyright notice
 ***********************************************************************************/

#include <assert.h>
#include "snes9x.h"
#include "memmap.h"
#include "dma.h"
#include "apu/apu.h"
#include "fxinst.h"
#include "fxemu.h"
#include "sdd1.h"
#include "srtc.h"
#include "snapshot.h"
#include "controls.h"
#include "movie.h"
#include "display.h"
#include "language.h"

void S9xFreezeToStream(jaffarCommon::serializer::Base &s, const optionalBlocks_t& optionalBlocks)
{
  uint8 soundsnapshot[SPC_SAVE_STATE_BLOCK_SIZE];

  s.push(&CPU, sizeof(CPU));

  s.push(&Registers, sizeof(Registers));

  if (optionalBlocks._enablePPUBlock) s.push(&PPU, sizeof(PPU));
  if (optionalBlocks._enableDMABlock) s.push(&DMA, sizeof(DMA));
  if (optionalBlocks._enableVRABlock) s.push(Memory.VRAM, 0x10000);
  if (optionalBlocks._enableRAMBlock) s.push(Memory.RAM, 0x20000);
  if (optionalBlocks._enableSRABlock) s.push(Memory.SRAM, 0x20000);
  if (optionalBlocks._enableFILBlock) s.push(Memory.FillRAM, 0x8000);

  if (optionalBlocks._enableSNDBlock)
  {
    S9xAPUSaveState(soundsnapshot);
    s.push(soundsnapshot, SPC_SAVE_STATE_BLOCK_SIZE);
  }

  if (optionalBlocks._enableCTLBlock)
  {
    struct SControlSnapshot ctl_snap;
    S9xControlPreSaveState(&ctl_snap);
    s.push(&ctl_snap, sizeof(ctl_snap));
  }

  if (optionalBlocks._enableTIMBlock) s.push(&Timings, sizeof(Timings));

  if (Settings.SuperFX)
  {
    GSU.avRegAddr = (uint8 *)&GSU.avReg;
    s.push(&GSU, sizeof(GSU));
  }

  if (Settings.SA1)
  {
    S9xSA1PackStatus();
    s.push(&SA1, sizeof(SA1));
    s.push(&SA1Registers, sizeof(SA1Registers));
  }

  if (Settings.DSP == 1) s.push(&DSP1, sizeof(DSP1));
  if (Settings.DSP == 2) s.push(&DSP2, sizeof(DSP2));
  if (Settings.DSP == 4) s.push(&DSP4, sizeof(DSP4));
  if (Settings.C4) s.push(Memory.C4RAM, 8192);
  if (Settings.SETA == ST_010) s.push(&ST010, sizeof(ST010));

  if (Settings.OBC1)
  {
    s.push(&OBC1, sizeof(OBC1));
    s.push(Memory.OBC1RAM, 8192);
  }

  if (Settings.SPC7110)
  {
    S9xSPC7110PreSaveState();
    s.push(&s7snap, sizeof(s7snap));
  }

  if (Settings.SRTC)
  {
    S9xSRTCPreSaveState();
    s.push(&srtcsnap, sizeof(srtcsnap));
  }

  if (Settings.SRTC || Settings.SPC7110RTC) s.push(RTCData.reg, 20);
  if (Settings.BS) s.push(&BSX, sizeof(BSX));
  if (Settings.MSU1) s.push(&MSU1, sizeof(MSU1));
}

int S9xUnfreezeFromStream(jaffarCommon::deserializer::Base &d, const optionalBlocks_t& optionalBlocks)
{
  int version = SNAPSHOT_VERSION;

  struct SControlSnapshot ctl_snap;

  d.pop(&CPU, sizeof(CPU));
  d.pop(&Registers, sizeof(Registers));

  if (optionalBlocks._enablePPUBlock)
  {
    S9xResetPPU();
    d.pop(&PPU, sizeof(PPU));
  }
  if (optionalBlocks._enableDMABlock) d.pop(&DMA, sizeof(DMA));
  if (optionalBlocks._enableVRABlock) d.pop(Memory.VRAM, 0x10000);
  if (optionalBlocks._enableRAMBlock) d.pop(Memory.RAM, 0x20000);
  if (optionalBlocks._enableSRABlock) d.pop(Memory.SRAM, 0x20000);
  if (optionalBlocks._enableFILBlock) d.pop(Memory.FillRAM, 0x8000);

  if (optionalBlocks._enableSNDBlock)
  {
    uint8_t soundsnapshot[SPC_SAVE_STATE_BLOCK_SIZE];
    d.pop(soundsnapshot, SPC_SAVE_STATE_BLOCK_SIZE);
    S9xAPULoadState(soundsnapshot);
  }

  if (optionalBlocks._enableCTLBlock) d.pop(&ctl_snap, sizeof(ctl_snap));
  if (optionalBlocks._enableTIMBlock) d.pop(&Timings, sizeof(Timings));

  if (Settings.SuperFX)
  {
    GSU.avRegAddr = (uint8 *)&GSU.avReg;
    d.pop(&GSU, sizeof(GSU));
  }

  if (Settings.SA1)
  {
    uint32 sa1_old_flags = SA1.Flags;

    d.pop(&SA1, sizeof(SA1));
    d.pop(&SA1Registers, sizeof(SA1Registers));

    SA1.Cycles = SA1.PrevCycles = 0;
    SA1.TimerIRQLastState       = FALSE;
    SA1.HTimerIRQPos            = Memory.FillRAM[0x2212] | (Memory.FillRAM[0x2213] << 8);
    SA1.VTimerIRQPos            = Memory.FillRAM[0x2214] | (Memory.FillRAM[0x2215] << 8);
    SA1.HCounter                = 0;
    SA1.VCounter                = 0;
    SA1.PrevHCounter            = 0;
    SA1.MemSpeed                = SLOW_ONE_CYCLE;
    SA1.MemSpeedx2              = SLOW_ONE_CYCLE * 2;

    SA1.Flags |= sa1_old_flags & TRACE_FLAG;
    S9xSA1PostLoadState();
  }

  if (Settings.DSP == 1) d.pop(&DSP1, sizeof(DSP1));
  if (Settings.DSP == 2) d.pop(&DSP2, sizeof(DSP2));
  if (Settings.DSP == 4) d.pop(&DSP4, sizeof(DSP4));
  if (Settings.C4) d.pop(Memory.C4RAM, 8192);
  if (Settings.SETA == ST_010) d.pop(&ST010, sizeof(ST010));

  if (Settings.OBC1)
  {
    d.pop(&OBC1, sizeof(OBC1));
    d.pop(Memory.OBC1RAM, 8192);
  }

  if (Settings.SPC7110) d.pop(&s7snap, sizeof(s7snap));
  if (Settings.SRTC) d.pop(&srtcsnap, sizeof(srtcsnap));
  if (Settings.SRTC || Settings.SPC7110RTC) d.pop(RTCData.reg, 20);
  if (Settings.BS) d.pop(&BSX, sizeof(BSX));
  if (Settings.MSU1) d.pop(&MSU1, sizeof(MSU1));

  uint32 old_flags = CPU.Flags;

  CPU.Flags |= old_flags & (DEBUG_MODE_FLAG | TRACE_FLAG | SINGLE_STEP_FLAG | FRAME_ADVANCE_FLAG);
  ICPU.ShiftedPB = Registers.PB << 16;
  ICPU.ShiftedDB = Registers.DB << 16;
  S9xSetPCBase(Registers.PBPC);
  S9xUnpackStatus();
  S9xFixCycles();

  CPU.InDMA = CPU.InHDMA = FALSE;
  CPU.InDMAorHDMA = CPU.InWRAMDMAorHDMA = FALSE;
  CPU.HDMARanInDMA                      = 0;

  S9xFixColourBrightness();
  IPPU.ColorsChanged   = TRUE;
  IPPU.OBJChanged      = TRUE;
  IPPU.RenderThisFrame = TRUE;

  uint8 hdma_byte = Memory.FillRAM[0x420c];
  S9xSetCPU(hdma_byte, 0x420c);

  S9xControlPostLoadState(&ctl_snap);

  if (Settings.SDD1) S9xSDD1PostLoadState();
  if (Settings.SPC7110) S9xSPC7110PostLoadState(version);
  if (Settings.SRTC) S9xSRTCPostLoadState(version);
  if (Settings.BS) S9xBSXPostLoadState();
  if (Settings.MSU1) S9xMSU1PostLoadState();

  return 0;
}
