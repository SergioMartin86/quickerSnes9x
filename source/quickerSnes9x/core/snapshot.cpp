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


static thread_local struct Obsolete
{
	uint8	CPU_IRQActive;
}	Obsolete;


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

	FreezeBlock(stream, "REG", (uint8_t*) &Registers, sizeof(Registers));

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
		FreezeBlock(stream, "SFX", (uint8_t*) &GSU, sizeof(GSU));
	}

	if (Settings.SA1)
	{
		S9xSA1PackStatus();
		FreezeBlock(stream, "SA1", (uint8_t*) &SA1, sizeof(SA1));
		FreezeBlock(stream, "SAR", (uint8_t*) &SA1Registers, sizeof(SA1Registers));
	}

	if (Settings.DSP == 1) FreezeBlock(stream, "DP1", (uint8_t*)&DSP1, sizeof(DSP1));
	if (Settings.DSP == 2) FreezeBlock(stream, "DP2", (uint8_t*)&DSP2, sizeof(DSP2));
	if (Settings.DSP == 4)FreezeBlock(stream, "DP4", (uint8_t*)&DSP4, sizeof(DSP4));
	if (Settings.C4)	FreezeBlock (stream, "CX4", Memory.C4RAM, 8192);
	if (Settings.SETA == ST_010)	FreezeBlock(stream, "ST0", (uint8_t*) &ST010, sizeof(ST010));

	if (Settings.OBC1)
	{
		FreezeBlock(stream, "OBC", (uint8_t*) &OBC1, sizeof(OBC1));
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
		FreezeBlock(stream, "SRT", (uint8_t*) &srtcsnap, sizeof(srtcsnap));
	}

	if (Settings.SRTC || Settings.SPC7110RTC) FreezeBlock (stream, "CLK", RTCData.reg, 20);
	if (Settings.BS) FreezeBlock(stream, "BSX", (uint8_t*)&BSX, sizeof(BSX));
	if (Settings.MSU1)	FreezeBlock(stream, "MSU", (uint8_t*)&MSU1, sizeof(MSU1));
}

int S9xUnfreezeFromStream (STREAM stream)
{

	int version = SNAPSHOT_VERSION;
	
struct SControlSnapshot	ctl_snap;

		UnfreezeBlock(stream, "CPU", (uint8_t*) &CPU, sizeof(CPU));
		
		UnfreezeBlock(stream, "REG", (uint8_t*)&Registers, sizeof(Registers));

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

		if (Settings.SA1)
		{
				uint32 sa1_old_flags = SA1.Flags;

    UnfreezeBlock(stream, "SA1", (uint8_t*)&SA1, sizeof(SA1));
		  UnfreezeBlock(stream, "SAR", (uint8_t*)&SA1Registers, sizeof(SA1Registers));

				SA1.Cycles = SA1.PrevCycles = 0;
				SA1.TimerIRQLastState = FALSE;
				SA1.HTimerIRQPos = Memory.FillRAM[0x2212] | (Memory.FillRAM[0x2213] << 8);
				SA1.VTimerIRQPos = Memory.FillRAM[0x2214] | (Memory.FillRAM[0x2215] << 8);
				SA1.HCounter = 0;
				SA1.VCounter = 0;
				SA1.PrevHCounter = 0;
				SA1.MemSpeed = SLOW_ONE_CYCLE;
				SA1.MemSpeedx2 = SLOW_ONE_CYCLE * 2;

				SA1.Flags |= sa1_old_flags & TRACE_FLAG;
			 S9xSA1PostLoadState();
		}
		
  if (Settings.DSP == 1) UnfreezeBlock(stream, "DP1", (uint8_t*)&DSP1, sizeof(DSP1));
		if (Settings.DSP == 2) UnfreezeBlock(stream, "DP2", (uint8_t*)&DSP2, sizeof(DSP2));
		if (Settings.DSP == 4) UnfreezeBlock(stream, "DP4", (uint8_t*)&DSP4, sizeof(DSP4));
		if (Settings.C4) UnfreezeBlock (stream, "CX4", (uint8_t*) Memory.C4RAM, 8192);
		if (Settings.SETA == ST_010) UnfreezeBlock(stream, "ST0", (uint8_t*) &ST010, sizeof(ST010));

		if (Settings.OBC1)
		{
    UnfreezeBlock(stream, "OBC", (uint8_t*)&OBC1, sizeof(OBC1));
	   UnfreezeBlock(stream, "OBM", Memory.OBC1RAM, 8192);
		}
		
	 if (Settings.SPC7110) UnfreezeBlock(stream, "S71", (uint8_t*)&s7snap, sizeof(s7snap));
  if (Settings.SRTC) UnfreezeBlock(stream, "SRT", (uint8_t*) &srtcsnap, sizeof(srtcsnap));
  if (Settings.SRTC || Settings.SPC7110RTC) UnfreezeBlock (stream, "CLK", (uint8_t*) RTCData.reg, 20);
		if (Settings.BS) UnfreezeBlock(stream, "BSX", (uint8_t*) &BSX, sizeof(BSX));
  if (Settings.MSU1) UnfreezeBlock(stream, "MSU", (uint8_t*) &MSU1, sizeof(MSU1));
		
		uint32 old_flags     = CPU.Flags;


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

		if (Settings.SDD1)	S9xSDD1PostLoadState();
		if (Settings.SPC7110)	S9xSPC7110PostLoadState(version);
		if (Settings.SRTC) S9xSRTCPostLoadState(version);
		if (Settings.BS)	S9xBSXPostLoadState();
		if (Settings.MSU1)	S9xMSU1PostLoadState();


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



