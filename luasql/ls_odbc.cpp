/*
** LuaSQL, ODBC driver
** Authors: Pedro Rabinovitch, Roberto Ierusalimschy, Diego Nehab
** Tomas Guisasola
**   Substantially reworked by Matt Placek to work with Luabind+Nylon,
**   so don't blame the above authors for my hacky changes.
** See Copyright Notice in license.html
** $Id: ls_odbc.c,v 1.39 2009/02/07 23:16:23 tomas Exp $
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#include <sqlext.h>
#elif defined(INFORMIX)
#include "infxcli.h"
#elif defined(UNIXODBC)
#include "sql.h"
#include "sqltypes.h"
#include "sqlext.h"
#endif

extern "C" {
 #include "lua.h"
 #include "lauxlib.h"
 #include "luasql.h"
}

#include "nylon-runner.h"
#include <luabind/luabind.hpp>



/* we are lazy */
#define hENV SQL_HANDLE_ENV
#define hSTMT SQL_HANDLE_STMT
#define hDBC SQL_HANDLE_DBC
#define error(a) ((a) != SQL_SUCCESS && (a) != SQL_SUCCESS_WITH_INFO)


class Connection;
class Cursor;
class Environment;


#include <stdexcept>
class DmpSqlError : public std::runtime_error
{
public:
    DmpSqlError(const std::string& msg) : std::runtime_error("DmpSqlError: " + msg)
    {}
};


class SqlErrorTosser_
{
    static std::string fail(const SQLSMALLINT type, const SQLHANDLE handle) {
        SQLCHAR State[6];
        SQLINTEGER NativeError;
        SQLSMALLINT MsgSize, i;
        SQLRETURN ret;
        SQLCHAR Msg[SQL_MAX_MESSAGE_LENGTH];
        std::string msg;
        
        i = 1;
        while (1) {
            ret = SQLGetDiagRec(type, handle, i, State, &NativeError, Msg, 
                sizeof(Msg), &MsgSize);
            if (ret == SQL_NO_DATA) break;
            msg += std::string( (char*)Msg, MsgSize ) + '\n';
            i++;
        }
        return msg;
    }
    
    
    void check( SQLRETURN ret ) const
    {
        if (error(ret))
        {
            char msg[1024];
            
            sprintf( msg, "line=%d rc=%d %s", this->line_, ret, (msg_ ? msg_ : "") );
            
            if (type_ == -1)
                throw DmpSqlError( msg );
            else
            {
                std::string m = std::string(msg) + "\n";
                m += this->fail( type_, handle_ );
                std::cout << "FAIL:" << m << std::endl;
                throw DmpSqlError( m );
            }
        }
    }

    int line_;
    SQLSMALLINT type_;
    SQLHANDLE handle_;
    const char* msg_;
    
public:
    SqlErrorTosser_& operator=( SQLRETURN ret )
    {
        this->check(ret);
        return *this;
    }
    
    SqlErrorTosser_(int line )
    : line_(line),
      type_(-1),
      msg_(nullptr)
    {}

    SqlErrorTosser_(int line, const char* msg )
    : line_(line),
      type_(-1),
      msg_( msg )
    {}
    
    SqlErrorTosser_(int line, SQLHSTMT h)
    : line_(line),
      type_(hSTMT),
      handle_(h),
      msg_(nullptr)
    {}

    SqlErrorTosser_(int line, int type, SQLHANDLE h)
    : line_(line),
      type_(type),
      handle_(h),
      msg_(nullptr)
    {}
    
    SqlErrorTosser_( int line, SQLRETURN ret )
    : line_(line),
      type_(-1),
      msg_(nullptr)
    {
        this->check( ret );
    }
};

#define SqlErrorTosser(...) SqlErrorTosser_( __LINE__, ##__VA_ARGS__ )


class RaiiSqlHandle
{
    SQLHSTMT hstmt_;
public:
    RaiiSqlHandle( SQLHDBC hdbc )
    {
        SqlErrorTosser(hDBC,hdbc) = SQLAllocHandle( hSTMT, hdbc, &hstmt_ );
    }
    ~RaiiSqlHandle()
    {
        if( hstmt_ )
            SQLFreeHandle( hSTMT, hstmt_ );
    }
    void release() { hstmt_ = 0; }

    operator SQLHSTMT() { return hstmt_; }
};


//LUASQL_API int luaopen_luasql_odbc (lua_State *L);
class Environment
{
public:
    struct env_data {
        short      closed;
        int        conn_counter;
        SQLHENV    henv;               /* environment handle */
    };

    void plusConnection()
    {
        env_.conn_counter++;
    }
    
    Environment()
    {
        SqlErrorTosser() = SQLAllocHandle(hENV, SQL_NULL_HANDLE, &env_.henv);
        
        SQLRETURN ret = SQLSetEnvAttr (env_.henv, SQL_ATTR_ODBC_VERSION, 
            (void*)SQL_OV_ODBC3, 0);

        SqlErrorTosser("error setting SQL version.") = (ret);

        // @todo: proper RAII on henv, or smart/resource freeing pointer in object?
        // what are the rules on calling destructor on exception in constructor; it
        // doesn't get done, right??

//         if (error(ret)) {
//             SQLFreeHandle (hENV, henv);
//         }
        
        env_.closed = 0;
        env_.conn_counter = 0;
    }

    SQLHENV henv() { return env_.henv; }

    void close()
    {
#if 0        
        SQLRETURN ret;
//         env_data *env = (env_data *)luaL_checkudata(L, 1, LUASQL_ENVIRONMENT_ODBC);
//         luaL_argcheck (L, env != NULL, 1, LUASQL_PREFIX"environment expected");
        if (env->closed) {
            return;
        }
        
        if (env->conn_counter > 0)
            throw std::runtime_error("there are open connections");

        this->closed = 1;
        ret = SQLFreeHandle (hENV, env->henv);
        if (error(ret)) {
            int ret = fail(L, hENV, env->henv);
            env->henv = NULL;
            return ret;
        }
        return pass(L);
#endif 
    }

private:
    env_data env_;
    bool closed;
}; // end, class Environment



/*
** Fails with error message from ODBC
** Inputs: 
**   type: type of handle used in operation
**   handle: handle used in operation
*/
static int fail(lua_State *L,  const SQLSMALLINT type, const SQLHANDLE handle) {
    SQLCHAR State[6];
    SQLINTEGER NativeError;
    SQLSMALLINT MsgSize, i;
    SQLRETURN ret;
    SQLCHAR Msg[SQL_MAX_MESSAGE_LENGTH];
    luaL_Buffer b;
    lua_pushnil(L);

    luaL_buffinit(L, &b);
    i = 1;
    while (1) {
        ret = SQLGetDiagRec(type, handle, i, State, &NativeError, Msg, 
                sizeof(Msg), &MsgSize);
        if (ret == SQL_NO_DATA) break;
        luaL_addlstring(&b, (char*)Msg, MsgSize);
        luaL_addchar(&b, '\n');
        i++;
    } 
    luaL_pushresult(&b);
    return 2;
}

/*
** Returns the name of an equivalent lua type for a SQL type.
*/
static const char *sqltypetolua (const SQLSMALLINT type) {
    switch (type) {
        case SQL_UNKNOWN_TYPE: case SQL_CHAR: case SQL_VARCHAR: 
        case SQL_TYPE_DATE: case SQL_TYPE_TIME: case SQL_TYPE_TIMESTAMP: 
        case SQL_DATE: case SQL_INTERVAL: case SQL_TIMESTAMP: 
        case SQL_LONGVARCHAR:
        case SQL_WCHAR: case SQL_WVARCHAR: case SQL_WLONGVARCHAR:
            return "string";
        case SQL_BIGINT: case SQL_TINYINT: case SQL_NUMERIC: 
        case SQL_DECIMAL: case SQL_INTEGER: case SQL_SMALLINT: 
        case SQL_FLOAT: case SQL_REAL: case SQL_DOUBLE:
            return "number";
        case SQL_BINARY: case SQL_VARBINARY: case SQL_LONGVARBINARY:
            return "binary";	/* !!!!!! nao seria string? */
        case SQL_BIT:
            return "boolean";
        default:
            assert(0);
            return NULL;
    }
}



class Cursor
{
public:
    luabind::object getcolnames() { return this->colnames; }
    luabind::object getcoltypes() { return this->coltypes; }
    
    Cursor
    (   lua_State *L, Connection& conn,
        const SQLHSTMT hstmt_in, const SQLSMALLINT numcols_in
    )
    : conn_( conn ),
      closed(false),
      numcols( numcols_in ),
      hstmt( hstmt_in )
    {
//        std::cout << "cursor=" << (void*)this << " conn=" << &conn_ << std::endl;
        
        /* make and store column information table */
        this->create_colinfo(L);
    }

/*
** Closes a cursor.
*/
    void close ();
    
    luabind::object fetch
    (   lua_State* L,
        luabind::object lasttbl,
        const std::string& opts
    )
    {
        SQLRETURN rc = SQLFetch(this->hstmt);
        
        if (rc == SQL_NO_DATA) {
            return luabind::object(); //  o( luabind::from_stack(L,-1) );
        } else {
            SqlErrorTosser(hstmt) = rc; // fail(L, hSTMT, hstmt);
        }

        // if (lua_istable (L, 2))
        SQLUSMALLINT i;
        int num = opts.find('n') != std::string::npos;
        int alpha = opts.find('a') != std::string::npos;

        for (i = 1; i <= this->numcols; i++)
        {
            luabind::object o = push_column( L,
                luabind::object_cast<const std::string>(this->coltypes[i]),
                this->hstmt, i );

            if (alpha)
                lasttbl[this->colnames[i]] = o;
                        
            if (num)
                lasttbl[i] = o;
        }
            
        return lasttbl;	/* return table */
        
# if 0  // dmp not sure what this is for?  Looks like it just returns all column values directly, not in a table
        else {
            SQLUSMALLINT i;
            luaL_checkstack (L, this->numcols, LUASQL_PREFIX"too many columns");
            for (i = 1; i <= this->numcols; i++) {
                ret = push_column (L, this->coltypes, this->hstmt, i);
                if (ret)
                    return ret;
            }
            return this->numcols;
        }
# endif 
    }

private:
    Connection &conn_;
	bool       closed;
	int        conn;               /* reference to connection                */
	int        numcols;            /* number of columns                      */
	SQLHSTMT   hstmt;              /* statement handle                       */
    luabind::object coltypes;
    luabind::object colnames;      /* reference to column information tables */

    /*
    ** Retrieves data from the i_th column in the current row
    ** Input
    **   types: index in stack of column types table
    **   hstmt: statement handle
    **   i: column number
    ** Returns:
    **   0 if successfull, non-zero otherwise;
    */
    static luabind::object push_column
    (   lua_State *L,
        const std::string& tname,
        const SQLHSTMT hstmt, 
        SQLUSMALLINT i
    )
    {
        char type = tname[1];
//        std::cout << "column=" << i << " tname=" << tname << std::endl;

        /* deal with data according to type */
        switch (type) {
            /* nUmber */
            case 'u': { 
                double num;
                SQLLEN got;
                SqlErrorTosser() = SQLGetData(hstmt, i, SQL_C_DOUBLE, &num, 0, &got);
                if (got == SQL_NULL_DATA)
                    lua_pushnil(L);
                else
                    lua_pushnumber(L, num);
            }
            break;
            
                /* bOol */
            case 'o': { 
                char b;
                SQLLEN got;
                SqlErrorTosser() = SQLGetData(hstmt, i, SQL_C_BIT, &b, 0, &got);
                // return fail(L, hSTMT, hstmt);
                if (got == SQL_NULL_DATA)
                    lua_pushnil(L);
                else
                    lua_pushboolean(L, b);
            }
            break;
            
                /* sTring */
            case 't': 
                /* bInary */
            case 'i': { 
                SQLSMALLINT stype = (type == 't') ? SQL_C_CHAR : SQL_C_BINARY;
                SQLLEN got;
                char *buffer;
                luaL_Buffer b;
                SQLRETURN rc;
                luaL_buffinit(L, &b);
                buffer = luaL_prepbuffer(&b);
                rc = SQLGetData(hstmt, i, stype, buffer, LUAL_BUFFERSIZE, &got);
                if (got == SQL_NULL_DATA) {
                    lua_pushnil(L);
                    break;
                }
                /* concat intermediary chunks */
                while (rc == SQL_SUCCESS_WITH_INFO) {
                    if (got >= LUAL_BUFFERSIZE || got == SQL_NO_TOTAL) {
                        got = LUAL_BUFFERSIZE;
                        /* get rid of null termination in string block */
                        if (stype == SQL_C_CHAR) got--;
                    }
                    luaL_addsize(&b, got);
                    buffer = luaL_prepbuffer(&b);
                    rc = SQLGetData(hstmt, i, stype, buffer, 
                        LUAL_BUFFERSIZE, &got);
                }
                /* concat last chunk */
                SqlErrorTosser() = rc;
                
                if (rc == SQL_SUCCESS) {
                    if (got >= LUAL_BUFFERSIZE || got == SQL_NO_TOTAL) {
                        got = LUAL_BUFFERSIZE;
                        /* get rid of null termination in string block */
                        if (stype == SQL_C_CHAR) got--;
                    }
                    luaL_addsize(&b, got);
                }
                /* return everything we got */
                luaL_pushresult(&b);
            } // end, case 'i'
            break;
            
        } // end, switch

        // return whatever the last thing pushed was
        luabind::object object(luabind::from_stack(L,-1));
        lua_pop( L, 1 );
        return object;
    }

    /* ** Creates two tables with the names and the types of the columns. */
    void create_colinfo (lua_State *L)
    {
        this->coltypes = luabind::newtable( L );
        this->colnames = luabind::newtable( L );
    
        for (int i = 1; i <= this->numcols; i++)
        {
            SQLCHAR buffer[256];
            SQLSMALLINT namelen, datatype;

            SqlErrorTosser() = SQLDescribeCol(this->hstmt, i, buffer, sizeof(buffer), 
                &namelen, &datatype, NULL, NULL, NULL);

            this->colnames[i] = std::string( (char*)buffer, namelen );
            this->coltypes[i] = std::string(sqltypetolua(datatype));
        }
    }
}; // end, class Cursor




/*
** Creates and returns a connection object
** Lua Input: source [, user [, pass]]
**   source: data source
**   user, pass: data source authentication information
** Lua Returns:
**   connection object if successfull
**   nil and error message otherwise.
*/
class Connection
{
    struct conn_data
    {
        bool       closed;
        int        cur_counter;
        Environment&  env;                /* reference to environment */
        SQLHDBC    hdbc;               /* database connection handle */
        conn_data( Environment& e ) : env(e) {}
    };

    void create_connection( ThreadReporter )
    {
        SQLCHAR retConStr[1024];
        
        /* tries to connect handle */
        SqlErrorTosser(hDBC, conn_.hdbc) = SQLDriverConnect (conn_.hdbc, NULL,
            (SQLCHAR*)sourcename_.c_str(), SQL_NTS, 
            retConStr, 1024, NULL, SQL_DRIVER_NOPROMPT);

        // @todo: proper RAII on hDBC handle
//         if (error(ret)) {
//             SQLFreeHandle(hDBC, hdbc);
//             return ret;
//         }

        /* set auto commit mode */
        SqlErrorTosser(hDBC, conn_.hdbc) =
            SQLSetConnectAttr(conn_.hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER) SQL_AUTOCOMMIT_ON, 0);

        /* fill in structure */
        conn_.closed = 0;
        conn_.cur_counter = 0;
        conn_.env.plusConnection();
    }

    void do_ctexec( const std::string sss, lua_State* L, ThreadReporter reporter )
    {
        const SQLCHAR* statement = (const SQLCHAR*)sss.c_str();
        conn_data* conn = & conn_;
        RaiiSqlHandle hstmt( conn->hdbc );

        SqlErrorTosser(hstmt) = SQLPrepare(hstmt, const_cast<SQLCHAR*>(statement), SQL_NTS);

        /* execute the statement */
        SqlErrorTosser(hstmt) = SQLExecute(hstmt);

        /* determine the number of results */
        SQLSMALLINT numcols;
        SqlErrorTosser(hstmt) = SQLNumResultCols (hstmt, &numcols);

        if (numcols > 0)
        {
            /* if there is a results table (e.g., SELECT) */
            reporter.preReport();
//            std::cout << "creating cursor" << std::endl;
            Cursor* c = new Cursor(L, *this, hstmt, numcols);
            ++conn_.cur_counter;
            
//            std::cout << "created cursor=" << c << std::endl;
            reporter.postReport();
        
//            std::cout << "freeing up" << std::endl;
            reporter.report( c );
//            std::cout << "reported" << std::endl;

            hstmt.release();
        }
        else
        {
            /* if action has no results (e.g., UPDATE) */
            SQLLEN numrows;
            SqlErrorTosser() = SQLRowCount(hstmt, &numrows);
            reporter.report( (int)numrows );
        }
    } // end, threadreporting exec

public:
    StdFunctionCaller
    ctexec( lua_State* L, const std::string& statement )
    {
        return StdFunctionCaller( std::bind( &Connection::do_ctexec, this, statement, L, std::placeholders::_1 ) );
    }

    void cursorClosed( Cursor* cursor )
    {
        --conn_.cur_counter;
    }

    void close()
    {
        if (conn_.closed)
            return;
        
        if (conn_.cur_counter > 0)
            throw std::runtime_error("there are open cursors");

        /* Decrement connection counter on environment object */
        // @todo: 
        // conn_.env.conn_counter--;
        
        /* Nullify structure fields. */
        conn_.closed = true;

        // @todo: review ownership
        //luaL_unref (L, LUA_REGISTRYINDEX, conn->env);
        
        SqlErrorTosser(hDBC,conn_.hdbc) = SQLDisconnect(conn_.hdbc);
        
        SqlErrorTosser(hDBC,conn_.hdbc) = SQLFreeHandle(hDBC, conn_.hdbc);
    }
    
    Connection( Environment& env, const std::string& sourcename )
    : sourcename_( sourcename ),
      conn_( env )
    {
//         SQLCHAR *username = nullptr; // (SQLCHAR*)luaL_optstring (L, 3, NULL);
//         SQLCHAR *password = nullptr; // (SQLCHAR*)luaL_optstring (L, 4, NULL);

        /* tries to allocate connection handle */
        SQLRETURN ret = SQLAllocHandle (hDBC, env.henv(), &conn_.hdbc);
        SqlErrorTosser(ret, "connection allocation error.");

    }

    StdFunctionCaller ctconnect()
    {
        /* success, return connection object */
        return StdFunctionCaller( std::bind( &Connection::create_connection, this, std::placeholders::_1 ) );
    }

    void commit ()   { SqlErrorTosser(hSTMT,conn_.hdbc) = SQLEndTran(hDBC, conn_.hdbc, SQL_COMMIT); }
    void rollback () { SqlErrorTosser(hSTMT,conn_.hdbc) = SQLEndTran(hDBC, conn_.hdbc, SQL_ROLLBACK); }

    void
    setautocommit( bool turnItOn )
    {
        SqlErrorTosser(hSTMT, conn_.hdbc) = SQLSetConnectAttr(conn_.hdbc, SQL_ATTR_AUTOCOMMIT,
                (SQLPOINTER) (turnItOn ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF), 0);
    }

private:
    std::string sourcename_;
    conn_data conn_;
};


void Cursor::close ()
{
    if (this->closed)
        return;

    /* Nullify structure fields. */
    this->closed = 1;

    SqlErrorTosser(this->hstmt) = SQLCloseCursor(this->hstmt);
    SqlErrorTosser(this->hstmt) = SQLFreeHandle(hSTMT, this->hstmt);
    
    /* Decrement cursor counter on connection object */
    this->conn_.cursorClosed(this);
    this->colnames = luabind::object();
    this->coltypes = luabind::object();
}



/*
** Register the objects via Luabind
*/
extern "C" __declspec(dllexport) int luaopen_luasql_odbc (lua_State *L)
{
    using namespace luabind;
    
    module( L, "NylonSqlOdbc" ) [
       
        class_<Environment>("Environment")
        .def( constructor<>() )
        .def( "close", &Environment::close ),
       
        class_<Connection>("Connection")
        .def( constructor<Environment&, const std::string&>() )        
        .def( "ctconnect",     &Connection::ctconnect )
        .def( "ctexec",        &Connection::ctexec )
        .def( "setautocommit", &Connection::setautocommit )
        .def( "close",         &Connection::close )
        .def( "commit",        &Connection::commit )
        .def( "rollback",      &Connection::rollback ),
//       .def( "connect", &Environment::connect )

        class_<Cursor>("Cursor")
        .def( "fetch", &Cursor::fetch )
        .def( "close", &Cursor::close )
        .def( "getcolnames", &Cursor::getcolnames )
        .def( "getcoltypes", &Cursor::getcoltypes )       
    ];
    
	return 0;
} 
