# quickerSnes9x Core sources

quickerSnes9xSrc =  [
  'core/apu/apu.cpp',
  'core/apu/bapu/dsp/sdsp.cpp',
  'core/apu/bapu/dsp/SPC_DSP.cpp',
  'core/apu/bapu/smp/smp.cpp',
  'core/apu/bapu/smp/smp_state.cpp',
  'core/bsx.cpp',
  'core/c4.cpp',
  'core/c4emu.cpp',
  'core/cheats.cpp',
  'core/cheats2.cpp',
  'core/clip.cpp',
  'core/conffile.cpp',
  'core/controls.cpp',
  'core/cpu.cpp',
  'core/cpuexec.cpp',
  'core/cpuops.cpp',
  'core/crosshairs.cpp',
  'core/dma.cpp',
  'core/dsp.cpp',
  'core/dsp1.cpp',
  'core/dsp2.cpp',
  'core/dsp3.cpp',
  'core/dsp4.cpp',
  'core/fxinst.cpp',
  'core/fxemu.cpp',
  'core/gfx.cpp',
  'core/globals.cpp',
  'core/logger.cpp',
  'core/memmap.cpp',
  'core/msu1.cpp',
  'core/movie.cpp',
  'core/obc1.cpp',
  'core/ppu.cpp',
  'core/stream.cpp',
  'core/sa1.cpp',
  'core/sa1cpu.cpp',
  'core/screenshot.cpp',
  'core/sdd1.cpp',
  'core/sdd1emu.cpp',
  'core/seta.cpp',
  'core/seta010.cpp',
  'core/seta011.cpp',
  'core/seta018.cpp',
  'core/snapshot.cpp',
  'core/snes9x.cpp',
  'core/spc7110.cpp',
  'core/srtc.cpp',
  'core/tile.cpp',
  'core/filter/2xsai.cpp',
  'core/filter/blit.cpp',
  'core/filter/epx.cpp',
  'core/filter/hq2x.cpp',
  'core/filter/snes_ntsc.c',
  'core/statemanager.cpp',
  'core/unix/unix.cpp',
  'core/unix/x11.cpp',
]

quickerSnes9xIncludeDirs = [
  '.',
  'core',
  'core/unix',
  'core/apu',
  'core/apu/bapu',
  'core/filter', 
]

quickerSnes9xCompileFlags = [
    '-DNO_DUP_MAIN',
    '-DHAVE_MKSTEMP',
    '-DHAVE_STRINGS_H',
    '-DHAVE_SYS_IOCTL_H',
    '-DHAVE_STDINT_H',
    '-DRIGHTSHIFT_IS_SAR',
    '-DUSE_XVIDEO',
    '-DNOSOUND'
]

# quickerSnes9x Core Configuration

 quickerSnes9xDependency = declare_dependency(
  compile_args        : [ ],
  include_directories : include_directories(quickerSnes9xIncludeDirs),
  sources             : [ quickerSnes9xSrc ]
 )


