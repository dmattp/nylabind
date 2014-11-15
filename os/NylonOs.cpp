

#include <Windows.h>
#include <WTypes.h>
#include <gdiplus.h>
#include <iostream>
#include <luabind/luabind.hpp>

#include "nylon-runner.h"

#include "objidl.h"


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

    void getclipboard_ext( luabind::object cbfun )
    {
        std::string str;

        if( !CF_HTML )
        {
            CF_HTML = RegisterClipboardFormat(L"HTML Format");
        }
        
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
            
            CloseClipboard();
        }
    }
    

    void setclipboard( const std::string& text )
    {
        if(OpenClipboard(0))
        {
            HGLOBAL clipbuffer;
            char * buffer;
            EmptyClipboard();
            clipbuffer = GlobalAlloc(GMEM_DDESHARE, text.length()+1);
            buffer = (char*)GlobalLock(clipbuffer);
            strcpy(buffer, text.c_str()); // LPCSTR(source));
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
#endif 
   ];
   
   //std::cout << "NylonOs opened" << std::endl;
   return 0;
}
