#include "zmq.hpp"


#if _WINDOWS
# include "stdafx.h"
# include <WTypes.h>
#endif

#include "nylon-runner.h"


#include <iostream>
#include <luabind/luabind.hpp>




namespace {

    void do_getch_loop( ThreadReporter reporter )
    {
        while (true)
        {
//             auto c = wgetch(stdscr);
//             reporter.report(c);
        }
    }

    StdFunctionCaller
    cthread_getch_loop()
    {
        return StdFunctionCaller( std::bind( &do_getch_loop, std::placeholders::_1 ) );
    }

    int blowme_()
    {
        return 0;
    }
   
} // end, namespace anonymous


using namespace zmq;

class Socket : public socket_t
{
public:
    Socket( context_t* pContext, int theType )
    : socket_t( *pContext, theType )
    {
    }

    void setsocketopt_( int option, const std::string& value )
    {
        this->setsockopt( option, value.c_str(), value.length() );
    }

    void connect_( const std::string& addr )
    {
        socket_t::connect(addr);
    }

    void sendflags_( const std::string& msg, int flags = 0 )
    {
        // zmq::message_t request( msg.c_str(), msg.length() );
        // memcpy( request.data(), msg.c_str(), msg.length() );
        this->send( msg.c_str(), msg.length(), flags );
    }
    
    void send_( const std::string& msg )
    {
        this->sendflags_( msg );
    }

    void sendnowait_( const std::string& msg )
    {
        this->sendflags_( msg, ZMQ_DONTWAIT );
    }
    
    void sendmore_( const std::string& msg )
    {
        this->sendflags_( msg, ZMQ_SNDMORE );
    }
    

    void bind_( const std::string& msg )
    {
        this->bind( msg );
    }
    
    void do_ctsend_( const std::string& msg, ThreadReporter ) { this->sendnowait_( msg ); }
    StdFunctionCaller ctsend_( const std::string& msg ) { return StdFunctionCaller( std::bind( &Socket::do_ctsend_, this, msg, std::placeholders::_1 ) ); }

    void do_ctsendnowait_( const std::string& msg, ThreadReporter ) { this->send_( msg ); }
    StdFunctionCaller ctsendnowait_( const std::string& msg ) { return StdFunctionCaller( std::bind( &Socket::do_ctsendnowait_, this, msg, std::placeholders::_1 ) ); }
    
    std::string getreply_()
    {
        zmq::message_t reply;
        this->recv(&reply);
        return std::string( (const char*)reply.data(), reply.size() );
    }

    bool getreplynowait_( std::string& rc )
    {
        zmq::message_t reply;
        const bool gotmsg = this->recv(&reply, ZMQ_DONTWAIT);
        if (gotmsg)
            rc = std::string( (const char*)reply.data(), reply.size() );
        return gotmsg;
    }


    void do_ctgetreply_( ThreadReporter r )
    {
        const std::string& rc = this->getreply_();
        r.report( rc );
    }
    
    void do_ctgetreplynowait_( ThreadReporter r )
    {
        std::string rc;
        const bool gotmsg = this->getreplynowait_(rc);
        if (gotmsg)
            r.report( rc );
        else
            r.report( false );
    }
    
    StdFunctionCaller ctgetreply_()
    {
        return StdFunctionCaller( std::bind( &Socket::do_ctgetreply_, this, std::placeholders::_1 ) );
    }
    
    StdFunctionCaller ctgetreplynowait_()
    {
        return StdFunctionCaller( std::bind( &Socket::do_ctgetreplynowait_, this, std::placeholders::_1 ) );
    }
    
};



#ifdef _WINDOWS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif



extern "C" DLLEXPORT  int luaopen_zmqnylon( lua_State* L )
{
   using namespace luabind;

   // std::cout << "Nylon open Pdcurses" << std::endl;

   // open( L ); // wow, don't do this from a coroutine.  make sure the main prog inits luabind.
   // also, dont do this after somebody else has done it, at least with newer versions of luabind; it replaces the
   // table of all registered classes.

   module( L, "Zmq" ) [
//        namespace_("Static") [
//         aa  def("blowme",&blowme_)
//        ],
       
       class_<zmq::context_t>("Context")
       .def( constructor<int>() ),
       
       class_<Socket>("Socket")
       .def( constructor<zmq::context_t*, int>() )
       .def( "connect", &Socket::connect_ )
       .def( "setsocketopt", &Socket::setsocketopt_ )
       .def( "send", &Socket::send_ )
       .def( "sendnowait", &Socket::sendnowait_ )
       .def( "sendmore", &Socket::sendmore_ )
       .def( "ctsend", &Socket::ctsend_ )
       .def( "ctsendnowait", &Socket::ctsendnowait_ )
       .def( "recv", &Socket::getreply_ )
       // .def( "recvnowait", &Socket::getreplynowait_ )
       .def( "bind", &Socket::bind_ )
       .def( "close", &Socket::close )
       .def( "ctrecv", &Socket::ctgetreply_ )
       .def( "ctrecvnowait", &Socket::ctgetreplynowait_ )
       .enum_("type") [
           value("pair", ZMQ_PAIR),
           value("pub", ZMQ_PUB),
           value("sub", ZMQ_SUB),
           value("req", ZMQ_REQ),
           value("rep", ZMQ_REP),
           value("dealer", ZMQ_DEALER),
           value("router", ZMQ_ROUTER),
           value("pull", ZMQ_PULL),
           value("push", ZMQ_PUSH),
           value("xpub", ZMQ_XPUB),
           value("xsub", ZMQ_XSUB),
           value("subscribe", ZMQ_SUBSCRIBE),
           value("stream", ZMQ_STREAM)
       ]
    ];
       
//           .enum_("constants") [
//                value("sup",    KEY_SUP),
//                value("sdown",  KEY_SDOWN),
//                value("alt_del",   ALT_DEL ),       
//                value("alt_ins",   ALT_INS),       
//                value("alt_tab",   ALT_TAB),       
//                value("alt_home",  ALT_HOME),      
//                value("alt_pgup",  ALT_PGUP),      
//           ],
//        class_<Color>("Color")
//        .def( constructor<int,int,int>() )
//        .enum_("constants") [
//            value("red", COLOR_RED),
//            value("blue", COLOR_BLUE),
//            value("green", COLOR_GREEN),
//            value("black", COLOR_BLACK),
//            value("white", COLOR_WHITE),
//            value("cyan", COLOR_CYAN), // and cyan is sort of a blue green, I guess, not what I think of as cyan
//            value("magenta", COLOR_MAGENTA), // magenta is like, dark blue?
//            value("yellow", COLOR_YELLOW), //  yellow just seems to be bold white.

//            value("a_bold", A_BOLD), 
//            value("a_dim", A_DIM), 
//            value("a_reverse", A_REVERSE),
//            value("a_blink", A_BLINK)
//        ],
//        class_<Window>("Window")
//        .def( constructor<int,int,int,int>() )
//        .def( constructor<>() ) // get window for stdscr
//        .def( "getx",        &Window::getx )
//        .def( "gety",        &Window::gety )
//        .def( "getmaxx",     &Window::getmaxx_ )
//        .def( "getmaxy",     &Window::getmaxy_ )
//        .def( "printw",      &Window::printw )
//        .def( "mvprintw",    &Window::mvprintw )
//        .def( "refresh",     &Window::refresh )
//        .def( "move",        &Window::move )
//        .def( "addstr",      &Window::addstr_ )
//        .def( "mvaddstr",    &Window::mvaddstr_ )
//        .def( "getstr",      &Window::getstr_ )
//        .def( "mvgetstr",    &Window::mvgetstr__ )
//        .def( "border",      &Window::border_ )
//        .def( "stdbox_",     &Window::stdbox_ )
//        .def( "box",     &Window::box_ )
//        .def( "clear",       &Window::clear_)
//        .def( "clrtoeol",    &Window::clrtoeol_)
//        .def( "attron",      (void (Window::*)(int))&Window::attron_)
//        .def( "attron",      (void (Window::*)(Color*))&Window::attron_)
//        .def( "attroff",      (void (Window::*)(int))&Window::attroff_)
//        .def( "attroff",      (void (Window::*)(Color*))&Window::attroff_)
//        .def( "attr_set",    &Window::attr_set_)
//        .def( "forcedelete", &Window::forcedelete)
//        .def( "hline",       &Window::hline_)
//        .def( "vline",       &Window::vline_)
//        .def( "redraw",      &Window::redraw)
//        .def( "mvwin",       &Window::mvwin)
//        .def( "resize",      &Window::resize)
   
   //std::cout << "Nylon Pdcurses opened" << std::endl;
   return 0;
}
