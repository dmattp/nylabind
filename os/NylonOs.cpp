

#include <Windows.h>
#include <WTypes.h>
#include <gdiplus.h>
#include <iostream>
#include <luabind/luabind.hpp>

#include "nylon-runner.h"

#include "objidl.h"
#include "shlobj.h"


#ifdef _WINDOWS
# include <codecvt>
#else
# include <locale>
#endif



namespace {

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
    using namespace Gdiplus;
    
        UINT  num = 0;          // number of image encoders
        UINT  size = 0;         // size of the image encoder array in bytes

        ImageCodecInfo* pImageCodecInfo = NULL;

        GetImageEncodersSize(&num, &size);
        if(size == 0)
        return -1;  // Failure

        pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
        if(pImageCodecInfo == NULL)
        return -1;  // Failure

        GetImageEncoders(num, size, pImageCodecInfo);

        for(UINT j = 0; j < num; ++j)
        {
                if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
                {
                        *pClsid = pImageCodecInfo[j].Clsid;
                        free(pImageCodecInfo);
                        return j;  // Success
                }
        }

        free(pImageCodecInfo);
        return -1;  // Failure
}


    
    std::string getclipboard()
    {
        std::string str;
        
        if(OpenClipboard(0))
        {
            char* cftext = (char*)GetClipboardData(CF_TEXT);
            if (cftext)
            {
                std::string str( cftext );
                str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
                CloseClipboard();
                return str;
            }
            CloseClipboard();
        }
        
        return ""; // not text
    }

    template<class T, class T2>
    bool CONTAINS( const T& container, const T2& item )
    {
        return std::find(container.begin(),container.end(),item) != container.end();
    }

    UINT CF_HTML;
    UINT CBID_FILECONTENTS;
    UINT CBID_FILEDESCRIPTOR;
    UINT CBID_FILEGROUPDESCRIPTOR;

    void getclipboard_ext( lua_State* L, luabind::object cbfun )
    {
        std::string str;

        if( !CF_HTML )
        {
            CF_HTML = RegisterClipboardFormat(L"HTML Format");
        }

        if( !CBID_FILECONTENTS )
        {
            CBID_FILECONTENTS = RegisterClipboardFormat(CFSTR_FILECONTENTS);
        }

        if( !CBID_FILEDESCRIPTOR )
        {
            CBID_FILEDESCRIPTOR = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
        }

//         if( !CBID_FILEGROUPDESCRIPTOR )
//         {
//             CBID_FILEGROUPDESCRIPTOR = RegisterClipboardFormat(CFSTR_FILEGROUPDESCRIPTORW);
//         }
        
        if(OpenClipboard(0))
        {
            std::vector<UINT> cbFormats;

            for (UINT format = EnumClipboardFormats(0); format != 0; format = EnumClipboardFormats(format))
            {
//                std::cout << "got format=" << format << std::endl;
                cbFormats.push_back(format);
            }

            // std::cout << "done" << std::endl;
            if (CONTAINS(cbFormats,CF_HTML))
            {
                char* cftext = (char*)GetClipboardData(CF_HTML);
                if (cftext)
                {
                    std::string str( cftext );
                    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
                    cbfun( "html", str );
                }
            }
            
            if (CONTAINS(cbFormats,CF_TEXT))
            {
                char* cftext = (char*)GetClipboardData(CF_TEXT);
                if (cftext)
                {
                    std::string str( cftext );
                    str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
                    cbfun( "text", str );
                }
            }

            if (CONTAINS(cbFormats,CF_BITMAP))
            {
                HBITMAP hbm = (HBITMAP)GetClipboardData(CF_BITMAP);
                
                using namespace Gdiplus;
                
                GdiplusStartupInput gdiplusStartupInput;
                ULONG_PTR gdiplusToken;
                GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

                Bitmap* pBM = Bitmap::FromHBITMAP( hbm, 0 );

                // Get the CLSID of the PNG encoder.
                CLSID   encoderClsid;
                GetEncoderClsid(L"image/png", &encoderClsid);

                IStream* pImageStream=0;
                HGLOBAL mem = GlobalAlloc(GHND,0);
                CreateStreamOnHGlobal( mem, false, &pImageStream );

                Status stat = pBM->Save(pImageStream, &encoderClsid, NULL);
                
                if(stat == Ok)
                {
                    STATSTG stats;
                    pImageStream->Stat(&stats,0);
                    BYTE* copy = new BYTE[ stats.cbSize.LowPart ];

                    const LARGE_INTEGER seekPos = {0};
                    pImageStream->Seek(seekPos, STREAM_SEEK_SET, 0);
                    //assert(hr == S_OK);
                    pImageStream->Read(copy, stats.cbSize.LowPart, 0);
                    //assert(hr == S_OK);
                    std::string imageString( (char*)copy, stats.cbSize.LowPart );
                    cbfun( "image/png", imageString );
                    delete[] copy;
                }

                        GlobalDiscard(mem);
                delete pBM;
                GdiplusShutdown(gdiplusToken);
            }

            if (CONTAINS(cbFormats,CF_HDROP))
            {
                luabind::object t = luabind::newtable( L );

                const HANDLE hData = GetClipboardData(CF_HDROP);

                if (hData)
                {
                    HDROP hDrop = (HDROP)GlobalLock(hData);
                    if (hDrop)
                    {
                        wchar_t lpszFileName[MAX_PATH] = { 0 };
                        
                        for (int i = 0; i < 16384; ++i)
                        {
                            if (!DragQueryFile(hDrop, i, lpszFileName, MAX_PATH))
                                break;
                            static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                            const std::string wpath = converter.to_bytes( lpszFileName );
                            t[i+1] = wpath;
                        }
                        
                        GlobalUnlock(hData);
                    }
                }
                
                cbfun( "CF_HDROP", t );
            }

            // see: https://msdn.microsoft.com/en-us/library/windows/desktop/bb776904(v=vs.85).aspx#filecontents
            if (CONTAINS( cbFormats, CBID_FILEDESCRIPTOR ))
            {
                luabind::object tarray = luabind::newtable( L );

                const HANDLE hData = GetClipboardData( CBID_FILEDESCRIPTOR );

                if (hData)
                {
                    const FILEGROUPDESCRIPTOR* const pDescriptor = (const FILEGROUPDESCRIPTOR*)GlobalLock(hData);
                    
                    if (pDescriptor)
                    {
                        for (unsigned ndx = 0; ndx < pDescriptor->cItems; ++ndx)
                        {
                            luabind::object t = luabind::newtable( L );

                            const wchar_t* lpszFileName = pDescriptor->fgd[ndx].cFileName;

                            t["flags"] = pDescriptor->fgd[ndx].dwFlags;

                            if (lpszFileName)
                            {
                                static std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                        
                                t["name"] = converter.to_bytes( lpszFileName );
                            }
                        
                            if (pDescriptor->fgd[ndx].dwFlags & FD_FILESIZE)
                            {
                                t["size"] = pDescriptor->fgd[ndx].nFileSizeLow;
                            }

                            tarray[ndx+1] = t;

                            GlobalUnlock(hData);
                            
                            if (CONTAINS( cbFormats, CBID_FILECONTENTS ))
                            {
#if 1
                                CloseClipboard();
                                
                                IDataObject* pDataObj = 0;
                                HRESULT hr = OleGetClipboard(&pDataObj);

                                if ((hr == S_OK) && pDataObj)
                                {
                                    t["haveOle"] = (unsigned)pDataObj;

                                    DVTARGETDEVICE dvtd = { 0 };
                                    FORMATETC fmt = { 0 };
                                    fmt.cfFormat = CBID_FILECONTENTS;
                                    fmt.lindex = ndx;
                                    fmt.tymed = TYMED_ISTREAM; // |TYMED_ISTORAGE|TYMED_HGLOBAL;
                                    fmt.ptd = &dvtd;
                                        
                                    STGMEDIUM medium = { 0 };
                                    auto rc = pDataObj->GetData ( &fmt, &medium );

                                    if (rc != S_OK)
                                    {
                                        switch(rc)
                                        {
                                            case DV_E_LINDEX: std::cerr << "DV_E_LINDEX\n"; break;
                                            case DV_E_FORMATETC: std::cerr << "DV_E_FORMATETC\n"; break;
                                            case DV_E_TYMED: std::cerr << "DV_E_TYMED\n"; break;
                                            case E_INVALIDARG: std::cerr << "E_INVALIDARG\n"; break;
                                        }
                                        std::cerr << "GetLastError()=" << GetLastError << std::endl;
                                    }

                                    t["gdrc"] = rc;

                                    switch( medium.tymed )
                                    {
                                        case TYMED_HGLOBAL:
                                            t["tymed"] = "hglobal";
                                            break;
                                        case TYMED_ISTREAM:
                                            t["tymed"] = "istream";
                                            break;
                                        case TYMED_ISTORAGE:
                                            t["tymed"] = "istorage";
                                            break;
                                        default:
                                            t["tymed"] = medium.tymed;
                                    }
                                    OleFlushClipboard();
                                }
                                else
                                {
                                    std::cerr << "could not OleGetClipboard, hr=" << hr << std::endl;
                                    if (hr == CLIPBRD_E_CANT_OPEN)
                                        std::cerr << "CLIPBRD_E_CANT_OPEN=" << CLIPBRD_E_CANT_OPEN << std::endl;
                                    std::cerr << "GetLastError()=" << GetLastError() << std::endl;
                                }
                                
                                OpenClipboard(0);
                                break;

                                    //const STGMEDIUM* const pMedium = (const STGMEDIUM*)GlobalLock(hData);
                                    // t["tymed"] = pMedium->tymed;
                                
                                // const HANDLE hData = GetClipboardData( CBID_FILECONTENTS );

//                                 if (hData)
//                                 {
//                                     // const STGMEDIUM* const pMedium = (const STGMEDIUM*)GlobalLock(hData);

//                                     IStream* pStream;
//                                     CreateStreamOnHGlobal( hData, false, 

                                    
                                    
//                                    STGMEDIUM*
//                                     IDataObject iobj;
#endif 
                            }
                            
                        } // end, for each descriptor
                        
                        
                        cbfun( "CFSTR_FILECONTENTS", tarray );
                    }
                }
            }
            
            
            CloseClipboard();
        }
    } // end, getclipboard_ext
    

    void setclipboard( const std::string& text )
    {
        if(OpenClipboard(0))
        {
            HGLOBAL clipbuffer;
            char * buffer;
            EmptyClipboard();
            clipbuffer = GlobalAlloc(GMEM_DDESHARE, text.length()+1);
            buffer = (char*)GlobalLock(clipbuffer);
            strcpy_s(buffer, text.length()+1, text.c_str() ); // LPCSTR(source));
            GlobalUnlock(clipbuffer);
            SetClipboardData(CF_TEXT,clipbuffer);
            CloseClipboard();
        }
    }


#if _WINDOWS      
BOOL SetPrivilege(
    HANDLE hToken,          // access token handle
    LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
    BOOL bEnablePrivilege   // to enable or disable privilege
    ) 
{
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if ( !LookupPrivilegeValue( 
            NULL,            // lookup privilege on local system
            lpszPrivilege,   // privilege to lookup 
            &luid ) )        // receives LUID of privilege
    {
        printf("LookupPrivilegeValue error: %u\n", GetLastError() ); 
        return FALSE; 
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if ( !AdjustTokenPrivileges(
           hToken, 
           FALSE, 
           &tp, 
           sizeof(TOKEN_PRIVILEGES), 
           (PTOKEN_PRIVILEGES) NULL, 
           (PDWORD) NULL) )
    { 
          printf("AdjustTokenPrivileges error: %u\n", GetLastError() ); 
          return FALSE; 
    } 

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

    {
          printf("The token does not have the specified privilege. \n");
          return FALSE;
    } 

    return TRUE;
}
    void addCreateGlobalPrivilege()
    {
        HANDLE hToken;
        OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken );
        SetPrivilege( hToken, L"SeCreateGlobalPrivilege", TRUE );
    }

    // these should really be moved to a separate "OS support" library, but
    // it was convenient to have them here when I didn't have that yet.
    // Sorry for the mess.
    luabind::object systemTimeToLua( lua_State* L, const SYSTEMTIME& stTimestamp )
    {
        luabind::object t = luabind::newtable( L );

        t["y"] = stTimestamp.wYear;
        t["m"] = stTimestamp.wMonth;
        t["d"] = stTimestamp.wDay;
        t["H"] = stTimestamp.wHour;
        t["M"] = stTimestamp.wMinute;
        t["S"] = stTimestamp.wSecond;
        t["mil"] = stTimestamp.wMilliseconds;
        
        return t;
    }
    luabind::object systemTimeToLua( lua_State* L, const FILETIME& ftTimestamp )
    {
        SYSTEMTIME stTimestamp;
        FileTimeToSystemTime( &ftTimestamp, &stTimestamp );
        return systemTimeToLua( L, stTimestamp );
    }

    luabind::object GetFileTime_( lua_State* L, const std::string& fname )
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wpath = converter.from_bytes( fname );
        luabind::object times = luabind::newtable( L );
        FILETIME ftcreate;
        FILETIME ftaccess;
        FILETIME ftwrite;
        HANDLE hFile = CreateFile( wpath.c_str(), GENERIC_READ, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, 0, NULL);
        BOOL rc = ::GetFileTime( hFile, &ftcreate, &ftaccess, &ftwrite );
        if( rc )
        {
            times["create"] = systemTimeToLua( L, ftcreate );
            times["access"] = systemTimeToLua( L, ftaccess );
            times["write"]  = systemTimeToLua( L, ftwrite );
        }

        CloseHandle( hFile );

        return times;
    }
#endif 

    bool IsWindows()
    {
#ifdef _WINDOWS
        return true;
#else
        return false;
#endif 
    }

} // end, namespace anonymous



#ifdef _WINDOWS
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif 

extern "C" DLLEXPORT  int luaopen_NylonOs( lua_State* L )
{
   using namespace luabind;

   // std::cout << "opening NylonOs" << std::endl;

   // open( L ); // wow, don't do this from a coroutine.  make sure the main prog inits luabind.
   // also, dont do this after somebody else has done it, at least with newer versions of luabind; it replaces the
   // table of all registered classes.

   module( L, "NylonOs" ) [
       namespace_("Static") [
           def("getclipboard",&getclipboard)
           ,def("getclipboard_ext",&getclipboard_ext)
           ,def("setclipboard",&setclipboard)
       ]
       ,def("IsWindows", &IsWindows)
#ifdef _WINDOWS       
       ,def( "addCreateGlobalPrivilege", &addCreateGlobalPrivilege )
       ,def( "GetFileTime", &GetFileTime_ )
#endif 
   ];
   
   //std::cout << "NylonOs opened" << std::endl;
   return 0;
}
