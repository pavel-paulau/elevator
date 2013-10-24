//
//  vcookiestore.cpp
//  Vcookie
//
//  Created by Russ Stringham on 8/15/12.
//
//

#include "vcookiestore.h"
#include "vcookie.h"
#include <string.h>

const unsigned char VC_SERIAL_VERSION = 0;

namespace {
    template<typename T>
    void AddItem (std::vector<char> &buffer, T val)
    {
        size_t index = buffer.size();
        buffer.resize (index + sizeof (T));
        
        memcpy (&buffer[index], &val, sizeof (T));
    }
    void AddItem (std::vector<char> &buffer, std::string const &val)
    {
        size_t index = buffer.size();
        buffer.resize (index + val.size() + 1);
        
        memcpy (&buffer[index], val.c_str(), val.size() + 1);
        
    }
    template<typename T, typename F>
    void AddField (std::vector<char> &buffer, F fieldId, T val)
    {
        if (val) {
            AddItem (buffer, fieldId);
            AddItem (buffer, val);
        }
    }
    template<typename F>
    void AddField (std::vector<char> &buffer, F fieldId, std::string val)
    {
        if (!val.empty()) {
            AddItem (buffer, fieldId);
            AddItem (buffer, val);
        }
    }
    
    template<typename T>
    bool ReadItem (const char **b, const char *e, T &val)
    {
        if (*b + sizeof (T) <= e) {
            memcpy (&val, *b, sizeof (T));
            *b += sizeof (T);
            return true;
        }
        return false;
    }
    bool ReadItem (const char **b, const char *e, std::string &val)
    {
        ptrdiff_t len = e - *b;
        if (len <= 0) {
            return false;
        }
        const char *p = reinterpret_cast<const char *> (memchr (*b, 0, size_t (len)));
        if (p) {
            val = *b;
            *b += p - *b + 1;
            return true;
        }
        return false;
    }
} // end anonymous namespace

void VCookieStore::Serialize (VCookie const &vcookie, std::vector<char> &buffer, bool saveLastHitTime)
{
    buffer.resize(0);
    buffer.reserve (1000);

    AddItem (buffer, VC_SERIAL_VERSION); // Version of serialization
    AddItem (buffer, VC_SERIAL_VERSION); // Read Compatible Version (an interface that knows how to read this version can deserialize this stream, even if the stream version is newer).
    
    unsigned char flag=0;
    AddItem (buffer, flag);
    
    unsigned offset = static_cast<unsigned>(buffer.size());
    AddItem (buffer, offset); // this field will be updated later with the offset of the RelVars
    
    // ALWAYS add new fields to the end or reading old serializations will be broken
    char field = 0;
    AddField (buffer, ++field, vcookie.GetFirstHitTimeGMT ());
    if (saveLastHitTime) {
        AddField (buffer, ++field, vcookie.GetLastHitTimeGMT ());
    }
    else {
        ++field;
    }
	AddField (buffer, ++field, vcookie.GetLastHitTimeVisitorLocal ());
	AddField (buffer, ++field, vcookie.GetLastVisitNum ());
    
	AddField (buffer, ++field, vcookie.GetLastPurchaseTimeGMT ());
	AddField (buffer, ++field, vcookie.GetFirstHitReferrer ());
	AddField (buffer, ++field, vcookie.GetFirstHitUrl ());
	AddField (buffer, ++field, vcookie.GetFirstHitPagename ());
	AddField (buffer, ++field, vcookie.GetLastPurchaseNum ());
    
	AddField (buffer, ++field, vcookie.GetMerchandising ());
    
    ++field;
	unsigned char c = static_cast<unsigned char> (vcookie.GetPurchaseIdCount ());
    if (c > 0) {
        AddField (buffer, field, c);
        for (unsigned i=0; i < c; ++i) {
            AddItem (buffer, vcookie.GetPurchaseId (i));
        }
    }
    
    AddItem (buffer, static_cast<unsigned char> (0)); // field value of zero marks beginning of RelationId Vars
    
    unsigned relVarOffset = static_cast<unsigned>(buffer.size());
    memcpy (&buffer[offset], &relVarOffset, sizeof (unsigned));
    
    VCookie::RelationId rid = vcookie.GetFirstSetVar ();
    VCookie::VarId vid;
    while (rid != VCookie::INVALID_RID) {
        unsigned cnt = vcookie.GetVarElementCount (rid, &vid);
        AddField (buffer, rid, cnt);
        for (unsigned i=0; i < cnt; ++i) {
            VCookie::RelVar const *rv = vcookie.GetVar (vid, i);
            AddItem (buffer, rv->value);
            AddItem (buffer, rv->timestamp);
            AddItem (buffer, rv->revision);
        }
        rid = vcookie.GetNextSetVar(rid);
    }
    AddItem (buffer, rid); // RelationId of InvalidRID indicates we are done
}

bool VCookieStore::Deserialize(VCookie &vcookie, const std::vector<char> &buffer)
{
    const char *b = &buffer[0];
    const char *e = b + buffer.size();
    
    unsigned char version, readCompatibleVersion, flag;
    ReadItem (&b, e, version);
    ReadItem (&b, e, readCompatibleVersion);
    ReadItem (&b, e, flag);
   
    if (readCompatibleVersion > VC_SERIAL_VERSION) {
        // error - we don't know how to read this serialization
        return false;
    }
    
    unsigned offset;
 
    if (!ReadItem(&b, e, offset)) return false;

    char field = 0;
    char next;
    if (!ReadItem (&b, e, next)) return false;
    
    
    // ALL of the "fields" below need to be kept in the same order as the serializer
    if (next == ++field) {
        time_t tval;
        if (!ReadItem(&b, e, tval)) return false;
        vcookie.SetFirstHitTimeGMT(tval);
        if (!ReadItem (&b, e, next)) return false;
    }
    if (next == ++field) {
        time_t tval;
        if (!ReadItem(&b, e, tval)) return false;
        vcookie.SetLastHitTimeGMT(tval);
        if (!ReadItem (&b, e, next)) return false;
    }
    if (next == ++field) {
        time_t tval;
        if (!ReadItem(&b, e, tval)) return false;
        vcookie.SetLastHitTimeVisitorLocal(tval);
        if (!ReadItem (&b, e, next)) return false;
    }
    if (next == ++field) {
        unsigned uval;
        if (!ReadItem(&b, e, uval)) return false;
        vcookie.SetLastVisitNum(uval);
        if (!ReadItem (&b, e, next)) return false;
    }

    if (next == ++field) {
        time_t tval;
        if (!ReadItem(&b, e, tval)) return false;
        vcookie.SetLastPurchaseTimeGMT(tval);
        if (!ReadItem (&b, e, next)) return false;
    }
    if (next == ++field) {
        std::string sval;
        if (!ReadItem(&b, e, sval)) return false;
        vcookie.SetFirstHitReferrer(sval);
        if (!ReadItem (&b, e, next)) return false;
    }
    if (next == ++field) {
        std::string sval;
        if (!ReadItem(&b, e, sval)) return false;
        vcookie.SetFirstHitUrl(sval);
        if (!ReadItem (&b, e, next)) return false;
    }
    if (next == ++field) {
        std::string sval;
        if (!ReadItem(&b, e, sval)) return false;
        vcookie.SetFirstHitPagename(sval);
        if (!ReadItem (&b, e, next)) return false;
    }
    if (next == ++field) {
        unsigned uval;
        if (!ReadItem(&b, e, uval)) return false;
        vcookie.SetLastPurchaseNum(uval);
        if (!ReadItem (&b, e, next)) return false;
    }

    if (next == ++field) {
        std::string sval;
        if (!ReadItem(&b, e, sval)) return false;
        vcookie.SetMerchandising(sval);
        if (!ReadItem (&b, e, next)) return false;
    }
    
    if (next == ++field) { // Purchase Id list
        unsigned char cnt;
        if (!ReadItem(&b, e, cnt)) return false;
        std::string pid;
        for (unsigned i=0; i < cnt; ++i) {
            if (!ReadItem(&b, e, pid)) return false;
            vcookie.SetPurchaseId(pid);
        }
        if (!ReadItem (&b, e, next)) return false;
    }
    // next should equal zero at this point (unless new fields have been added in a later version of the software
    
    if (b - &buffer[0] > offset) {
        // something is wrong, because we are past where the relVars as supposed to start
        return false;
    }
    
    b = &buffer[offset]; // this allows us to skip fields that were added to vcookie struct after this version of the code
 
    VCookie::RelationId rid;
    if (!ReadItem(&b, e, rid)) return false;
    
    std::string value;
    time_t timestamp;
    unsigned char revision;
    unsigned cnt;
    
    while (rid != VCookie::INVALID_RID) {
        VCookie::VarId vid = VCookie::VAR_NOT_SET;
        if (!ReadItem(&b, e, cnt)) return false;
        for (unsigned i=0; i < cnt; ++i) {
            if (!ReadItem(&b, e, value)) return false;
            if (!ReadItem(&b, e, timestamp)) return false;
            if (!ReadItem(&b, e, revision)) return false;
            if (vid == VCookie::VAR_NOT_SET) {
                vid = vcookie.SetVar(rid, value, timestamp, revision, cnt > 1 ? ALLOC_TYPE_LINEAR : ALLOC_TYPE_LAST, cnt);
            }
            else {
                vcookie.AddLinearElement(vid, value, timestamp, revision);
            }
            
        }
        if (!ReadItem(&b, e, rid)) return false;
    }
    return true;
}
