namespace luabind
{
    template <>
    struct default_converter<__int64>
      : native_converter_base<__int64>
    {
        static int compute_score(lua_State* L, int index)
        {
            return lua_type(L, index) == LUA_TNUMBER ? 0 : -1;
        }

        __int64 from(lua_State* L, int index)
        {
            return __int64(lua_tonumber(L, index));
        }

        void to(lua_State* L, __int64 const& x)
        {
            lua_pushnumber(L, (double)x);
        }
    };

    template <>
    struct default_converter<__int64 const&>
      : default_converter<__int64>
    {};
}
