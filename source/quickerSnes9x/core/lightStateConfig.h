#pragma once 

struct optionalBlocks_t
{
size_t _stateRAMSize = 0x20000;
bool _enablePPUBlock = true;
bool _enableDMABlock = true;
bool _enableVRABlock = true;
bool _enableRAMBlock = true;
bool _enableSRABlock = true;
bool _enableFILBlock = true;
bool _enableSNDBlock = true;
bool _enableCTLBlock = true;
bool _enableTIMBlock = true;
};