# OPL2 music player (MUS -> MIDI -> DOSBox OPL emulator), taken from prboom-go
set(COMPONENT_SRCDIRS ".")
set(COMPONENT_ADD_INCLUDEDIRS ".")
register_component()
rg_setup_compile_options(-O2 -Wno-error=incompatible-pointer-types -Wno-error=maybe-uninitialized)
