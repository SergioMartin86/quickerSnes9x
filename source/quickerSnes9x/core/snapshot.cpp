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
#include <lightStateConfig.h>

#ifndef min
#define min(a,b)	(((a) < (b)) ? (a) : (b))
#endif

typedef struct
{
	int			offset;
	int			offset2;
	int			size;
	int			type;
	uint16		debuted_in;
	uint16		deleted_in;
	const char	*name;
}	FreezeData;

enum
{
	INT_V,
	uint8_ARRAY_V,
	uint16_ARRAY_V,
	uint32_ARRAY_V,
	uint8_INDIR_ARRAY_V,
	uint16_INDIR_ARRAY_V,
	uint32_INDIR_ARRAY_V,
	POINTER_V
};

#define COUNT(ARRAY)				(sizeof(ARRAY) / sizeof(ARRAY[0]))
#define Offset(field, structure)	((int) (((char *) (&(((structure) NULL)->field))) - ((char *) NULL)))
#define OFFSET(f)					Offset(f, STRUCT *)
#define DUMMY(f)					Offset(f, struct Obsolete *)
#define DELETED(f)					(-1)

#define INT_ENTRY(save_version_introduced, field) \
{ \
	OFFSET(field), \
	0, \
	sizeof(((STRUCT *) NULL)->field), \
	INT_V, \
	save_version_introduced, \
	9999, \
	#field \
}

#define ARRAY_ENTRY(save_version_introduced, field, count, elemType) \
{ \
	OFFSET(field), \
	0, \
	count, \
	elemType, \
	save_version_introduced, \
	9999, \
	#field \
}

#define POINTER_ENTRY(save_version_introduced, field, relativeToField) \
{ \
	OFFSET(field), \
	OFFSET(relativeToField), \
	4, \
	POINTER_V, \
	save_version_introduced, \
	9999, \
	#field \
}

#define OBSOLETE_INT_ENTRY(save_version_introduced, save_version_removed, field) \
{ \
	DUMMY(field), \
	0, \
	sizeof(((struct Obsolete *) NULL)->field), \
	INT_V, \
	save_version_introduced, \
	save_version_removed, \
	#field \
}

#define OBSOLETE_ARRAY_ENTRY(save_version_introduced, save_version_removed, field, count, elemType) \
{ \
	DUMMY(field), \
	0, \
	count, \
	elemType, \
	save_version_introduced, \
	save_version_removed, \
	#field \
}

#define OBSOLETE_POINTER_ENTRY(save_version_introduced, save_version_removed, field, relativeToField) \
{ \
	DUMMY(field), \
	DUMMY(relativeToField), \
	4, \
	POINTER_V, \
	save_version_introduced, \
	save_version_removed, \
	#field \
}

#define DELETED_INT_ENTRY(save_version_introduced, save_version_removed, field, size) \
{ \
	DELETED(field), \
	0, \
	size, \
	INT_V, \
	save_version_introduced, \
	save_version_removed, \
	#field \
}

#define DELETED_ARRAY_ENTRY(save_version_introduced, save_version_removed, field, count, elemType) \
{ \
	DELETED(field), \
	0, \
	count, \
	elemType, \
	save_version_introduced, \
	save_version_removed, \
	#field \
}

#define DELETED_POINTER_ENTRY(save_version_introduced, save_version_removed, field, relativeToField) \
{ \
	DELETED(field), \
	DELETED(relativeToField), \
	4, \
	POINTER_V, \
	save_version_introduced, \
	save_version_removed, \
	#field \
}

struct SDMASnapshot
{
	struct SDMA	dma[8];
};

struct SnapshotMovieInfo
{
	uint32	MovieInputDataSize;
};

struct SnapshotScreenshotInfo
{
	uint16	Width;
	uint16	Height;
	uint8	Interlaced;
	uint8	Data[MAX_SNES_WIDTH * MAX_SNES_HEIGHT * 3];
};

static thread_local struct Obsolete
{
	uint8	CPU_IRQActive;
}	Obsolete;

#define STRUCT	struct SRegisters

static __thread FreezeData	SnapRegisters[] =
{
	INT_ENTRY(6, PB),
	INT_ENTRY(6, DB),
	INT_ENTRY(6, P.W),
	INT_ENTRY(6, A.W),
	INT_ENTRY(6, D.W),
	INT_ENTRY(6, S.W),
	INT_ENTRY(6, X.W),
	INT_ENTRY(6, Y.W),
	INT_ENTRY(6, PCw)
};

#undef STRUCT

#define STRUCT	struct SControlSnapshot

static __thread FreezeData	SnapControls[] =
{
	INT_ENTRY(6, ver),
	ARRAY_ENTRY(6, port1_read_idx, 2, uint8_ARRAY_V),
	ARRAY_ENTRY(6, dummy1, 4, uint8_ARRAY_V),
	ARRAY_ENTRY(6, port2_read_idx, 2, uint8_ARRAY_V),
	ARRAY_ENTRY(6, dummy2, 4, uint8_ARRAY_V),
	ARRAY_ENTRY(6, mouse_speed, 2, uint8_ARRAY_V),
	INT_ENTRY(6, justifier_select),
	ARRAY_ENTRY(6, dummy3, 8, uint8_ARRAY_V),
	INT_ENTRY(6, pad_read),
	INT_ENTRY(6, pad_read_last),
	ARRAY_ENTRY(6, internal, 60, uint8_ARRAY_V)
};

#undef STRUCT
#define STRUCT	struct STimings

static __thread FreezeData	SnapTimings[] =
{
	INT_ENTRY(6, H_Max_Master),
	INT_ENTRY(6, H_Max),
	INT_ENTRY(6, V_Max_Master),
	INT_ENTRY(6, V_Max),
	INT_ENTRY(6, HBlankStart),
	INT_ENTRY(6, HBlankEnd),
	INT_ENTRY(6, HDMAInit),
	INT_ENTRY(6, HDMAStart),
	INT_ENTRY(6, NMITriggerPos),
	INT_ENTRY(6, WRAMRefreshPos),
	INT_ENTRY(6, RenderPos),
	INT_ENTRY(6, InterlaceField),
	INT_ENTRY(6, DMACPUSync),
	INT_ENTRY(6, NMIDMADelay),
	INT_ENTRY(6, IRQPendCount),
	INT_ENTRY(6, APUSpeedup),
	INT_ENTRY(7, IRQTriggerCycles),
	INT_ENTRY(7, APUAllowTimeOverflow)
};

#undef STRUCT
#define STRUCT	struct FxRegs_s

static __thread FreezeData	SnapFX[] =
{
	ARRAY_ENTRY(6, avReg, 16, uint32_ARRAY_V),
	INT_ENTRY(6, vColorReg),
	INT_ENTRY(6, vPlotOptionReg),
	INT_ENTRY(6, vStatusReg),
	INT_ENTRY(6, vPrgBankReg),
	INT_ENTRY(6, vRomBankReg),
	INT_ENTRY(6, vRamBankReg),
	INT_ENTRY(6, vCacheBaseReg),
	INT_ENTRY(6, vCacheFlags),
	INT_ENTRY(6, vLastRamAdr),
	POINTER_ENTRY(6, pvDreg, avRegAddr),
	POINTER_ENTRY(6, pvSreg, avRegAddr),
	INT_ENTRY(6, vRomBuffer),
	INT_ENTRY(6, vPipe),
	INT_ENTRY(6, vPipeAdr),
	INT_ENTRY(6, vSign),
	INT_ENTRY(6, vZero),
	INT_ENTRY(6, vCarry),
	INT_ENTRY(6, vOverflow),
	INT_ENTRY(6, vErrorCode),
	INT_ENTRY(6, vIllegalAddress),
	INT_ENTRY(6, bBreakPoint),
	INT_ENTRY(6, vBreakPoint),
	INT_ENTRY(6, vStepPoint),
	INT_ENTRY(6, nRamBanks),
	INT_ENTRY(6, nRomBanks),
	INT_ENTRY(6, vMode),
	INT_ENTRY(6, vPrevMode),
	POINTER_ENTRY(6, pvScreenBase, pvRam),
#define O(N) \
	POINTER_ENTRY(6, apvScreen[N], pvRam)
	O(  0), O(  1), O(  2), O(  3), O(  4), O(  5), O(  6), O(  7),
	O(  8), O(  9), O( 10), O( 11), O( 12), O( 13), O( 14), O( 15),
	O( 16), O( 17), O( 18), O( 19), O( 20), O( 21), O( 22), O( 23),
	O( 24), O( 25), O( 26), O( 27), O( 28), O( 29), O( 30), O( 31),
#undef O
	ARRAY_ENTRY(6, x, 32, uint32_ARRAY_V),
	INT_ENTRY(6, vScreenHeight),
	INT_ENTRY(6, vScreenRealHeight),
	INT_ENTRY(6, vPrevScreenHeight),
	INT_ENTRY(6, vScreenSize),
	POINTER_ENTRY(6, pvRamBank, apvRamBank),
	POINTER_ENTRY(6, pvRomBank, apvRomBank),
	POINTER_ENTRY(6, pvPrgBank, apvRomBank),
#define O(N) \
	POINTER_ENTRY(6, apvRamBank[N], pvRam)
	O(0), O(1), O(2), O(3),
#undef O
	INT_ENTRY(6, bCacheActive),
	POINTER_ENTRY(6, pvCache, pvRegisters),
	ARRAY_ENTRY(6, avCacheBackup, 512, uint8_ARRAY_V),
	INT_ENTRY(6, vCounter),
	INT_ENTRY(6, vInstCount),
	INT_ENTRY(6, vSCBRDirty)
};

#undef STRUCT
#define STRUCT	struct SSA1

static __thread FreezeData	SnapSA1[] =
{
	DELETED_INT_ENTRY(6, 7, CPUExecuting, 1),
	INT_ENTRY(6, ShiftedPB),
	INT_ENTRY(6, ShiftedDB),
	INT_ENTRY(6, Flags),
	DELETED_INT_ENTRY(6, 7, IRQActive, 1),
	DELETED_INT_ENTRY(6, 7, Waiting, 1),
	INT_ENTRY(6, WaitingForInterrupt),
	DELETED_INT_ENTRY(6, 7, WaitAddress, 4),
	DELETED_INT_ENTRY(6, 7, WaitCounter, 4),
	DELETED_INT_ENTRY(6, 7, PBPCAtOpcodeStart, 4),
	DELETED_INT_ENTRY(6, 7, Executing, 1),
	INT_ENTRY(6, overflow),
	INT_ENTRY(6, in_char_dma),
	INT_ENTRY(6, op1),
	INT_ENTRY(6, op2),
	INT_ENTRY(6, arithmetic_op),
	INT_ENTRY(6, sum),
	INT_ENTRY(6, VirtualBitmapFormat),
	INT_ENTRY(6, variable_bit_pos),
	INT_ENTRY(7, Cycles),
	INT_ENTRY(7, PrevCycles),
	INT_ENTRY(7, TimerIRQLastState),
	INT_ENTRY(7, HTimerIRQPos),
	INT_ENTRY(7, VTimerIRQPos),
	INT_ENTRY(7, HCounter),
	INT_ENTRY(7, VCounter),
	INT_ENTRY(7, PrevHCounter),
	INT_ENTRY(7, MemSpeed),
	INT_ENTRY(7, MemSpeedx2)
};

#undef STRUCT
#define STRUCT	struct SSA1Registers

static __thread FreezeData	SnapSA1Registers[] =
{
	INT_ENTRY(6, PB),
	INT_ENTRY(6, DB),
	INT_ENTRY(6, P.W),
	INT_ENTRY(6, A.W),
	INT_ENTRY(6, D.W),
	INT_ENTRY(6, S.W),
	INT_ENTRY(6, X.W),
	INT_ENTRY(6, Y.W),
	INT_ENTRY(6, PCw)
};

#undef STRUCT

#define STRUCT	struct SST010

static __thread FreezeData	SnapST010[] =
{
	ARRAY_ENTRY(6, input_params, 16, uint8_ARRAY_V),
	ARRAY_ENTRY(6, output_params, 16, uint8_ARRAY_V),
	INT_ENTRY(6, op_reg),
	INT_ENTRY(6, execute),
	INT_ENTRY(6, control_enable)
};

#undef STRUCT
#define STRUCT	struct SOBC1

static __thread FreezeData	SnapOBC1[] =
{
	INT_ENTRY(6, address),
	INT_ENTRY(6, basePtr),
	INT_ENTRY(6, shift)
};

#undef STRUCT

#define STRUCT	struct SSRTCSnapshot

static __thread FreezeData	SnapSRTCSnap[] =
{
	INT_ENTRY(6, rtc_mode),
	INT_ENTRY(6, rtc_index)
};

#undef STRUCT
#define STRUCT	struct SBSX

static __thread FreezeData	SnapBSX[] =
{
	INT_ENTRY(6, dirty),
	INT_ENTRY(6, dirty2),
	INT_ENTRY(6, bootup),
	INT_ENTRY(6, flash_enable),
	INT_ENTRY(6, write_enable),
	INT_ENTRY(6, read_enable),
	INT_ENTRY(6, flash_command),
	INT_ENTRY(6, old_write),
	INT_ENTRY(6, new_write),
	INT_ENTRY(6, out_index),
	ARRAY_ENTRY(6, output, 32, uint8_ARRAY_V),
	ARRAY_ENTRY(6, PPU, 32, uint8_ARRAY_V),
	ARRAY_ENTRY(6, MMC, 16, uint8_ARRAY_V),
	ARRAY_ENTRY(6, prevMMC, 16, uint8_ARRAY_V),
	ARRAY_ENTRY(6, test2192, 32, uint8_ARRAY_V)
};

#undef STRUCT
#define STRUCT	struct SMSU1

static __thread FreezeData	SnapMSU1[] =
{
	INT_ENTRY(9, MSU1_STATUS),
	INT_ENTRY(9, MSU1_DATA_SEEK),
	INT_ENTRY(9, MSU1_DATA_POS),
	INT_ENTRY(9, MSU1_TRACK_SEEK),
	INT_ENTRY(9, MSU1_CURRENT_TRACK),
	INT_ENTRY(9, MSU1_RESUME_TRACK),
	INT_ENTRY(9, MSU1_VOLUME),
	INT_ENTRY(9, MSU1_CONTROL),
	INT_ENTRY(9, MSU1_AUDIO_POS),
	INT_ENTRY(9, MSU1_RESUME_POS)
};

#undef STRUCT
#define STRUCT	struct SnapshotScreenshotInfo

static __thread FreezeData	SnapScreenshot[] =
{
	INT_ENTRY(6, Width),
	INT_ENTRY(6, Height),
	INT_ENTRY(6, Interlaced),
	ARRAY_ENTRY(6, Data, MAX_SNES_WIDTH * MAX_SNES_HEIGHT * 3, uint8_ARRAY_V)
};

#undef STRUCT
#define STRUCT	struct SnapshotMovieInfo

static __thread FreezeData	SnapMovie[] =
{
	INT_ENTRY(6, MovieInputDataSize)
};

static int UnfreezeBlock (STREAM, const char *, uint8 *, int);
static int UnfreezeBlockCopy (STREAM, const char *, uint8 **, int);
static int UnfreezeStruct (STREAM, const char *, void *, FreezeData *, int, int);
static int UnfreezeStructCopy (STREAM, const char *, uint8 **, FreezeData *, int, int);
static void UnfreezeStructFromCopy (void *, FreezeData *, int, uint8 *, int);
static void FreezeBlock (STREAM, const char *, uint8 *, int);
static void FreezeStruct (STREAM, const char *, void *, FreezeData *, int);


void S9xResetSaveTimer (bool8 dontsave)
{
	static __thread time_t	t = -1;

	t = time(NULL);
}

uint32 S9xFreezeSize()
{
    nulStream stream;
    S9xFreezeToStream(&stream);
    return stream.size();
}

extern thread_local bool8 pad_read, pad_read_last;

void S9xFreezeToStream (STREAM stream)
{
	uint8	soundsnapshot[SPC_SAVE_STATE_BLOCK_SIZE];

	FreezeBlock(stream, "CPU", (uint8_t*)&CPU, sizeof(CPU));

	FreezeStruct(stream, "REG", &Registers, SnapRegisters, COUNT(SnapRegisters));

	if (_enablePPUBlock) FreezeBlock(stream, "PPU", (uint8_t*)&PPU, sizeof(PPU));

	if (_enableDMABlock)	FreezeBlock(stream, "DMA", (uint8_t*) &DMA, sizeof(DMA));

 if (_enableVRABlock)	FreezeBlock (stream, "VRA", Memory.VRAM, 0x10000);

	if (_enableRAMBlock) FreezeBlock (stream, "RAM", Memory.RAM, 0x20000);

	if (_enableSRABlock) FreezeBlock (stream, "SRA", Memory.SRAM, 0x20000);

	if (_enableFILBlock) FreezeBlock (stream, "FIL", Memory.FillRAM, 0x8000);

	if (_enableSNDBlock)
	{
		S9xAPUSaveState(soundsnapshot);
		FreezeBlock (stream, "SND", soundsnapshot, SPC_SAVE_STATE_BLOCK_SIZE);
	}

	if (_enableCTLBlock)
	{
		struct SControlSnapshot	ctl_snap;
		S9xControlPreSaveState(&ctl_snap);
		FreezeBlock(stream, "CTL", (uint8_t*)&ctl_snap, sizeof(ctl_snap));
	}

	if (_enableTIMBlock)	FreezeBlock(stream, "TIM", (uint8_t*)&Timings, sizeof(Timings));

	if (Settings.SuperFX)
	{
		GSU.avRegAddr = (uint8 *) &GSU.avReg;
		FreezeStruct(stream, "SFX", &GSU, SnapFX, COUNT(SnapFX));
	}

	if (Settings.SA1)
	{
		S9xSA1PackStatus();
		FreezeStruct(stream, "SA1", &SA1, SnapSA1, COUNT(SnapSA1));
		FreezeStruct(stream, "SAR", &SA1Registers, SnapSA1Registers, COUNT(SnapSA1Registers));
	}

	if (Settings.DSP == 1)
	 FreezeBlock(stream, "DP1", (uint8_t*)&DSP1, sizeof(DSP1));

	if (Settings.DSP == 2)
	 FreezeBlock(stream, "DP2", (uint8_t*)&DSP2, sizeof(DSP2));

	if (Settings.DSP == 4)
	 FreezeBlock(stream, "DP4", (uint8_t*)&DSP4, sizeof(DSP4));

	if (Settings.C4)
		FreezeBlock (stream, "CX4", Memory.C4RAM, 8192);

	if (Settings.SETA == ST_010)
		FreezeStruct(stream, "ST0", &ST010, SnapST010, COUNT(SnapST010));

	if (Settings.OBC1)
	{
		FreezeStruct(stream, "OBC", &OBC1, SnapOBC1, COUNT(SnapOBC1));
		FreezeBlock (stream, "OBM", Memory.OBC1RAM, 8192);
	}

	if (Settings.SPC7110)
	{
		S9xSPC7110PreSaveState();
		FreezeBlock(stream, "S71", (uint8_t*)&s7snap, sizeof(s7snap));
	}

	if (Settings.SRTC)
	{
		S9xSRTCPreSaveState();
		FreezeStruct(stream, "SRT", &srtcsnap, SnapSRTCSnap, COUNT(SnapSRTCSnap));
	}

	if (Settings.SRTC || Settings.SPC7110RTC)
		FreezeBlock (stream, "CLK", RTCData.reg, 20);

	if (Settings.BS)
		FreezeStruct(stream, "BSX", &BSX, SnapBSX, COUNT(SnapBSX));

	if (Settings.MSU1)
		FreezeStruct(stream, "MSU", &MSU1, SnapMSU1, COUNT(SnapMSU1));
}

int S9xUnfreezeFromStream (STREAM stream)
{

	int version = SNAPSHOT_VERSION;
	
	uint8	*local_registers     = NULL;
	uint8	*local_sa1           = NULL;
	uint8	*local_sa1_registers = NULL;
	uint8	*local_cx4_data      = NULL;
	uint8	*local_st010         = NULL;
	uint8	*local_obc1          = NULL;
	uint8	*local_obc1_data     = NULL;
	uint8	*local_srtc          = NULL;
	uint8	*local_rtc_data      = NULL;
	uint8	*local_bsx_data      = NULL;
	uint8	*local_msu1_data     = NULL;

struct SControlSnapshot	ctl_snap;

		UnfreezeBlock(stream, "CPU", (uint8_t*) &CPU, sizeof(CPU));
		UnfreezeStructCopy(stream, "REG", &local_registers, SnapRegisters, COUNT(SnapRegisters), version);
  if (_enablePPUBlock)	UnfreezeBlock(stream, "PPU", (uint8_t*)&PPU, sizeof(PPU));
  if (_enableDMABlock) UnfreezeBlock(stream, "DMA", (uint8_t*)&DMA, sizeof(DMA));
		if (_enableVRABlock) UnfreezeBlock(stream, "VRA", (uint8_t*)Memory.VRAM, 0x10000);
		if (_enableRAMBlock) UnfreezeBlock(stream, "RAM", (uint8_t*)Memory.RAM, 0x20000);
		if (_enableSRABlock) UnfreezeBlock(stream, "SRA", (uint8_t*)Memory.SRAM, 0x20000);
		if (_enableFILBlock) UnfreezeBlock(stream, "FIL", (uint8_t*)Memory.FillRAM, 0x8000);
		if (_enableSNDBlock)
		{
			uint8_t soundsnapshot[SPC_SAVE_STATE_BLOCK_SIZE];
			UnfreezeBlock (stream, "SND", soundsnapshot, SPC_SAVE_STATE_BLOCK_SIZE);
   S9xAPULoadState(soundsnapshot);
		}

		if (_enableCTLBlock) UnfreezeBlock (stream, "CTL", (uint8_t*)&ctl_snap, sizeof(ctl_snap));
  if (_enableTIMBlock) UnfreezeBlock(stream, "TIM", (uint8_t*)&Timings, sizeof(Timings));

		if (Settings.SuperFX)
		{
			 GSU.avRegAddr = (uint8 *) &GSU.avReg;
    UnfreezeBlock(stream, "SFX", (uint8_t*) &GSU, sizeof(GSU));
		}

		if (Settings.SA1) UnfreezeStructCopy(stream, "SA1", &local_sa1, SnapSA1, COUNT(SnapSA1), version);
		if (Settings.SA1) UnfreezeStructCopy(stream, "SAR", &local_sa1_registers, SnapSA1Registers, COUNT(SnapSA1Registers), version);
  if (Settings.DSP == 1) UnfreezeBlock(stream, "DP1", (uint8_t*)&DSP1, sizeof(DSP1));
		if (Settings.DSP == 2) UnfreezeBlock(stream, "DP2", (uint8_t*)&DSP2, sizeof(DSP2));
		if (Settings.DSP == 4) UnfreezeBlock(stream, "DP4", (uint8_t*)&DSP4, sizeof(DSP4));
		if (Settings.C4) UnfreezeBlockCopy (stream, "CX4", &local_cx4_data, 8192);
		if (Settings.SETA == ST_010) UnfreezeStructCopy(stream, "ST0", &local_st010, SnapST010, COUNT(SnapST010), version);

		if (Settings.OBC1) UnfreezeStructCopy(stream, "OBC", &local_obc1, SnapOBC1, COUNT(SnapOBC1), version);
	 if (Settings.OBC1) UnfreezeBlockCopy (stream, "OBM", &local_obc1_data, 8192);
	 if (Settings.SPC7110) UnfreezeBlock(stream, "S71", (uint8_t*)&s7snap, sizeof(s7snap));
  if (Settings.SRTC) UnfreezeStructCopy(stream, "SRT", &local_srtc, SnapSRTCSnap, COUNT(SnapSRTCSnap), version);
  if (Settings.SPC7110RTC) UnfreezeBlockCopy (stream, "CLK", &local_rtc_data, 20);
		if (Settings.BS) UnfreezeStructCopy(stream, "BSX", &local_bsx_data, SnapBSX, COUNT(SnapBSX), version);
  if (Settings.MSU1) UnfreezeStructCopy(stream, "MSU", &local_msu1_data, SnapMSU1, COUNT(SnapMSU1), version);
		
		uint32 old_flags     = CPU.Flags;
		uint32 sa1_old_flags = SA1.Flags;

		UnfreezeStructFromCopy(&Registers, SnapRegisters, COUNT(SnapRegisters), local_registers, version);

		if (local_sa1)
			UnfreezeStructFromCopy(&SA1, SnapSA1, COUNT(SnapSA1), local_sa1, version);

		if (local_sa1_registers)
			UnfreezeStructFromCopy(&SA1Registers, SnapSA1Registers, COUNT(SnapSA1Registers), local_sa1_registers, version);

		if (local_cx4_data)
			memcpy(Memory.C4RAM, local_cx4_data, 8192);

		if (local_st010)
			UnfreezeStructFromCopy(&ST010, SnapST010, COUNT(SnapST010), local_st010, version);

		if (local_obc1)
			UnfreezeStructFromCopy(&OBC1, SnapOBC1, COUNT(SnapOBC1), local_obc1, version);

		if (local_obc1_data)
			memcpy(Memory.OBC1RAM, local_obc1_data, 8192);

		if (local_srtc)
			UnfreezeStructFromCopy(&srtcsnap, SnapSRTCSnap, COUNT(SnapSRTCSnap), local_srtc, version);

		if (local_rtc_data)
			memcpy(RTCData.reg, local_rtc_data, 20);

		if (local_bsx_data)
			UnfreezeStructFromCopy(&BSX, SnapBSX, COUNT(SnapBSX), local_bsx_data, version);

		if (local_msu1_data)
			UnfreezeStructFromCopy(&MSU1, SnapMSU1, COUNT(SnapMSU1), local_msu1_data, version);

		if (version < SNAPSHOT_VERSION_IRQ)
		{
			printf("Converting old snapshot version %d to %d\n...", version, SNAPSHOT_VERSION);

			CPU.NMILine = (CPU.Flags & (1 <<  7)) ? TRUE : FALSE;
			CPU.IRQLine = (CPU.Flags & (1 << 11)) ? TRUE : FALSE;
			CPU.IRQTransition = FALSE;
			CPU.IRQLastState = FALSE;
			CPU.IRQExternal = (Obsolete.CPU_IRQActive & ~(1 << 1)) ? TRUE : FALSE;

			switch (CPU.WhichEvent)
			{
				case 12:	case   1:	CPU.WhichEvent = 1; break;
				case  2:	case   3:	CPU.WhichEvent = 2; break;
				case  4:	case   5:	CPU.WhichEvent = 3; break;
				case  6:	case   7:	CPU.WhichEvent = 4; break;
				case  8:	case   9:	CPU.WhichEvent = 5; break;
				case 10:	case  11:	CPU.WhichEvent = 6; break;
			}

			if (local_sa1) // FIXME
			{
				SA1.Cycles = SA1.PrevCycles = 0;
				SA1.TimerIRQLastState = FALSE;
				SA1.HTimerIRQPos = Memory.FillRAM[0x2212] | (Memory.FillRAM[0x2213] << 8);
				SA1.VTimerIRQPos = Memory.FillRAM[0x2214] | (Memory.FillRAM[0x2215] << 8);
				SA1.HCounter = 0;
				SA1.VCounter = 0;
				SA1.PrevHCounter = 0;
				SA1.MemSpeed = SLOW_ONE_CYCLE;
				SA1.MemSpeedx2 = SLOW_ONE_CYCLE * 2;
			}
		}

		CPU.Flags |= old_flags & (DEBUG_MODE_FLAG | TRACE_FLAG | SINGLE_STEP_FLAG | FRAME_ADVANCE_FLAG);
		ICPU.ShiftedPB = Registers.PB << 16;
		ICPU.ShiftedDB = Registers.DB << 16;
		S9xSetPCBase(Registers.PBPC);
		S9xUnpackStatus();
		S9xFixCycles();

		CPU.InDMA = CPU.InHDMA = FALSE;
		CPU.InDMAorHDMA = CPU.InWRAMDMAorHDMA = FALSE;
		CPU.HDMARanInDMA = 0;

		S9xFixColourBrightness();
		IPPU.ColorsChanged = TRUE;
		IPPU.OBJChanged = TRUE;
		IPPU.RenderThisFrame = TRUE;

		uint8 hdma_byte = Memory.FillRAM[0x420c];
		S9xSetCPU(hdma_byte, 0x420c);

		S9xControlPostLoadState(&ctl_snap);

		if (local_sa1 && local_sa1_registers)
		{
			SA1.Flags |= sa1_old_flags & TRACE_FLAG;
			S9xSA1PostLoadState();
		}

		if (Settings.SDD1)
			S9xSDD1PostLoadState();

			if (Settings.SPC7110)	S9xSPC7110PostLoadState(version);

		if (local_srtc)
			S9xSRTCPostLoadState(version);

		if (local_bsx_data)
			S9xBSXPostLoadState();

		if (local_msu1_data)
			S9xMSU1PostLoadState();


	if (local_registers)		delete [] local_registers;
	if (local_sa1)				delete [] local_sa1;
	if (local_sa1_registers)	delete [] local_sa1_registers;
	if (local_cx4_data)			delete [] local_cx4_data;
	if (local_st010)			delete [] local_st010;
	if (local_obc1)				delete [] local_obc1;
	if (local_obc1_data)		delete [] local_obc1_data;
	if (local_srtc)				delete [] local_srtc;
	if (local_rtc_data)			delete [] local_rtc_data;
	if (local_bsx_data)			delete [] local_bsx_data;

	return 0;
}

static int FreezeSize (int size, int type)
{
	switch (type)
	{
		case uint32_ARRAY_V:
		case uint32_INDIR_ARRAY_V:
			return (size * 4);

		case uint16_ARRAY_V:
		case uint16_INDIR_ARRAY_V:
			return (size * 2);

		default:
			return (size);
	}
}

static void FreezeStruct (STREAM stream, const char *name, void *base, FreezeData *fields, int num_fields)
{
	int	len = 0;
	int	i, j;

	for (i = 0; i < num_fields; i++)
	{
		if (SNAPSHOT_VERSION < fields[i].debuted_in)
		{
			fprintf(stderr, "%s[%p]: field has bad debuted_in value %d, > %d.", name, (void *) fields, fields[i].debuted_in, SNAPSHOT_VERSION);
			continue;
		}

		if (SNAPSHOT_VERSION < fields[i].deleted_in)
			len += FreezeSize(fields[i].size, fields[i].type);
	}

	uint8	*block = new uint8[len];
	uint8	*ptr = block;
	uint8	*addr;
	uint16	word;
	uint32	dword;
	int64	qaword;
	int		relativeAddr;

	for (i = 0; i < num_fields; i++)
	{
		if (SNAPSHOT_VERSION >= fields[i].deleted_in || SNAPSHOT_VERSION < fields[i].debuted_in)
			continue;

		addr = (uint8 *) base + fields[i].offset;

		// determine real address of indirect-type fields
		// (where the structure contains a pointer to an array rather than the array itself)
		if (fields[i].type == uint8_INDIR_ARRAY_V || fields[i].type == uint16_INDIR_ARRAY_V || fields[i].type == uint32_INDIR_ARRAY_V)
			addr = (uint8 *) (*((pint *) addr));

		// convert pointer-type saves from absolute to relative pointers
		if (fields[i].type == POINTER_V)
		{
			uint8	*pointer    = (uint8 *) *((pint *) ((uint8 *) base + fields[i].offset));
			uint8	*relativeTo = (uint8 *) *((pint *) ((uint8 *) base + fields[i].offset2));
			relativeAddr = pointer - relativeTo;
			addr = (uint8 *) &relativeAddr;
		}

		switch (fields[i].type)
		{
			case INT_V:
			case POINTER_V:
				switch (fields[i].size)
				{
					case 1:
						*ptr++ = *(addr);
						break;

					case 2:
						word = *((uint16 *) (addr));
						*ptr++ = (uint8) (word >> 8);
						*ptr++ = (uint8) word;
						break;

					case 4:
						dword = *((uint32 *) (addr));
						*ptr++ = (uint8) (dword >> 24);
						*ptr++ = (uint8) (dword >> 16);
						*ptr++ = (uint8) (dword >> 8);
						*ptr++ = (uint8) dword;
						break;

					case 8:
						qaword = *((int64 *) (addr));
						*ptr++ = (uint8) (qaword >> 56);
						*ptr++ = (uint8) (qaword >> 48);
						*ptr++ = (uint8) (qaword >> 40);
						*ptr++ = (uint8) (qaword >> 32);
						*ptr++ = (uint8) (qaword >> 24);
						*ptr++ = (uint8) (qaword >> 16);
						*ptr++ = (uint8) (qaword >> 8);
						*ptr++ = (uint8) qaword;
						break;
				}

				break;

			case uint8_ARRAY_V:
			case uint8_INDIR_ARRAY_V:
				memmove(ptr, addr, fields[i].size);
				ptr += fields[i].size;

				break;

			case uint16_ARRAY_V:
			case uint16_INDIR_ARRAY_V:
				for (j = 0; j < fields[i].size; j++)
				{
					word = *((uint16 *) (addr + j * 2));
					*ptr++ = (uint8) (word >> 8);
					*ptr++ = (uint8) word;
				}

				break;

			case uint32_ARRAY_V:
			case uint32_INDIR_ARRAY_V:
				for (j = 0; j < fields[i].size; j++)
				{
					dword = *((uint32 *) (addr + j * 4));
					*ptr++ = (uint8) (dword >> 24);
					*ptr++ = (uint8) (dword >> 16);
					*ptr++ = (uint8) (dword >> 8);
					*ptr++ = (uint8) dword;
				}

				break;
		}
	}

	FreezeBlock(stream, name, block, len);
	delete [] block;
}

static void FreezeBlock (STREAM stream, const char *name, uint8 *block, int size)
{
	char	buffer[20];

	// check if it fits in 6 digits. (letting it go over and using strlen isn't safe)
	if (size <= 999999)
		sprintf(buffer, "%s:%06d:", name, size);
	else
	{
		// to make it fit, pack it in the bytes instead of as digits
		sprintf(buffer, "%s:------:", name);
		buffer[6] = (unsigned char) ((unsigned) size >> 24);
		buffer[7] = (unsigned char) ((unsigned) size >> 16);
		buffer[8] = (unsigned char) ((unsigned) size >> 8);
		buffer[9] = (unsigned char) ((unsigned) size >> 0);
	}

	buffer[11] = 0;

	WRITE_STREAM(buffer, 11, stream);
	WRITE_STREAM(block, size, stream);
}

static int UnfreezeBlock (STREAM stream, const char *name, uint8 *block, int size)
{
	char	buffer[20];
	int		len = 0, rem = 0;
	long	rewind = FIND_STREAM(stream);

	size_t	l = READ_STREAM(buffer, 11, stream);
	buffer[l] = 0;

	if (l != 11 || strncmp(buffer, name, 3) != 0 || buffer[3] != ':')
	{
	err:
#ifdef DEBUGGER
		fprintf(stdout, "absent: %s(%d); next: '%.11s'\n", name, size, buffer);
#endif
		REVERT_STREAM(stream, FIND_STREAM(stream) - l, 0);
		return (WRONG_FORMAT);
	}

	if (buffer[4] == '-')
	{
		len = (((unsigned char) buffer[6]) << 24)
			| (((unsigned char) buffer[7]) << 16)
			| (((unsigned char) buffer[8]) << 8)
			| (((unsigned char) buffer[9]) << 0);
	}
	else
		len = atoi(buffer + 4);

	if (len <= 0)
		goto err;

	if (len > size)
	{
		rem = len - size;
		len = size;
	}

	memset(block, 0, size);

	if (READ_STREAM(block, len, stream) != len)
	{
		REVERT_STREAM(stream, rewind, 0);
		return (WRONG_FORMAT);
	}

	if (rem)
	{
		char	*junk = new char[rem];
		len = READ_STREAM(junk, rem, stream);
		delete [] junk;
		if (len != rem)
		{
			REVERT_STREAM(stream, rewind, 0);
			return (WRONG_FORMAT);
		}
	}

	return (SUCCESS);
}

static int UnfreezeBlockCopy (STREAM stream, const char *name, uint8 **block, int size)
{
	int	result;

	*block = new uint8[size];

	result = UnfreezeBlock(stream, name, *block, size);
	if (result != SUCCESS)
	{
		delete [] (*block);
		*block = NULL;
		return (result);
	}

	return (SUCCESS);
}

static int UnfreezeStruct (STREAM stream, const char *name, void *base, FreezeData *fields, int num_fields, int version)
{
	int		result;
	uint8	*block = NULL;

	result = UnfreezeStructCopy(stream, name, &block, fields, num_fields, version);
	if (result != SUCCESS)
	{
		if (block != NULL)
			delete [] block;
		return (result);
	}

	UnfreezeStructFromCopy(base, fields, num_fields, block, version);
	delete [] block;

	return (SUCCESS);
}

static int UnfreezeStructCopy (STREAM stream, const char *name, uint8 **block, FreezeData *fields, int num_fields, int version)
{
	int	len = 0;

	for (int i = 0; i < num_fields; i++)
	{
		if (version >= fields[i].debuted_in && version < fields[i].deleted_in)
			len += FreezeSize(fields[i].size, fields[i].type);
	}

	return (UnfreezeBlockCopy(stream, name, block, len));
}


static void UnfreezeStructFromCopy (void *sbase, FreezeData *fields, int num_fields, uint8 *block, int version)
{
	uint8	*ptr = block;
	uint16	word;
	uint32	dword;
	int64	qaword;
	uint8	*addr;
	void	*base;
	int		relativeAddr;
	int		i, j;

	for (i = 0; i < num_fields; i++)
	{
		if (version < fields[i].debuted_in || version >= fields[i].deleted_in)
			continue;

		base = (SNAPSHOT_VERSION >= fields[i].deleted_in) ? ((void *) &Obsolete) : sbase;
		addr = (uint8 *) base + fields[i].offset;

		if (fields[i].type == uint8_INDIR_ARRAY_V || fields[i].type == uint16_INDIR_ARRAY_V || fields[i].type == uint32_INDIR_ARRAY_V)
			addr = (uint8 *) (*((pint *) addr));

		switch (fields[i].type)
		{
			case INT_V:
			case POINTER_V:
				switch (fields[i].size)
				{
					case 1:
						if (fields[i].offset < 0)
						{
							ptr++;
							break;
						}

						*(addr) = *ptr++;
						break;

					case 2:
						if (fields[i].offset < 0)
						{
							ptr += 2;
							break;
						}

						word  = *ptr++ << 8;
						word |= *ptr++;
						*((uint16 *) (addr)) = word;
						break;

					case 4:
						if (fields[i].offset < 0)
						{
							ptr += 4;
							break;
						}

						dword  = *ptr++ << 24;
						dword |= *ptr++ << 16;
						dword |= *ptr++ << 8;
						dword |= *ptr++;
						*((uint32 *) (addr)) = dword;
						break;

					case 8:
						if (fields[i].offset < 0)
						{
							ptr += 8;
							break;
						}

						qaword  = (int64) *ptr++ << 56;
						qaword |= (int64) *ptr++ << 48;
						qaword |= (int64) *ptr++ << 40;
						qaword |= (int64) *ptr++ << 32;
						qaword |= (int64) *ptr++ << 24;
						qaword |= (int64) *ptr++ << 16;
						qaword |= (int64) *ptr++ << 8;
						qaword |= (int64) *ptr++;
						*((int64 *) (addr)) = qaword;
						break;

					default:
						assert(0);
						break;
				}

				break;

			case uint8_ARRAY_V:
			case uint8_INDIR_ARRAY_V:
				if (fields[i].offset >= 0)
					memmove(addr, ptr, fields[i].size);
				ptr += fields[i].size;

				break;

			case uint16_ARRAY_V:
			case uint16_INDIR_ARRAY_V:
				if (fields[i].offset < 0)
				{
					ptr += fields[i].size * 2;
					break;
				}

				for (j = 0; j < fields[i].size; j++)
				{
					word  = *ptr++ << 8;
					word |= *ptr++;
					*((uint16 *) (addr + j * 2)) = word;
				}

				break;

			case uint32_ARRAY_V:
			case uint32_INDIR_ARRAY_V:
				if (fields[i].offset < 0)
				{
					ptr += fields[i].size * 4;
					break;
				}

				for (j = 0; j < fields[i].size; j++)
				{
					dword  = *ptr++ << 24;
					dword |= *ptr++ << 16;
					dword |= *ptr++ << 8;
					dword |= *ptr++;
					*((uint32 *) (addr + j * 4)) = dword;
				}

				break;
		}

		if (fields[i].type == POINTER_V)
		{
			relativeAddr = (int) *((pint *) ((uint8 *) base + fields[i].offset));
			uint8	*relativeTo = (uint8 *) *((pint *) ((uint8 *) base + fields[i].offset2));
			*((pint *) (addr)) = (pint) (relativeTo + relativeAddr);
		}
	}
}



