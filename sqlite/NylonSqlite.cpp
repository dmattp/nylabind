/*****************************************************************************
     NylonSqlite.cpp
*******************************************************************************/
//#define   LUA_EMBED_NylonSqlite_CPP__
#if _WINDOWS
# include "StdAfx.h"
#endif

#include "nylon-runner.h"
#include <sstream>

// #include "SilkySqlite.h"
// #include "lib/lua_embed/silky.h"

#include <stdio.h>
#ifdef _WINDOWS
#define lrint(x) static_cast<int>(x) // (floor(x+(x>0) ? 0.5 : -0.5))
#else
# include <sys/errno.h>
#endif

#include <string.h>
// #include "lib/nlog/errno.h"
#include <sqlite3.h>
#include <vector>
// #include "pure/annotation.h"

#define THROW_NOTATED(n) throw std::runtime_error( n )
#define ANNOTATE(n) (void)0
#define nlog(...) (void)0,

namespace {


      struct Any {
         unsigned luaType;
         
         union {
            double number;
            bool   boolean;
            struct {
               unsigned length;
               char     val[sizeof(unsigned)]; // might as well have it size of unsigned; MSVC complains about 0.
            } string;
         };

         //////////////////////////////////////////////////////////////////
         
         static Any* New( const luabind::object& value )
         {
            using namespace luabind;
            
            if( luabind::type(value) == LUA_TSTRING )
            {
               std::string s( object_cast<std::string>( value ) ); //
               // luabind:value.to_str() );
               
               unsigned len = s.length();
               unsigned alloc = (sizeof(Any)+len+3) % (~3);
               
               Any* p = (Any*)malloc( alloc );

//               nlog(extra) "Got string of len: ", len, " alloc: ", alloc;

               p->string.length = len;
               
               p->luaType = LUA_TSTRING;

               memcpy( p->string.val, s.c_str(), len );
               
//                nlog() "(Any thing: ", p, ")";
               return p;
            }
            else
            {
               if
               (  luabind::type(value) == LUA_TNUMBER
               || luabind::type(value) == LUA_TBOOLEAN
               )
               {
                  Any* p = (Any*)malloc( ((sizeof(Any)+3)%(~3)) );
                  if( luabind::type(value) == LUA_TNUMBER )
                  {
                     p->luaType = LUA_TNUMBER;
                     p->number = object_cast<double>( value );
//                     nlog(extra) "Got any number=", p->number;
                  }
                  else
                  {
                     p->luaType = LUA_TBOOLEAN;
                     p->boolean = object_cast<bool>( value );
                  }
//                   nlog() "(Any thing: ", p, ")";
                  return p;
               }
               else
                  THROW_NOTATED("Value must be string, number, or bool.");
            }
         }
            
         static void Delete( Any* old ) {
            free( old );
         }
      };

      std::vector<Any*>*
      toCppFromOptBindingParams( const luabind::object& params )
      {
         using namespace luabind;
         if( type(params) == LUA_TTABLE )
         {
            nlog(extra) "have params";
            std::vector<Any*>* vparams( new std::vector<Any*>() );
            // Lua::TableRef t = params.value();
//             for( typeof(t.begin_ordinal()) it = t.begin_ordinal();
//                  it != t.end_ordinal();
//                  ++it )
            for( iterator it( params ), end; it != end; ++it )
            {
               nlog(extra) "got a param";
               Any* newAny = Any::New( *it );
               vparams->push_back( newAny );
            }
            return vparams;
         }
         else if( type(params) == LUA_TNIL  )
         {
            return 0;
         }
         else
         {
            std::vector<Any*>* vparams( new std::vector<Any*>() );
            Any* newAny = Any::New( params );
            vparams->push_back( newAny );
            return vparams;
         }
      }

       void freeParams( std::vector<Any*>* params )
       {
          if( params )
          {
             for( auto it = params->begin(); it != params->end(); ++it )
                Any::Delete( *it );
             delete params;
          }
       }

    
    
   
    sqlite3_stmt* db_prep_statement ( sqlite3* pDb, const std::string& statement, const char* from )
       {
           sqlite3_stmt* stmt;
           int rc =
               sqlite3_prepare_v2
               (   pDb,
                   statement.c_str(),
                   statement.length(),
                   &stmt,
                   0
               );

           if( rc != SQLITE_OK )
           {
               std::ostringstream os;
               os << "Error prepping exec stmt from=" << from << " rc=" << rc;
               os << " emsg=" << sqlite3_errmsg( pDb );
               os << " statement=" << statement;
               THROW_NOTATED(os.str());
           }

           return stmt;
       }

         
      void
      bind_any( sqlite3_stmt* stmt, int pndx, Any* any )
      {
         int rc = SQLITE_OK;
         
         if( any->luaType == LUA_TNUMBER )
         {
            rc = sqlite3_bind_int( stmt, pndx, lrint(any->number) );
         }
         else if( any->luaType == LUA_TBOOLEAN )
         {
            rc = sqlite3_bind_int( stmt, pndx, any->boolean?1:0 );
         }
         else if( any->luaType == LUA_TSTRING )
         {
//             std::cout << "bind string, len=" << any->string.length << std::endl;
             rc = sqlite3_bind_text( stmt, pndx,
               any->string.val, any->string.length,
               SQLITE_TRANSIENT );
         }
         else
            THROW_NOTATED("Cant bind param for lua type="), any->luaType;

         if( rc != SQLITE_OK )
         {
             std::ostringstream oss;
             oss << "Error binding stmt=" << (void*)stmt << " param=" << pndx << "; rc=" << rc << " type=" << any->luaType;
             THROW_NOTATED(oss.str());
         }
      }
    
       void
       bind_all_params
       (   sqlite3_stmt* stmt,
           std::vector<Any*>* params
       )
       {
           sqlite3_reset(stmt);
           if( params )
           {
               int pndx = 1;
               for( auto it = params->begin();
                    it != params->end();
                    ++it )
               {
                   bind_any( stmt, pndx, *it );
                   ++pndx;
               }
           }
       }

      int
      do_db_exec
      (  sqlite3* pDb,
          sqlite3_stmt* stmt,
         std::vector<Any*>* params
      )
      {
         bind_all_params( stmt, params );

         int rc = sqlite3_step( stmt );

         nlog(extra) "sqlite exec rc=", rc;

         return rc;
      }
    
      void
      do_db_exec
      (  sqlite3* pDb,
          const std::string& statement,
         std::vector<Any*>* params
      )
      {
//         std::cout << "enter do_exec" << std::endl;
         

         //nlog(debug) "SilkySqlite::select_single=", statement;
          sqlite3_stmt* stmt = db_prep_statement( pDb, statement, __FUNCTION__ );

          int rc = do_db_exec( pDb, stmt, params );

         sqlite3_finalize( stmt );
         
         if( rc != SQLITE_DONE )
         {
             // std::cout << "Sqlite Exec Failed, rc=" << rc << ": " << sqlite3_errmsg(db_) << std::endl;
            THROW_NOTATED("Sqlite Exec Failed, rc="), rc, ": ", sqlite3_errmsg(pDb);
         }

//         std::cout << "SilkySqlite::exec FINISHED okay, statement=" << statement << std::endl;
       }
    

    class PreparedStatement;
   class SilkySqlite
   {
       friend class PreparedStatement;
      sqlite3* db_;
      std::function<void()> itemReady_;
      char** curColumns_;
      char** curValues_;
      int    curCount_;

   public:
      SilkySqlite( const std::string& dbname )
      : curColumns_( 0 ),
        curValues_( 0 ),
        curCount_( 0 )
      {
//         std::cout << "construct SilkySqlite, db=" << dbname << std::endl;
         
         int rc = sqlite3_open( dbname.c_str(), &db_ );
         if( rc != SQLITE_OK )
         {
            const std::string err( sqlite3_errmsg( db_ ) );
            sqlite3_close( db_ );
            THROW_NOTATED( std::string("Fail opening database=") + err );
         }
         nlog(prime) "creating SilkySqlite object";
      }
   private:

      char*
      dupit( const char* src )
      {
         int n = strlen( src ) + 1;
         char* newOne = (char*)malloc( n );
         memcpy( newOne, src, n );
         return newOne;
      }

      void
      freeCurrentColumns()
      {
         for( int i = 0; i < curCount_; ++i )
         {
            free( curColumns_[i] );
            if( curValues_[i] )
               free( curValues_[i] );
         }
         free( curColumns_ );
         free( curValues_ );
      }

      // called in mainloop context
      // via Silky::IoThread::itemReadyFunction() mechanism
#if 0      
      void row_avail( const luabind::object& item_cb )
      {
         // ANNOTATE("Calling SilkySimpq event callback fun");
         nlog(debug,noisy) "got sqlite row";

         item_cb( lastRow() ); //

         nprintlog( (debug,noisy), "passed item back" );
      }
#endif 

      void
      setupResults( int ncols )
      {
         if( curCount_ )
            freeCurrentColumns();

         curCount_   = ncols;
         curColumns_ = (char**)malloc( sizeof(char*) * ncols );
         curValues_  = (char**)malloc( sizeof(char*) * ncols );
      }

      int
      selectCallback( int argc, char** argv, char** azColName )
      {
         setupResults( argc );
         
         for( int i = 0; i < argc; ++i )
         {
            // nlog(debug) "col=", azColName[i], " val=", argv[i];
            curColumns_[i] = dupit( azColName[i] );

            // NOTE! Value can NULL, eg when NULLs are in database!
            curValues_[i] = argv[i] ? dupit(argv[i]) : NULL;
         }

         itemReady_();

         return 0;
      }
         
      static int
      select_callback( void* arg, int argc, char** argv, char** azColName )
      {
         SilkySqlite* this1 = static_cast<SilkySqlite*>( arg );
         return this1->selectCallback( argc, argv, azColName );
      }
      
      //////////////////////////////////////////////////////////////////
      // this approach waits for the lua coroutines to specifically indicate
      // when the last item has been processed as a separate step.
#if 0      
      void
      select_x_loopitylooploopbad
      (  const std::string& statement,
         std::vector<Any*>* params,
         ThreadReporter r
      )
      {
         nlog(debug) "SilkySqlite::select_x_loop=", statement, " params=", (void*)params;

         // dmp/12.0531: sqlite3_exec method doesn't support binding params,
         // so params should not be used with the current implementation of
         // this function!
         if( params )
         {
//            ASSERTZ("Should not pass params to sqlite3 selectMany");
            int pndx = 1;
            for( auto it = params->begin();
                 it != params->end();
                 ++it )
            {
               // bind_any( stmt, pndx, *it );
               Any::Delete( *it );
               ++pndx;
            }
            delete params;
         }

         itemReady_ =
            thread.itemReadyFunction
            (  std::bind( &SilkySqlite::row_avail, this, _1 )
            );

         nprintlog( (debug), "SilkySimpq::event_loop() entering loop" );

         char* zErrorMessage;
         
         int rc =
            sqlite3_exec
            (  db_,
               statement.c_str(),
               select_callback,
               this,
               &zErrorMessage
            );

         if( rc != SQLITE_OK )
         {
            const std::string err( zErrorMessage );
            sqlite3_free( zErrorMessage );
            THROW_NOTATED("Error running SQL="), statement,
               ": ", err;
         }
         
         nlog(debug) "SilkySqlite:: FINISHED select_loop=", statement;
      }
#endif 


      void
      select_loop_reporter
      (  const std::string& statement,
         std::vector<Any*>* params,
         ThreadReporter r
      )
      {
         ANNOTATE("SilkySqlite::select_loop_reporter="), statement;
         
         // std::cout << "SilkySqlite::select_loop=" << statement << std::endl;
         sqlite3_stmt* stmt = prep_statement( statement, __FUNCTION__ );

         if( params )
         {
            int pndx = 1;
            for( auto it = params->begin();
                 it != params->end();
                 ++it )
            {
               bind_any( stmt, pndx, *it );
               Any::Delete( *it );
               ++pndx;
            }
            delete params;
         }

         int rc = sqlite3_step( stmt );

         nlog(extra,db) "sqlite rc=", rc;

         while( rc == SQLITE_ROW )
         {
            r.preReport();

//            std::cout << "WOKE UP AFTER preReport()!!" << std::endl;
         
            luabind::object c = luabind::newtable(r.interpreter()); // @todo
         
            int nColumns = sqlite3_column_count( stmt );

            for( int i = 0; i < nColumns; ++i )
            {
               std::string cname = (char*)sqlite3_column_name( stmt, i );
               char* result = (char*)sqlite3_column_text( stmt, i );
               int ctype = sqlite3_column_type( stmt, i );
               int clen = sqlite3_column_bytes( stmt, i );
//                std::cout << "got column=" << cname << " value=" << (result?result:"NULL")
//                          << " type=" << ctype << "clen=" << clen << std::endl;
               if( result )
               {
                   c[ cname ] = std::string(result,clen);
               }
            }

//            std::cout << "set all columns count=" << nColumns << std::endl;

            r.access()( c );

            r.postReport();

            rc = sqlite3_step( stmt );
         }

         nlog(extra,db) "SilkySqlite:: FINISHED cols=", curColumns_, " select_single=", statement;
         sqlite3_finalize( stmt );
      }

       typedef std::pair<std::string,std::string> column_t;
       typedef std::vector< column_t > row_t;

      void
      select_loop
      (   const std::string& statement,
          std::vector<Any*>* params,
          int maxVals,
          std::function<void(row_t*)> callback
      )
      {
         ANNOTATE("SilkySqlite::select_single="), statement;
         
//         std::cout << "SilkySqlite::select_loop=" << statement << std::endl;

         sqlite3_stmt* stmt = prep_statement( statement, __FUNCTION__ );

         bind_all_params( stmt, params );

         int rc = sqlite3_step( stmt );

         nlog(extra,db) "sqlite rc=", rc;

         while( rc == SQLITE_ROW )
         {
             // luabind::object c = luabind::newtable(r.interpreter()); //
             // @todo
             row_t row;
         
             int nColumns = sqlite3_column_count( stmt );

             for( int i = 0; i < nColumns; ++i )
             {
                 char* result = (char*)sqlite3_column_text( stmt, i );
                 if( result )
                 {
                     int ctype = sqlite3_column_type( stmt, i );
                     int clen = sqlite3_column_bytes( stmt, i );
                     std::string cname = (char*)sqlite3_column_name( stmt, i );
                     
//                std::cout << "z01 got column=" << cname << " value=" << (result?result:"NULL")
//                          << " type=" << ctype << "clen=" << clen << std::endl;
                     
                     row.push_back( column_t( cname, std::string(result,clen) ) );
                 }
             }

             callback( &row );

             if( --maxVals <= 0 )
                 break;

             rc = sqlite3_step( stmt );
         }

         nlog(extra,db) "SilkySqlite:: FINISHED cols=", curColumns_, " select_single=", statement;
         sqlite3_finalize( stmt );
      } // end, select_loop
       

//       void
//       select_single
//       (  const std::string& statement,
//          std::vector<Any*>* params,
//          ThreadReporter r
//       )
//       {
//          ANNOTATE("SilkySqlite::select_single="), statement;
         
//          std::cout << "SilkySqlite::select_single=" << statement << std::endl;

//          sqlite3_stmt* stmt;
//          int rc = sqlite3_prepare/*_v2*/( db_, statement.c_str(),
//             statement.length(),
//             &stmt, 0 );

//          if( rc != SQLITE_OK )
//             THROW_NOTATED("Error prepping stmt, rc="), rc;

//          if( params )
//          {
//             int pndx = 1;
//             for( auto it = params->begin();
//                  it != params->end();
//                  ++it )
//             {
//                bind_any( stmt, pndx, *it );
//                Any::Delete( *it );
//                ++pndx;
//             }
//             delete params;
//          }

//          rc = sqlite3_step( stmt );

//          nlog(extra,db) "sqlite rc=", rc;

//          if( rc != SQLITE_ROW )
//             THROW_NOTATED("Could not get row, rc="), rc, ": ", sqlite3_errmsg(db_);

//          r.preReport();

//          std::cout << "WOKE UP AFTER preReport()!!" << std::endl;
         
//          luabind::object c = luabind::newtable(r.interpreter()); // @todo
         
//          int nColumns = sqlite3_column_count( stmt );

//          for( int i = 0; i < nColumns; ++i )
//          {
//             std::string cname = (char*)sqlite3_column_name( stmt, i );
//             char* result = (char*)sqlite3_column_text( stmt, i );
//             std::cout << "got column=" << cname << " value=" << (result?result:"NULL") << std::endl;
//             if( result )
//                c[ cname ] = std::string(result);
//          }

//          std::cout << "set all columns count=" << nColumns << std::endl;
         
// //          luabind::object c = luabind::newtable(0); // @todo

// //          for( int i = 0; i < curCount_; ++i )
// //          {
// //             if( curValues_[i] )
// //                c[curColumns_[i]] = curValues_[i];
// //          }

// //          // return c;
         
//          r.access()( c );

//          r.postReport();

//          sqlite3_finalize( stmt );

//          nlog(extra,db) "SilkySqlite:: FINISHED cols=", curColumns_, " select_single=", statement;

//       }

       sqlite3_stmt* prep_statement ( const std::string& statement, const char* from )
       {
           return db_prep_statement( db_, statement, from );
       }
       
       
      void
      do_exec
      (  const std::string& statement,
         std::vector<Any*>* params
      )
      {
         ANNOTATE("SilkySqlite::exec="), statement;
         do_db_exec( db_, statement, params );
      }
      
      
   public:
      ~SilkySqlite()
      {
         sqlite3_close( db_ );
      }

      int
      lastRowId()
      {
         return sqlite3_last_insert_rowid( db_ );
      }

#if 1
      luabind::object
      lastRow()
      {
         luabind::object c = luabind::newtable(0); // @todo

         for( int i = 0; i < curCount_; ++i )
         {
            if( curValues_[i] )
               c[curColumns_[i]] = curValues_[i];
         }

         return c;
      }
#endif 



#if 0
      StdFunctionCaller
      bgSelectMany( const std::string& selectStatement, const luabind::object& params )
      {
//         nprintlog( (debug), "SilkySimpq::process_events() setting iterator"
//         );
          std::cout << "NylonSqlite: selectMany stmt=" << selectStatement << std::endl;
         
         return
            StdFunctionCaller
            (  std::bind
               (  &SilkySqlite::select_loop,
                  this, selectStatement,
                  toCppFromOptBindingParams(params),
                  std::placeholders::_1
               )
            );
      }

      StdFunctionCaller
      selectOne
      (
         const std::string& selectStatement, const luabind::object& params
      )
      {
//          nlog(extra,db) "SilkySimpq::selectOne() params.isNil?=", params.isNil(),
//             " stmt=", selectStatement;

         return
            StdFunctionCaller
            (  std::bind
               (  &SilkySqlite::select_single,
                  this, selectStatement,
                  toCppFromOptBindingParams(params),
                  std::placeholders::_1
               )
            );
      }
#endif 

      static void
      selectManyReport
      (
          luabind::object* cbfun,
          row_t* row
      )
      {
          luabind::object rowTable = luabind::newtable( cbfun->interpreter() ); // @todo
            
          for( auto it = row->begin(); it != row->end(); ++it )
              rowTable[ it->first ] = it->second;

          (*cbfun)( rowTable );
      }

#if 0       
      StdFunctionCaller
      bgExec( const std::string& statement, const luabind::object& params )
      {
//          nlog(debug) "SilkySimpq::exec() params.isNil?=", (luabind::type(params)==LUA_TNIL),
//             " stmt=", statement;

         std::cout << "Create sqlite exec caller for stmt=" << statement << std::endl;
         return
            StdFunctionCaller
            (  std::bind
               (  &SilkySqlite::do_exec,
                  this, statement,
                  toCppFromOptBindingParams(params),
                  std::placeholders::_1
               )
            );
      }
#endif 

       
      void
      selectMany
      (
          const luabind::object& cbfun,
          const std::string& selectStatement,
          const luabind::object& params
      )
      {
          auto copied = toCppFromOptBindingParams(params);
          std::unique_ptr<luabind::object> cb( new luabind::object( cbfun ) );
          this->select_loop
          (
              selectStatement, copied, INT_MAX,
              std::bind( &selectManyReport, cb.get(), std::placeholders::_1 )
          );
          if( copied )
             freeParams( copied );
      }

      void
      selectOne
      (
          const luabind::object& cbfun,
          const std::string& selectStatement,
          const luabind::object& params
      )
      {
          auto copied = toCppFromOptBindingParams(params);
          std::unique_ptr<luabind::object> cb( new luabind::object( cbfun ) );
          this->select_loop
          (
              selectStatement, copied, 1,
              std::bind( &selectManyReport, cb.get(), std::placeholders::_1 )
          );
          freeParams( copied );
      }
       
       
       void
       exec( const std::string& statement, const luabind::object& params )
       {
//          nlog(debug) "SilkySimpq::exec() params.isNil?=", (luabind::type(params)==LUA_TNIL),
//             " stmt=", statement;
          auto copied = toCppFromOptBindingParams(params);
          try {
              do_exec( statement, copied );
          } catch( ... ) {
              freeParams( copied );
              throw;
          }
          freeParams( copied );
      }
       
      
   };  // end class NylonSqlite

    class PreparedStatement
    {
        sqlite3* db_;
        sqlite3_stmt* stmt_;
    public:
        PreparedStatement( SilkySqlite& silkydb, const std::string& stmt )
        : db_( silkydb.db_ ),
          stmt_( db_prep_statement( db_, stmt, __FUNCTION__ ) )
        {
        }
        
       void
       exec( const luabind::object& params )
       {
#if 1
          auto copied = toCppFromOptBindingParams(params);
          try {
              do_db_exec( db_, stmt_, copied );
          } catch( ... ) {
              freeParams( copied );
              throw;
          }
          freeParams( copied );
#endif 
       }
    };
    

   
//    AUTOLOAD_LUA_REGISTER_CLASS( SilkySqlite,
//       (selectMany)
//       (selectOne)
//       (lastRow)      (lastRowId)
//       (exec)
//                               );
}



#ifdef _WINDOWS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif 

extern "C" DLLEXPORT  int luaopen_NylonSqlite( lua_State* L )
{
   using namespace luabind;

//   std::cout << "open NylonSqlite" << std::endl;

   // open( L ); // wow, don't do this from a coroutine.  make sure the main prog inits luabind.

//   std::cout << "NylonSqlite open.01a" << std::endl;

   module( L ) [
      class_<SilkySqlite>("NylonSqlite")
      .def( constructor<const std::string&>() )
      // .def( "exec", &SilkySqlite::exec )
      .def( "lastRowId", &SilkySqlite::lastRowId )
//      .def( "bgSelectMany", &SilkySqlite::bgSelectMany )
//      .def( "bgExec", &SilkySqlite::bgExec )

      //////////////////////////////////////////////////////////////////
      .def( "selectMany", &SilkySqlite::selectMany )
      .def( "selectOne", &SilkySqlite::selectOne )
      .def( "exec", &SilkySqlite::exec ),

      class_<PreparedStatement>("SqlitePreparedStatement")
      .def( constructor<SilkySqlite&, const std::string&>() )
      .def( "exec", &PreparedStatement::exec )
   ];

//   std::cout << "done opening NylonSqlite" << std::endl;
   return 0;
}
