//
//  VCStoreInMemory.h
//  Vcookie
//
//  Created by Russ Stringham on 8/20/12.
//
//

#ifndef Vcookie_VCStoreInMemory_h
#define Vcookie_VCStoreInMemory_h

#include "vcookie.h"
#include "vcookiestore.h"
#include <map>


// This is a very simple in-memory database. VCookies are stored in a C++ STL map data structure.
// A map is a key/value data store implemented using a tree.
// The VCookieId data structure (defined below) is the Key. It contains the userid and visitor id
// The value portion is actually a STL pair data structure, containing the last hit time as the first value
// in the pair, and the serialized byte array of all other vcookie data in the second half of the pair.
// The last hit time is stored separately, so I can walk the tree and easily find hits older than
// a specified date.

class VCookieId {
public:
    VCookieId (unsigned user, unsigned long long vid_high, unsigned long long vid_low)
    : userid (user), visid_high (vid_high), visid_low(vid_low)
    {
    }
    unsigned GetUser () const                   { return userid; }
    unsigned long long GetVisIdHigh () const    { return visid_high; }
    unsigned long long GetVisIdLow () const     { return visid_low; }
    
    bool operator < (const VCookieId &b) const
    {
        return (userid < b.userid || visid_high < b.visid_high || visid_low < b.visid_low);
    }
    bool operator >= (const VCookieId &b) const { return !operator < (b); }
    bool operator <= (const VCookieId &b) const
    {
        return (userid <= b.userid || visid_high <= b.visid_high || visid_low <= b.visid_low);
    }
    bool operator > (const VCookieId &b) const { return !operator <= (b); }
private:
    const unsigned userid;
    const unsigned long long visid_high;
    const unsigned long long visid_low;
};

class VCStoreInMemory: public VCookieStore
{
public:
   
	virtual bool SaveVCookie (VCookie const &vcookie)
    {
        VCookieId vid (vcookie.GetUser(), vcookie.GetVisIdHigh(), vcookie.GetVisIdLow());
        std::vector<char> dummy; // use a dummy vector, because the map::insert call copies the vector data.
        time_t t = vcookie.GetLastHitTimeGMT();
        StoreRet r = store.insert (StorePair (vid, ValuePair (t, dummy)));
        Serialize(vcookie, r.first->second.second, false); // fill in the map vector with the vcookie data
        r.first->second.first = t;
        return true;
    }
	virtual bool LoadVCookie (VCookie &vcookie)
    {
        VCookieId vid (vcookie.GetUser(), vcookie.GetVisIdHigh(), vcookie.GetVisIdLow());
        
        StoreMap::const_iterator v = store.find(vid);
        if (v == store.end()) {
            return false;
        }
        
        vcookie.SetLastHitTimeGMT(v->second.first);
        return Deserialize(vcookie, v->second.second);
    }
	virtual bool DeleteVCookie (VCookie &vcookie)
    {
        VCookieId vid (vcookie.GetUser(), vcookie.GetVisIdHigh(), vcookie.GetVisIdLow());
        
        size_t s = store.erase (vid);
        return (s > 0);
    }
    virtual unsigned long long DeleteOldVCookies (time_t t)
    {
        unsigned long long deleted = 0;
        StoreMap::iterator v=store.begin();
        while (v != store.end()) {
            if (v->second.first < t) {
                store.erase(v++);
                ++deleted;
            }
            else {
                ++v;
            }
        }
        return deleted;
    }
	virtual unsigned long long GetVCookieCount () const
    {
        return store.size();
    }
    
	virtual bool GetVCookie (VCookie &vcookie, unsigned long long index) const
    {
        if (index >= store.size()) {
            return false;
        }
        size_t i=0;
        StoreMap::const_iterator v=store.begin();
        // walking the tree is really inefficient if tree is at all big,
        // but this interface is strictly for testing and should never have more than a few nodes
        while (i < index) {
            ++v;
        }
        vcookie.Reset (v->first.GetUser(), v->first.GetVisIdHigh(), v->first.GetVisIdLow());
        
        vcookie.SetLastHitTimeGMT(v->second.first);
        return Deserialize(vcookie, v->second.second);
    }
private:
    typedef std::pair<time_t, std::vector<char> > ValuePair;
    typedef std::map<VCookieId, ValuePair > StoreMap;
    typedef std::pair<VCookieId, ValuePair > StorePair;
    typedef std::pair<StoreMap::iterator, bool> StoreRet;
                      
    StoreMap store;
};

#endif
