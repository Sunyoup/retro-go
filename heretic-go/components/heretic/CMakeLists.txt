set(COMPONENT_SRCDIRS ".")
set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_REQUIRES "retro-go")
register_component()

# The original was built with Watcom /j (signed char) and /ei (int enums)
rg_setup_compile_options(
    -fsigned-char
    -w
    # 1996 C: implicit int, implicit declarations, loose pointer types
    -Wno-error=implicit-int
    -Wno-error=implicit-function-declaration
    -Wno-error=incompatible-pointer-types
    -Wno-error=int-conversion
    -Wno-error=return-mismatch
    "-DSAVEGAMENAME=\"/sd/retro-go/saves/heretic/hticsav\""
    -O2
)
