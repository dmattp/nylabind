

class PshellUnmanWrap
{
private:
    class Pimpl;
    std::unique_ptr<Pimpl> pimpl_;
public:
    PshellUnmanWrap();
    virtual ~PshellUnmanWrap();
    luabind::object invoke( lua_State* l, const std::string& cmd );
    void invoke_threadreporter( lua_State* l, const std::string& cmd, ThreadReporter reporter);
};


_declspec(dllexport) std::unique_ptr<PshellUnmanWrap>
CreatePshellUnmanWrap();
    
