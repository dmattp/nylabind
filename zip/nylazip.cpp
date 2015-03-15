



#if _WINDOWS
# include "stdafx.h"
# include <WTypes.h>
#endif

#include "nylon-runner.h"

#include <iostream>
#include <stdexcept>
#include <luabind/luabind.hpp>

#include "zip.h"


namespace {

    class Archive
    {
        std::string filename_;
        zip* arc_;
    public:
        Archive( const std::string& arFilename )
        : filename_( arFilename )
        {
            int error;
            arc_ = zip_open( arFilename.c_str(), 0, &error );
            if (!arc_)
                throw std::runtime_error( std::string("Could not open zip file=")+arFilename );
        }

        luabind::object stat_index(lua_State* l, int ndx)
        {
            struct zip_stat stat;
            
            const int rc = zip_stat_index( arc_, ndx, 0, &stat );
            
            if (rc != 0 )
                throw std::runtime_error( "Could not stat index" );
            
            luabind::object t = luabind::newtable(l);
            
            if (stat.valid & ZIP_STAT_NAME)
                t["name"] = stat.name;
            if (stat.valid & ZIP_STAT_SIZE)
                t["size"] = stat.size;
            if (stat.valid & ZIP_STAT_MTIME)
                t["mtime"] = stat.mtime;
            if (stat.valid & ZIP_STAT_COMP_SIZE)
                t["comp_size"] = stat.comp_size;
            
            return t;
        }

        int num_entries()
        {
            return static_cast<int>( zip_get_num_entries( arc_, 0 ) );
        }

        friend class File;
    };

    class File
    {
        zip_file* file_;
    public:
        File( Archive& arc, int ndx )
        {
            file_ = zip_fopen_index( arc.arc_, ndx, 0 );
        }

#if 0        
        std::string
        read( int nbytes )
        {
            char* buf=static_cast<char*>( alloca(nbytes) );
            int read = static_cast<int>(zip_fread( file_, buf, nbytes ));
            if (read >= 0)
                return std::string( buf, read );
            else
                return std::string(); // error
        }
#else
        // this function may be called frequently, so the overhead of using
        // std::string (with a constructor/string new+delete for the string)
        // can really add up in time critical functions.
        void
        read( lua_State* L, int nbytes )
        {
            char* buf=static_cast<char*>( alloca(nbytes) );
            int read = static_cast<int>(zip_fread( file_, buf, nbytes ));
            if (read >= 0)
                lua_pushlstring( L, buf, read );
            else
                lua_pushlstring( L, buf, 0 );
        }
#endif 
        void close()
        {
            zip_fclose( file_ );
            file_ = 0;
        }
    };
}


#ifdef _WINDOWS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif


extern "C" DLLEXPORT  int luaopen_nylazip( lua_State* L )
{
   using namespace luabind;

   // open( L ); // wow, don't do this from a coroutine.  make sure the main prog inits luabind.
   // also, dont do this after somebody else has done it, at least with newer versions of luabind; it replaces the
   // table of all registered classes.

   module( L, "Zip" ) [
       class_<Archive>("Archive")
       .def( constructor<const std::string&>() )
       .def( "stat_index", &Archive::stat_index )
       .def( "num_entries", &Archive::num_entries ),

       class_<File>("File")
       .def( constructor<Archive&, int>() )
       .def("read", &File::read )
       .def("close", &File::close )
  ];

   return 0;
}
