SET(SOURCES
    lapi.c lcode.c lctype.c ldebug.c ldo.c 
    ldump.c lfunc.c lgc.c llex.c lmem.c 
    lobject.c lopcodes.c lparser.c lstate.c 
    lstring.c ltable.c ltm.c lundump.c lvm.c 
    lzio.c ltests.c lauxlib.c lbaselib.c 
    ldblib.c liolib.c lmathlib.c loslib.c 
    ltablib.c lstrlib.c lutf8lib.c loadlib.c 
    lcorolib.c linit.c lua.c
)

include_directories(.)

MAKE_PROGRAM(lua)

TARGET_LINK_LIBRARIES(lua m)
# nusaOS: disable dynamic readline (dlopen/dlsym tidak stabil di nusaOS).
# Tanpa ini, Lua REPL mencoba dlopen("libreadline.so") saat startup,
# menghasilkan pointer invalid → infinite bad-ptr spam di kernel log.
# LUA_READLINELIB="" membuat loadreadline() skip blok dlopen sepenuhnya.
target_compile_definitions(lua PRIVATE
    LUA_READLINELIB=""
)