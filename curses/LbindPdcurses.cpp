

#if _WINDOWS
# include "stdafx.h"
# include <WTypes.h>
#endif

#include "nylon-runner.h"

# include "curses.h"

#include <iostream>
#include <luabind/luabind.hpp>




namespace {

    class Window;

    class Color
    {
        friend class Window;
        int pair_;
    public:
        Color( int p, int c1, int c2 )
        : pair_( p )
        {
            init_pair( p, c1, c2 );
        }
    };
    
    class Window
    {
        WINDOW* w_;
    public:
        Window( int h, int w, int y, int x) 
        {
            w_ = newwin( h, w, y, x );
        }

        Window()
        : w_( stdscr )
        {
        }
        
        Window( WINDOW* w )
        : w_( w )
        {}

        /* doing this because I need a way to force a window
         * to go away e.g., for popups?  I hate it, because it leaves the
         * object in an invalid state, but until I know how to hide a window
         * without calling delwin it's all i've got. */
        void forcedelete()
        {
            if( w_  && w_ != stdscr )
            {
                delwin( w_ );
                w_ = 0;
            }
        }

        ~Window()
        {
//            std::cout << "Destroyed window w=" << (void*)w_ << std::endl;
            this->forcedelete();
        }
        

        int gety() { int x; int y; getyx(w_,y,x); return y; }
        int getx() { int x; int y; getyx(w_,y,x); return x; }
       
        int getmaxx_() { return getmaxx(w_); }
        int getmaxy_() { return getmaxy(w_); }
       
        void printw( const std::string& t ) { wprintw( w_, t.c_str() ); }
        void mvprintw( int y, int x, const std::string& t ) { mvwprintw( w_, y, x, t.c_str() ); }
        void addstr_( const std::string& t ) { waddnstr( w_, t.c_str(), t.length() ); }
        void mvaddstr_( int y, int x, const std::string& t ) { mvwaddnstr( w_, y, x, t.c_str(), t.length() ); }
        void refresh() { wrefresh( w_ ); }
        void move( int y, int x ) { wmove( w_, y, x ); }
        std::string getstr_() {
            char str[8192];
            wgetnstr( w_, str, sizeof(str)-1 );
            str[sizeof(str)-1] = '\0';
            return str;
        }
        std::string mvgetstr__( int y, int x ) {
            char str[8192];
            mvwgetnstr( w_, y, x, str, sizeof(str)-1 );
            str[sizeof(str)-1] = '\0';
            return str;
        }
        void border_( char ls, char rs, char ts, char bs, char tl, char tr, char bl, char br )
        {
            wborder( w_, ls, rs, ts, bs, tl, tr, bl, br );
        }
        void stdbox_()
        {
            box( w_, ACS_VLINE, ACS_HLINE );
        }
        void box_( int v, int h )
        {
            box( w_, v, h );
        }
        void clrtoeol_()
        {
            wclrtoeol( w_ );
        }
        void clear_() { wclear( w_ ); }

        void attron_( Color* c )  {
            // init_pair( 1, COLOR_BLUE, COLOR_YELLOW );
            // wattron(w_, COLOR_PAIR(1) );
            wattron(w_, COLOR_PAIR(c->pair_) );
        }
        void attron_( int attr )  {
            // init_pair( 1, COLOR_BLUE, COLOR_YELLOW );
            // wattron(w_, COLOR_PAIR(1) );
            wattron(w_, attr );
        }
        void attroff_( Color* c ) {
            //wattroff(w_, COLOR_PAIR(1) );
            wattroff(w_, COLOR_PAIR(c->pair_) );
        }
        void attroff_( int attr ) {
            //wattroff(w_, COLOR_PAIR(1) );
            wattroff(w_, attr);
        }
        void attr_set_( Color* c, int attr ) 
        {
            wattr_set(w_, attr, COLOR_PAIR(c->pair_), (void *)0);
        }
        void vline_( chtype c, int len )
        {
            wvline( w_, c, len );
        }
        void hline_( chtype c, int len )
        {
            whline( w_, c, len );
        }
        void redraw() { redrawwin(w_); }
        void mvwin( int row, int col) { ::mvwin(w_, row, col); }
        void resize( int lines, int col) { wresize(w_, lines, col); }
    }; // end, class Window

    Window* initscr_()
    {
        WINDOW* w = ::initscr();
        refresh();
        return new Window( w );
    }

    int getch_()
    {
        return wgetch(stdscr);
    }
    void keypad_( bool yesno )
    {
        keypad(stdscr, yesno);
    }

    class Key{};

    class Lines{};
    

    void do_getch_loop( ThreadReporter reporter )
    {
//        std::cout << "infinitethreadness" << std::endl;
        while (true)
        {
            auto c = wgetch(stdscr);
            reporter.report(c);
        }
//        std::cout << "exit infinitethreadness" << std::endl;
    }

    StdFunctionCaller
    cthread_getch_loop()
    {
        return StdFunctionCaller( std::bind( &do_getch_loop, std::placeholders::_1 ) );
    }

    int color_pairs()
    {
        return COLOR_PAIRS;
    }

   // dmp/141005: On linux, lots of these are implemented
   // as macros, so I have to write a distinct function to
   // create a binding.
   void beep_()    { beep();    }
   void flash_()   { flash();   }   
   void refresh_() { refresh(); }
   void endwin_()  { endwin();  }
   void clear_()   { clear();   }
   void start_color_()   { start_color();   }   
   
} // end, namespace anonymous



#ifdef _WINDOWS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

// extern "C" void* luabind_deboostified_allocator( void* context, void const* ptr, size_t sz )
// {
//     return realloc(const_cast<void*>(ptr), sz);
// }


extern "C" DLLEXPORT  int luaopen_LbindPdcurses( lua_State* L )
{
   using namespace luabind;

   // std::cout << "Nylon open Pdcurses" << std::endl;

   //luabind::allocator = luabind_deboostified_allocator;
   
//   luabind::open( L ); // wow, don't do this from a coroutine.  make sure the
                       // main prog inits luabind.
   
   // also, dont do this after somebody else has done it, at least with newer versions of luabind; it replaces the
   // table of all registered classes.

   module( L, "Pdcurses" ) [
       namespace_("Static") [
           def("initscr",&initscr_)
#if _WINDOWS
           ,def("mouse_on",&::mouse_on)
#endif
           ,def("endwin",     &endwin_)
           ,def("refresh",    &refresh_)
           ,def("curs_set",   &::curs_set)
           ,def("getch",      &getch_)
           ,def("noecho",     &::noecho)
           ,def("noraw",      &::noraw)
           ,def("nocbreak",   &::nocbreak)
           ,def("cbreak",   &::cbreak)
           ,def("raw",        &::raw)
           ,def("clear",      &clear_)
           ,def("keypad",     &keypad_)
           ,def("beep",       &beep_)
           ,def("flash",      &flash_)
           ,def("start_color",&start_color_)
           ,def("color_pairs",&color_pairs)
           ,def("cthread_getch_loop",&::cthread_getch_loop)           
       ],
       class_<Key>("key") 
           .enum_("constants") [

               value("dc",     KEY_DC),    // delete key
               value("up",     KEY_UP),
               value("down",   KEY_DOWN),
               value("npage",  KEY_NPAGE),
               value("ppage",  KEY_PPAGE),
               value("end",    KEY_END),
               value("home",   KEY_HOME),
               value("enter",  KEY_ENTER),
               value("left",   KEY_LEFT),
               value("right",  KEY_RIGHT),
               value("mouse",  KEY_MOUSE),               
               value("backspace", KEY_BACKSPACE),
               value("sleft",  KEY_SLEFT),
               value("sright", KEY_SRIGHT),
               value("send",   KEY_SEND),
               value("shome",  KEY_SHOME),
#if _WINDOWS               
               value("sup",    KEY_SUP),
               value("sdown",  KEY_SDOWN),
               value("alt_del",   ALT_DEL ),       
               value("alt_ins",   ALT_INS),       
               value("alt_tab",   ALT_TAB),       
               value("alt_home",  ALT_HOME),      
               value("alt_pgup",  ALT_PGUP),      
               value("alt_pgdn",  ALT_PGDN),      
               value("alt_end",   ALT_END),       
               value("alt_up",    ALT_UP),        
               value("alt_down",  ALT_DOWN),      
               value("alt_right", ALT_RIGHT),     
               value("alt_left",  ALT_LEFT),      
               value("alt_enter", ALT_ENTER),     
               value("alt_esc",   ALT_ESC),       
               value("alt_bksp",  ALT_BKSP),       
               value("alt_left",  KEY_ALT_L),       
               value("alt_right", KEY_ALT_R),       
               value("ctl_del",   CTL_DEL),       
               value("ctl_ins",   CTL_INS),
               value("ctl_tab",   CTL_TAB),       
               value("ctl_up",    CTL_UP),        
               value("ctl_down",  CTL_DOWN),      
               value("ctl_enter", CTL_ENTER),       
               value("ctl_left",  CTL_LEFT),       
               value("ctl_right", CTL_RIGHT),       
               value("ctl_pgup",  CTL_PGUP),       
               value("ctl_pddn",  CTL_PGDN),       
               value("ctl_home",  CTL_HOME),       
               value("ctl_end",   CTL_END),
#endif                
               value("f0", KEY_F0)
           ],
       class_<Lines>("Lines") 
           .enum_("constants") [
               value("vline", ACS_VLINE),
               value("hline", ACS_HLINE),
               value("ulcorner", ACS_ULCORNER),
               value("urcorner", ACS_URCORNER),
               value("llcorner", ACS_LLCORNER),
               value("lrcorner", ACS_LRCORNER)
           ],
       class_<Color>("Color")
       .def( constructor<int,int,int>() )
       .enum_("constants") [
           value("red", COLOR_RED),
           value("blue", COLOR_BLUE),
           value("green", COLOR_GREEN),
           value("black", COLOR_BLACK),
           value("white", COLOR_WHITE),
           value("cyan", COLOR_CYAN), // and cyan is sort of a blue green, I guess, not what I think of as cyan
           value("magenta", COLOR_MAGENTA), // magenta is like, dark blue?
           value("yellow", COLOR_YELLOW), //  yellow just seems to be bold white.

           value("a_bold", A_BOLD), 
           value("a_dim", A_DIM), 
           value("a_reverse", A_REVERSE),
           value("a_blink", A_BLINK)
       ],
       class_<Window>("Window")
       .def( constructor<int,int,int,int>() )
       .def( constructor<>() ) // get window for stdscr
       .def( "getx",        &Window::getx )
       .def( "gety",        &Window::gety )
       .def( "getmaxx",     &Window::getmaxx_ )
       .def( "getmaxy",     &Window::getmaxy_ )
       .def( "printw",      &Window::printw )
       .def( "mvprintw",    &Window::mvprintw )
       .def( "refresh",     &Window::refresh )
       .def( "move",        &Window::move )
       .def( "addstr",      &Window::addstr_ )
       .def( "mvaddstr",    &Window::mvaddstr_ )
       .def( "getstr",      &Window::getstr_ )
       .def( "mvgetstr",    &Window::mvgetstr__ )
       .def( "border",      &Window::border_ )
       .def( "stdbox_",     &Window::stdbox_ )
       .def( "box",     &Window::box_ )
       .def( "clear",       &Window::clear_)
       .def( "clrtoeol",    &Window::clrtoeol_)
       .def( "attron",      (void (Window::*)(int))&Window::attron_)
       .def( "attron",      (void (Window::*)(Color*))&Window::attron_)
       .def( "attroff",      (void (Window::*)(int))&Window::attroff_)
       .def( "attroff",      (void (Window::*)(Color*))&Window::attroff_)
       .def( "attr_set",    &Window::attr_set_)
       .def( "forcedelete", &Window::forcedelete)
       .def( "hline",       &Window::hline_)
       .def( "vline",       &Window::vline_)
       .def( "redraw",      &Window::redraw)
       .def( "mvwin",       &Window::mvwin)
       .def( "resize",      &Window::resize)
   ];
   
   //std::cout << "Nylon Pdcurses opened" << std::endl;
   return 0;
}
