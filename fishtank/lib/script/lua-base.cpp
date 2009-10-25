#include "lua-base.hpp"
#include <cstring>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/format.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>
#include <framework/vfs.hpp>
#include <fstream>
#include <luabind/luabind.hpp>
#include <luabind/exception_handler.hpp>
#include <luabind/tag_function.hpp>
extern "C" {
#include <lua.h>
#include <lualib.h>
}
#include <string>

// FIXME: #includes from top level dir
#include "../../main.hpp"

using namespace luabind;
using namespace std;
namespace fs = boost::filesystem;
using boost::format;
using boost::system::error_code;
using boost::system::get_system_category;
using boost::system::system_error;

// Cannot use LuaBind to implement this function because LuaBind catches exceptions
static int os_exit (lua_State* L)
{
	throw exit_exception(luaL_optint(L, 1, EXIT_SUCCESS));
}

struct LoadF
{
	bool extraline;
	ifstream f;
	char buff[LUAL_BUFFERSIZE];
};

static const char* getF(lua_State*, LoadF* lf, size_t* size)
{
	if (lf->extraline)
	{
		lf->extraline = false;
		*size = 1;
		return "\n";
	}

	if (lf->f.eof())
		return NULL;

	lf->f.read(lf->buff, sizeof(lf->buff));
	*size = lf->f.gcount();
	return (*size > 0) ? lf->buff : NULL;
}

static void base_loadfile(lua_State* L, const fs::path& path)
{
	LoadF lf;
	const int n = lua_gettop(L);
	lf.extraline = false;
	vfs::open(lf.f, path);
	if (!lf.f.is_open())
		throw fs::filesystem_error("cannot open", path, error_code(errno, get_system_category()));

	int c = lf.f.get();
	if (c == '#')
	{  /* Unix exec. file? */
		lf.extraline = true;
		while ((c = lf.f.get()) != EOF && c != '\n') ;  /* skip first line */
		if (c == '\n')
			c = lf.f.get();
	}
	if (c == LUA_SIGNATURE[0])
	{  /* binary file? */
		lf.f.close();
		vfs::open(lf.f, path, ios_base::binary);  /* reopen in binary mode */
		if (!lf.f.is_open())
			throw fs::filesystem_error("cannot reopen", path, error_code(errno, get_system_category()));

		/* skip eventual `#!...' */
		while ((c = lf.f.get()) != EOF && c != LUA_SIGNATURE[0]) ;
		lf.extraline = false;
	}
	lf.f.putback(c);
	string fname((format("@%s") % path).str());
	const int status = lua_load(L, (const char* (*)(lua_State*, void*, size_t*)) &getF, &lf, fname.c_str());
	if (lf.f.bad())
	{
		lua_settop(L, n);  /* ignore results from `lua_load' */
		throw fs::filesystem_error("cannot read", path, error_code(errno, get_system_category()));
	}
	if (status != 0)
		throw luabind::error(L);
}

static int base_dofile(lua_State* L)
{
	const int argc = lua_gettop(L);
	const char* fname = luaL_checkstring(L, 1);

	lua_pushvalue(L, lua_upvalueindex(1)); // loadfile

	// Find out in what file our caller lives
	lua_Debug ar;
	if (lua_getstack(L, 1, &ar)
	 && lua_getinfo(L, "S", &ar)
	 && ar.short_src
	 && ar.short_src[0])
	{
		fs::path file(ar.short_src);
		file = file.parent_path() / fname;
		if (fs::is_symlink(file))
		{
			file = normalized_path(readlink(file));
			// Strip references to the current working directory (to make Lua related debug messages cleaner)
			while (file.relative_path().string().substr(0, 2) == "./")
				file = file.string().substr(2);
		}

		lua_pushstring(L, file.string().c_str());
	}
	else
	{
		lua_pushvalue(L, 1); // file (parameter)
	}

	lua_call(L, 1, 1);
	lua_call(L, 0, LUA_MULTRET); // Call the Lua chunck produced by loadfile
	return lua_gettop(L) - argc;
}

void lua_base_register_with_lua(lua_State* L)
{
	/*
	 * Functions that need to be ditched or overridden for safety:
	 *  * dofile
	 *  * loadfile
	 *  * print        -- overridden by debug_register_with_lua
	 *  * io.input
	 *  * io.lines
	 *  * io.open
	 *  * io.output
	 *  * io.popen
	 *  * os.execute
	 *  * os.exit
	 *  * os.getenv
	 *  * os.remove
	 *  * os.rename
	 *  * os.setlocale
	 */

	module(L)
	[
		def("loadfile", tag_function<void (lua_State*, const char*)>(&base_loadfile), raw(_1))
	];

	lua_getglobal(L, "loadfile");
	lua_pushcclosure(L, &base_dofile, 1);
	lua_setglobal(L, "dofile");

	lua_getglobal(L, "os");
	lua_pushcfunction(L, &os_exit);
	lua_setfield(L, -2, "exit");
	lua_pop(L, 1); // pop the 'os' table
}
