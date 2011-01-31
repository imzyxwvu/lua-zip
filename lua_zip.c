#include <lauxlib.h>
#include <lua.h>
#include <zip.h>
#include <assert.h>
#include <errno.h>

#define ARCHIVE_MT "zip{archive}"

#define check_archive(L, narg)                                   \
    ((struct zip**)luaL_checkudata((L), (narg), ARCHIVE_MT))

/* If zip_error is non-zero, then push an appropriate error message
 * onto the top of the Lua stack and return zip_error.  Otherwise,
 * just return 0.
 */
static int S_push_error(lua_State* L, int zip_error, int sys_error) {
    char buff[1024];
    if ( 0 == zip_error ) return 0;

    int len = zip_error_to_str(buff, sizeof(buff), zip_error, sys_error);
    if ( len >= sizeof(buff) ) len = sizeof(buff)-1;
    lua_pushlstring(L, buff, len);

    return zip_error;
}

static int S_archive_open(lua_State* L) {
    const char*  path  = luaL_checkstring(L, 1);
    int          flags = (lua_gettop(L) < 2) ? 0 : luaL_checkint(L, 2); 
    struct zip** ar    = (struct zip**)lua_newuserdata(L, sizeof(struct zip*));
    int          err   = 0;

    *ar = zip_open(path, flags, &err);

    if ( ! *ar ) {
        assert(err);
        S_push_error(L, err, errno);
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }

    luaL_getmetatable(L, ARCHIVE_MT);
    assert(!lua_isnil(L, -1)/* ARCHIVE_MT found? */);

    lua_setmetatable(L, -2);

    return 1;
}

/* Explicitly close the archive, throwing an error if there are any
 * problems.
 */
static int S_archive_close(lua_State* L) {
    struct zip** ar  = check_archive(L, 1);
    int          err;

    if ( ! *ar ) return 0;

    err = zip_close(*ar);
    *ar = NULL;

    if ( S_push_error(L, err, errno) ) lua_error(L);


    return 0;
}

/* Try to revert all changes and close the archive since the archive
 * was not explicitly closed.
 */
static int S_archive_gc(lua_State* L) {
    struct zip** ar = check_archive(L, 1);

    if ( ! *ar ) return 0;

    zip_unchange_all(*ar);
    zip_close(*ar);

    *ar = NULL;

    return 0;
}

static int S_archive_get_num_files(lua_State* L) {
    struct zip** ar = check_archive(L, 1);

    if ( ! *ar ) return 0;

    lua_pushinteger(L, zip_get_num_files(*ar));

    return 1;
}

static int S_archive_name_locate(lua_State* L) {
    struct zip** ar    = check_archive(L, 1);
    const char*  fname = luaL_checkstring(L, 2);
    int          flags = (lua_gettop(L) < 3) ? 0 : luaL_checkint(L, 3);
    int          idx;

    if ( ! *ar ) return 0;

    idx = zip_name_locate(*ar, fname, flags);

    if ( idx < 0 ) {
        int zip_err, sys_err;
        zip_error_get(*ar, &zip_err, &sys_err);
        lua_pushnil(L);
        S_push_error(L, zip_err, sys_err);
        return 2;
    }

    lua_pushinteger(L, idx+1);
    return 1;
}

static void S_register_archive(lua_State* L) {
    luaL_newmetatable(L, ARCHIVE_MT);

    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, S_archive_close);
    lua_setfield(L, -2, "close");

    lua_pushcfunction(L, S_archive_gc);
    lua_setfield(L, -2, "__gc");

    lua_pushcfunction(L, S_archive_get_num_files);
    lua_setfield(L, -2, "get_num_files");

    lua_pushcfunction(L, S_archive_get_num_files);
    lua_setfield(L, -2, "__len");

    lua_pushcfunction(L, S_archive_name_locate);
    lua_setfield(L, -2, "name_locate");

    lua_pop(L, 1);
}

static int S_OR(lua_State* L) {
    int result = 0;
    int top = lua_gettop(L);
    while ( top ) {
        result |= luaL_checkint(L, top--);
    }
    lua_pushinteger(L, result);
    return 1;
}

LUALIB_API int luaopen_zip(lua_State* L) {
    static luaL_reg fns[] = {
        { "open",     S_archive_open },
        { "OR",       S_OR },
        { NULL, NULL }
    };

    lua_newtable(L);
    luaL_register(L, NULL, fns);

    lua_pushnumber(L, ZIP_CREATE);
    lua_setfield(L, -2, "CREATE");

    lua_pushnumber(L, ZIP_EXCL);
    lua_setfield(L, -2, "EXCL");

    lua_pushnumber(L, ZIP_CHECKCONS);
    lua_setfield(L, -2, "CHECKCONS");

    lua_pushnumber(L, ZIP_FL_NOCASE);
    lua_setfield(L, -2, "FL_NOCASE");

    lua_pushnumber(L, ZIP_FL_NODIR);
    lua_setfield(L, -2, "FL_NODIR");

    S_register_archive(L);

    return 1;
}