//#include "stdafx.h"

#define _WINDOWS
// #define DLLEXPORT __declspec(dllexport)

#include "nylon-runner.h"
#include "psbind-unmanwrap.h"

namespace {
    class ShellWrapper
    {
        std::unique_ptr<PshellUnmanWrap> pshell_;
    public:
        luabind::object invoke( lua_State* l, const std::string& cmd )
        {
            return pshell_->invoke( l, cmd );
        }
        ShellWrapper()
        : pshell_( CreatePshellUnmanWrap() )
        {
        };

        void do_invoke_loop( lua_State* l, const std::string& cmd, ThreadReporter reporter )
        {
            pshell_->invoke_threadreporter( l, cmd, reporter );
        }

        StdFunctionCaller
        cthread_invoke( lua_State* l, const std::string& cmd )
        {
            return StdFunctionCaller( std::bind( &ShellWrapper::do_invoke_loop, this, l, cmd, std::placeholders::_1 ) );
        }
        
    };
}



extern "C" DLLEXPORT  int luaopen_psbind( lua_State* L )
{
   using namespace luabind;

//   std::cout << "Lua open psbind " << std::endl;

   // open( L ); // wow, don't do this from a coroutine.  make sure the main prog inits luabind.
   // also, dont do this after somebody else has done it, at least with newer versions of luabind; it replaces the
   // table of all registered classes.

    module( L, "Psbind" ) [
        class_<ShellWrapper>("Shell")
        .def(constructor<>() )
        .def("invoke",&ShellWrapper::invoke)
        .def("cthread_invoke",&ShellWrapper::cthread_invoke)
    ];

   return 0;
}

