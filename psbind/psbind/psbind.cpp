// Okay, this is my first attempt at mixed-mode / managed C++ binding, so
// cut me some slack.

// @todo:  Provide a invoke_select that allows to specify which properties / fields / methods
// ought to be converted.  As it is, the conversion of _all_ properties can be pretty time
// consuming, and can even become recursive.  It's not a shock, I guess; the PS object properties
// can be "active" objects, so potentially they could go on forever.  I guess an alternate strategy
// would be to do study/clone whatever strategy is used by export-clixml, because it must deal
// with the same problem somehow.
//
// alternatively i guess you can just include "select-object x,y,z" as the final part of the pipeline...
// ^^^ that's really the easiest thing to do.
//
// Or, @todo, do a proper .net object binding, at least for PSObject.  That makes a great deal more sense than converting
// everything to lua tables immediately anyway.

#include "stdafx.h"

#include "psbind.h"

using namespace System::Management::Automation;
using namespace System::Management::Automation::Host;
using namespace System::Management::Automation::Runspaces;
using namespace System::Collections::ObjectModel;

//#include <msclr/marshal_cppstd.h>

//#include <luabind/luabind.hpp>
#include "nylon-runner.h"

#include "luabind_i64.h"

void MarshalString ( System::String ^ s, std::string& os ) {
   using namespace Runtime::InteropServices;
   const char* chars = 
      (const char*)(Marshal::StringToHGlobalAnsi(s)).ToPointer();
   os = chars;
   Marshal::FreeHGlobal(IntPtr((void*)chars));
}

// using namespace msclr::interop;

// using namespace Runtime::InteropServices;
luabind::object PSObjectToLuaTable( lua_State* l, PSObject^ result, bool bAllowMoreRecursion );

void SetLuaFieldFromNetProperty
(   lua_State* l,
    luabind::object& tResult,
    PSPropertyInfo^ property,
    bool bAllowMoreRecursion = true
)
{
    if
    (   property->MemberType == PSMemberTypes::Property
    ||  property->MemberType == PSMemberTypes::NoteProperty
    )
    {
        try
        {
            if (property->Value == nullptr)
            {
                if( false )
                {
                    Console::WriteLine(
                        " --@ {0,-20} [{1}] NULL VALUE",
                        property->Name,
                        property->TypeNameOfValue );
                }
                return;
            }

            if
            (   Object::ReferenceEquals(property->Value->GetType(),System::String::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::Int32::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::Int64::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::Boolean::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::Double::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::DateTime::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::DayOfWeek::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::DateTimeKind::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::TimeSpan::typeid)
            ||  Object::ReferenceEquals(property->Value->GetType(),System::IO::FileAttributes::typeid)
//            ||  Object::ReferenceEquals(property->Value->GetType(),System::IO::DirectoryInfo::typeid)
//            ||  Object::ReferenceEquals(property->Value->GetType(),System::ServiceProcess::ServiceControllerStatus::typeid)
            ||  property->Value->GetType()->IsEnum
            )
            {
                std::string propName;
                MarshalString( property->Name, propName );

                if ( Object::ReferenceEquals(property->Value->GetType(),System::String::typeid) )
                {
                    std::string propValue;
                    MarshalString( safe_cast<System::String^>(property->Value), propValue );
                    tResult[propName] = propValue;
                }
                else if ( Object::ReferenceEquals(property->Value->GetType(),System::Int32::typeid) )
                {
                    int i = *safe_cast<System::Int32^>(property->Value);
                    tResult[propName] = i;
                }
                else if ( Object::ReferenceEquals(property->Value->GetType(),System::Int64::typeid) )
                {
                    __int64 v = *safe_cast<System::Int64^>(property->Value);
                    tResult[propName] = v;
                }
                else if ( Object::ReferenceEquals(property->Value->GetType(),System::Boolean::typeid) )
                {
                    bool v = *safe_cast<System::Boolean^>(property->Value);
                    tResult[propName] = v;
                }
                else if ( Object::ReferenceEquals(property->Value->GetType(),System::Double::typeid) )
                {
                    double v = *safe_cast<System::Double^>(property->Value);
                    tResult[propName] = v;
                }
                else if ( Object::ReferenceEquals(property->Value->GetType(),System::DateTime::typeid) )
                {
                    if (bAllowMoreRecursion)
                    {
                        tResult[propName] = PSObjectToLuaTable(l, gcnew PSObject( property->Value ), false );
                    }
                }
                else if (property->Value->GetType()->IsEnum)
                {
                    std::string propValue;
                    MarshalString( safe_cast<System::Enum^>(property->Value)->ToString(), propValue );
                    tResult[propName] = propValue;
                }
                else
                {
                    // luabind::object t = NetObjectToLuaTable();
                    if (bAllowMoreRecursion)
                    {
                        tResult[propName] = PSObjectToLuaTable(l, gcnew PSObject( property->Value ), true );
                    }
//                     tResult[propName] = true;
//                     Console::WriteLine(
//                         " --@ {0,-20} [{1}] VALUE={2}",
//                         property->Name,
//                         property->TypeNameOfValue,
//                         property->Value );
                }
            }
            else
            {
                Console::WriteLine(
                    " --@ {0,-20} + TypeNameOfValue={1} GetType={2} MemberType={3}",
                    property->Name,
                    property->TypeNameOfValue,
                    property->GetType(),
                    property->MemberType
                                  );
            }
        }
        catch( GetValueInvocationException^e )
        {
            if( false )
            {
                Console::WriteLine(
                    " Could not get value for {0} -- {1}",
                    property->Name, e );
            }
        }
    } 
}


luabind::object PSObjectToLuaTable( lua_State* l, PSObject^ result, bool bAllowMoreRecursion )
{
    luabind::object tResult = luabind::newtable(l);
    PSMemberInfoCollection<PSPropertyInfo^>^ properties = result->Properties;

#if 0
    Console::WriteLine(
        "{0,-20} {1}",
        result->Members["ProcessName"]->Value,
        result->Members["HandleCount"]->Value);
#endif             

    for each (PSPropertyInfo^ property in properties)
    {
        SetLuaFieldFromNetProperty( l, tResult, property, bAllowMoreRecursion );
    }

//    Console::WriteLine("");

    return tResult;
}

#include "psbind-unmanwrap.h"

   template< class T >
   void templatereport( luabind::object& o_, T&& p1 )
   {
//      preReport();
      o_( std::forward<T>(p1) );
//      postReport();
   }

// holy cow this is a bunch of indirection and wrapping of redirecting wrapping indirectors
// but honestly, meh; whatever. it's really not as slow as you might think
public ref class Pshell
{
    Runspace^ runspace_;
    PowerShell^ shell_;
public:
    Pshell()
    {
        runspace_ = RunspaceFactory::CreateRunspace();
        runspace_->Open();
        shell_ = PowerShell::Create();
    }

    luabind::object invoke( lua_State* l, const std::string& cmd )
    {
        luabind::object tResults = luabind::newtable(l);

        shell_->AddScript( gcnew System::String(cmd.c_str()) );
    
        Collection<PSObject^> results = shell_->Invoke();

        auto t1 = DateTime::Now.Ticks;
        int nResults = 0;
        for each (PSObject^ result in results )
        {
            tResults[++nResults] = PSObjectToLuaTable( l, result, true );
        }
        auto t2 = DateTime::Now.Ticks;
        Console::WriteLine("Conversion took={0} ticks", (t2-t1)/10000);

        return tResults;
    }

    void invoke_threadreporter( lua_State* l, const std::string& cmd, ThreadReporter reporter )
    {
        shell_->AddScript( gcnew System::String(cmd.c_str()) );
    
        Collection<PSObject^> results = shell_->Invoke();

        auto t1 = DateTime::Now.Ticks;
        for each (PSObject^ result in results )
        {
            reporter.preReport();
            luabind::object t = PSObjectToLuaTable( l, result, true );
            templatereport( reporter.access(), t );
            reporter.postReport();
        }
        auto t2 = DateTime::Now.Ticks;
        Console::WriteLine("Conversion took={0} ticks", (t2-t1)/10000);

//        return tResults;
    }
    
};


#include <vcclr.h>

class PshellUnmanWrap::Pimpl
{
    gcroot<Pshell^> pshell_;
public:
    Pimpl()
    {
        pshell_ = gcnew Pshell();
    }
    luabind::object invoke( lua_State* l, const std::string& cmd )
    {
        return pshell_->invoke( l, cmd );
    }
    void invoke_threadreporter( lua_State* l, const std::string& cmd, ThreadReporter reporter )
    {
        pshell_->invoke_threadreporter( l, cmd, reporter );
    }
};

PshellUnmanWrap::~PshellUnmanWrap()
{}

PshellUnmanWrap::PshellUnmanWrap()
{
    pimpl_ = std::unique_ptr<Pimpl>( new Pimpl );
}

luabind::object PshellUnmanWrap::invoke( lua_State* l, const std::string& cmd )
{
    return pimpl_->invoke(l,cmd);
}

void PshellUnmanWrap::invoke_threadreporter( lua_State* l, const std::string& cmd, ThreadReporter reporter )
{
    pimpl_->invoke_threadreporter(l,cmd,reporter);
}

_declspec(dllexport) std::unique_ptr<PshellUnmanWrap>
CreatePshellUnmanWrap()
{
    return std::unique_ptr<PshellUnmanWrap>( new PshellUnmanWrap );
}


// __declspec(dllexport) luabind::object pshell_invoke_x( lua_State* l, const std::string& cmd )
// {
//     luabind::object t = luabind::newtable(l);

//     // managed_invoke( l, t, cmd );

//     return t;
// }


