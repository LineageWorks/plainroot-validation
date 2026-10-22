extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdio>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s MODULE_PATH SCRIPT_PATH\n", argv[0]);
    return 2;
  }

  lua_State *state = luaL_newstate();
  if (!state) {
    std::fprintf(stderr, "unable to create Lua state\n");
    return 2;
  }
  luaL_openlibs(state);
  lua_pushstring(state, argv[1]);
  lua_setglobal(state, "PLAINROOT_VALIDATION_MODULE");

  const int result = luaL_dofile(state, argv[2]);
  if (result != LUA_OK) {
    const char *message = lua_tostring(state, -1);
    std::fprintf(stderr, "Lua adapter test failed: %s\n",
                 message ? message : "unknown Lua error");
  }
  lua_close(state);
  return result == LUA_OK ? 0 : 1;
}
