
#include <apu.h>

__thread bool _enablePPUBlock = true;
__thread bool _enableDMABlock = true;
__thread bool _enableVRABlock = true;
__thread bool _enableRAMBlock = true;
__thread bool _enableSRABlock = true;
__thread bool _enableFILBlock = true;
__thread bool _enableSNDBlock = true;
__thread bool _enableCTLBlock = true;
__thread bool _enableTIMBlock = true;