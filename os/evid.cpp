///////////////////////////////////////////////////////////////////////////////////////////////////
// This module generates what attempt to be universally unique event IDs based on
// a combination of a timestamp, a unique node id, and a sequence number.
//
// The approach is similar to that used by e.g., twitter snowflake, cuid, or others with
// the distinction that we rely on a large (96 bit) unique node id (really event source process id)
// to guarantee universal uniqueness rather than trying to pack the event id content into
// fewer bytes eg. a 64-bit integer; a 64 bit size limits the number of node id's which would
// require some sort of central coordinator.
//
// the event id consists of a 36-bit timestamp followed by the 96 bit node id, followed by
// a 12 bit sequence id.  This allows the generation of 4096 unique event ids by any process
// within a 0.1 second period. The event id bits are encoded into a web-safe set of lexicographically
// ordered BASE64 encoding characters, giving a transport-safe event id of 24 characters (144 bits total).
// The BASE64 character set chosen allows the event IDs to be e.g., used in URLs if
// desired and also maintains a proper sort order based on the timestamp (the first 6 characters are the 36 bit timestamp).
// This allows the ID to serve not just as a unique ID, but also as a roughly accurate
// timestamp for time based ordering among events. Of course the timestamp cannot be guaranteed to be
// accurate relatively between different systems, but within a process the IDs should correctly
// reflect the order in which event IDs were generated.  Unfortunately this means using a
// non-standard BASE64 encoding but there aren't any common or well known BASE64 encodings
// which have both of these properties; although there are some web-safe BASE64 standards,
// they don't use a character map with proper lexicographic order that will preserve the
// relative order of the values.  The 96 bit node id gives a collision probability of
// (of the node id) of ~6e-12 with a billion nodes- and collision probability is further
// reduced by the timestamp bits which would require any possible colliding nodes to
// be active within the same 100ms time slice. Collision probability is reduced a bit
// further by starting the 12 bit sequence field at a random number from 0-255.
//
// event id generation is shared among all threads in a process, so each process will receive
// a unique "node_id" to ensure event ids are unique among all processes. node_id is persisted
// based on the process name, and will be generated randomly if not present in the persistent
// storage (currently the windows registry).
//
// Persistent vs. Ephemeral node ids
//
// There may be some utility in, e.g., a process or event source like devman retaining the same
// node id through system restarts; e.g, this makes debugging easier maybe if the node id is
// stable and can always be identified as the devman process or what have you.  This comes at
// a cost of persisting the node id and also the last timestamp used to be sure timestamps
// never go backwards for a particular node id, and that requires periodically flushing the
// timestamp to disk, eg. every second or decisecond - and it means that if the clock is
// adjusted backwards the timestamps on the events may never be the correct time, since an
// offset would have to be maintained in order to ensure monotonic forward moving timestamps.
// So it gets ugly.
// 
// Ephemeral IDs will be re-generated on each process startup; they aren't stable between restarts but
// this avoids the need to persist either the node ID or the last timestamp used by that node, or to
// use a timestamp offset in the case the clock goes backwards.  If time goes backwards we just
// re-generate the node ID, voila! a much simpler solution.  Because of the timestamp component
// in the event ID we can always rely on the probability of hash collisions on node ids being low
// so long as the number of nodes active simultaneously remains low relative to the number of
// bits in the random node id.
// 
///////////////////////////////////////////////////////////////////////////////////////////////////

#if _WINDOWS
# include <windows.h>
# include <sysinfoapi.h>
# include <processthreadsapi.h>
# include <psapi.h> 
# include <wincrypt.h> 
# include "JagIso8601DateTime.h"
#else
# include <unistd.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif 

#include <cstdint>
#include <cstdlib>
#include <string>
#include <mutex>

#include "evid.h"

#define LCFG_PERSISTENT_NODE_IDS 0

namespace {
    static int gLastSequence = 0;
    static int64_t gLastTime;
    static const char BASE64_CHARS[] = "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

    std::recursive_mutex gEvidSequenceMutex;


#if LCFG_PERSISTENT_NODE_IDS    
    std::wstring GetProcessName()
    {
        HANDLE handle = GetCurrentProcess();
        
        wchar_t procname[1024];
            
        if (GetModuleBaseName(handle, NULL, procname, _countof(procname)))
        {
            return procname;
        }
        else
        {
            return L"???UnknownProcess???";
        }
    }
#endif 

    

    // There are two reasonable approaches to ensuring we don't duplicate IDs;
    // one would be to keep a persistent node ID for a process and ensure the
    // timestamp / sequence portion always increase - but this approach would require that
    // we also persist the timestamp and would generate persistently inaccurate times if the 
    // time of day clock went backwards and we needed to keep monotonicity.  The other approach is
    // to generate a unique node ID for every process, and rely on the size of node
    // ID being large enough to reduce collision probability among all actively running
    // systems that might be logging in the same time slice.  This has a slight disadvantage
    // in reading the event IDs because the ID for a process would change on every start,
    // but it makes things much much simpler WRT persisting.  If the system time is
    // ever observed to jump backwards, we generate a new node id so there won't be
    // collision as the system clock moves forward.
    int NextSequence(int64_t the_time)
    {
        std::lock_guard<std::recursive_mutex> guard(gEvidSequenceMutex);
        if (the_time == gLastTime)
        {
            int sequence = gLastSequence + 1;
            
            if (sequence >= 4096)
            {
                return -1;
            }

            gLastSequence = sequence;
            return sequence;
        }
        else
        {
            gLastTime = the_time;
            gLastSequence = rand() % 256;
            return gLastSequence;
        }
    }

    void encodeInt( int64_t the_int, unsigned bits, std::string& appendToMe )
    {
        unsigned max_bits = ((bits + 5) / 6) * 6;
        
        int shiftval = max_bits - 6;

        while (true)
        {
            appendToMe += BASE64_CHARS[ (the_int >> shiftval) & 0x3F ];
            
            if (shiftval < 6)
            {
                break;
            }
            
            shiftval -= 6;
        }
    }


    unsigned char valueOfChar[] = {
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        1, 2, 3, 4, 5, 6, 7, 8,     9,10, 0, 0, 0, 0, 0, 0,
        0,11,12,13,14,15,16,17,    18,19,20,21,22,23,24,25,   
        26,27,28,29,30,31,32,33,    34,35,36, 0, 0, 0, 0,37,
        0,38,39,40,41,42,43,44,    45,46,47,48,49,50,51,52,
        53,54,55,56,57,58,59,60,    61,62,63, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,     0, 0, 0, 0, 0, 0, 0, 0,
    };

    static int64_t
    decodeToInt64(const std::string& string)
    {
        int64_t sum = 0;

        for (size_t i = 0; i < string.length(); ++i)
        {
            sum <<= 6;
            sum += valueOfChar[string[i]];
        }

        return sum;
    }

    std::string
    NewEvid(const char* node_id)
    {
        std::string out;

#if _WINDOWS        
        FILETIME ft_now;
        GetSystemTimeAsFileTime(&ft_now);
        
        int64_t ll_now = (LONGLONG)ft_now.dwLowDateTime +
        ((LONGLONG)(ft_now.dwHighDateTime) << 32LL);
        
        int64_t epoch = ll_now - 116444736000000000LL;
        
        // value is 100ns increments, so / 10,000 = milliseconds
        int64_t deciseconds_epoch = epoch / 1000000;
#else
        int64_t deciseconds_epoch = time(NULL) * 10; // wanky, but whatever  //~~~@todo: fixme please
#endif 

        encodeInt(deciseconds_epoch, 36, out);

        out += node_id;
        
        const int seq = NextSequence(deciseconds_epoch);
        // todo: fix NextSequence to generate new node-id if we overrun

        encodeInt( seq, 12, out );
        
        return out;
    }

#if LCFG_PERSISTENT_NODE_IDS
}
#include "registry.h"
namespace {
    const char*
    LookupNodeId(const std::wstring& procname)
    {
        wchar_t node_id[65] = L"";
        static char node_id_short[65] = "";

        auto rc = CRegistry(L"SOFTWARE\\Radiant\\mqtt\\node_ids").GetValue(
            procname.c_str(), node_id, _countof(node_id));

        if (rc)
        {
            const size_t nid_len = wcslen(node_id);
            for (size_t i = 0; i <= nid_len; ++i)
            {
                node_id_short[i] = static_cast<char>(node_id[i]);
            }
        }

        return rc ? node_id_short : nullptr;
    }
    
    void
    PersistNodeId(const char* the_node_id, const std::wstring& procname )
    {
        auto rc = CRegistry::CreateKey(L"SOFTWARE\\Radiant\\mqtt");
        auto rc2 = CRegistry::CreateKey(L"SOFTWARE\\Radiant\\mqtt\\node_ids");

        std::wstring longNodeId(the_node_id, the_node_id + strlen(the_node_id));


        CRegistry writable;
        writable.OpenKey(L"SOFTWARE\\Radiant\\mqtt\\node_ids", platsys::osal::users::USER_ACCESS_WRITE);
        writable.SetValue(procname.c_str(), longNodeId.c_str());
    }
#endif 

    const char*
    GenerateNodeId()
    {
        static char node_id[17] = {0};
#if _WINDOWS        
        HCRYPTPROV crypto = 0;

        auto rc = CryptAcquireContext(
            &crypto, NULL, NULL, /* nullptr = default provider */
            PROV_RSA_FULL, CRYPT_VERIFYCONTEXT|CRYPT_SILENT);
        
        // node_id size is 16 base64 encoded chars, or 96 bits
        auto rcRandom = CryptGenRandom(crypto, 16, reinterpret_cast<BYTE*>(node_id));
        node_id[16] = 0;

        CryptReleaseContext(crypto, 0);
#else
        int fd = open("/dev/random", O_RDONLY);
        read(fd, node_id, 16);
        close(fd);
#endif 

        // easy peasy; generate random bytes and convert to valid BASE64 characters
        for (size_t i = 0; i < 16; ++i)
        {
            node_id[i] = BASE64_CHARS[ node_id[i] & 0x3f ];
        }

        return node_id;
    }


    const char*
    LookupOrGenerateNodeId()
    {
        static const char* the_node_id = nullptr;

        if (!the_node_id)
        {
#if LCFG_PERSISTENT_NODE_IDS        
            const auto& procname = GetProcessName();

            the_node_id = LookupNodeId(procname);

            if (!the_node_id)
            {
                the_node_id = GenerateNodeId();
                PersistNodeId(the_node_id, procname);
            }
#else
            the_node_id = GenerateNodeId();
#endif 
        }

        return the_node_id;
    }

    const char*
    GetNodeId()
    {
        static const char* node_id = LookupOrGenerateNodeId();

        return node_id;
    }
} // end, anonymous namespace



std::string EvidTime(const std::string& evid)
{
    std::string timePortion = evid.substr(0, 6);
    
    int64_t deciseconds = decodeToInt64(timePortion);

    int64_t ns100_evid = deciseconds * 1000000;

    int64_t ns100_epoch = ns100_evid + 116444736000000000LL;

#if _WINDOWS    
    FILETIME ft_evid;
    ft_evid.dwLowDateTime  = ns100_epoch & 0xFFFFFFFF;
    ft_evid.dwHighDateTime = ns100_epoch >> 32LL;

    SYSTEMTIME systime_evid;
    FileTimeToSystemTime(&ft_evid, &systime_evid);

    jag::datetime::DateTimeOffset datetime_evid(systime_evid);
    
    return datetime_evid.ToIso8601DateTimeString();
#else    
    return "2099-01-01T00:00:00.0";  //~~~@todo: fixme
#endif
}


std::string NewEvid()
{
    return NewEvid(GetNodeId());
}





