#ifndef VCOOKIE_HDR
#define VCOOKIE_HDR

#include <time.h>
#include <string>
#include <vector>
#include <deque>
#include <algorithm>
//#include "ecommerce_defs.h" // defines valid relation IDs

#include "vcookiestore.h"
enum AllocationType {
    ALLOC_TYPE_LAST   = 0,
    ALLOC_TYPE_FIRST  = 1,
    ALLOC_TYPE_LINEAR = 2
};

const unsigned MAX_LINEAR_NOT_SET = unsigned (-1);
const unsigned MAX_LINEAR_INFINITE = 0;

const unsigned NUM_SAVED_PURCHASE_IDS = 5;

class VCookie {
public:
    struct RelVar {
        std::string     value;
        time_t          timestamp;
        unsigned char   revision;
    };
    
    enum VarId {
        VAR_NOT_SET = unsigned (-1)
    };
    typedef unsigned short RelationId;
    static const RelationId INVALID_RID = static_cast<RelationId>(-1);

    VCookie (
        const unsigned _userid,
        const unsigned long long _visid_high,
        const unsigned long long _visid_low,
        bool newVisitor,  // set if we know this is a new visitor who will have no record in the database. If false, visitor still may not have a record.
        VCookieStore &store
    ) :
        newCookie (newVisitor),
        userid (_userid),
        visid_high (_visid_high),
        visid_low (_visid_low),
        lastHitTimeGMT (0),
        lastHitTimeVisitorLocal (0),
        firstHitTimeGMT (0),
        lastVisitNum (0),
        lastPurchaseTimeGMT (0),
        lastPurchaseNum (0),
        vstore (store)
    {
        
        if (!newCookie) {
            newCookie = !vstore.LoadVCookie (*this);
        }
        modified = trafficModified = ecommerceModified = merchandisingModified = relVarModified = false;

    }
    
    ~VCookie ()
    {
        if (modified)
			Store();
    }

	bool Store(void)
	{
		bool retVal = false;
		try {
			// save it
			retVal = vstore.SaveVCookie(*this);
			// and mark it unmodified
			modified = trafficModified = ecommerceModified = merchandisingModified = relVarModified = false;			
		}
		catch (...) {}
		
		return retVal;
	}
	
    // should only be used by VCookieStore implemenations, not by elevator
    void Reset (unsigned user, unsigned long long vid_high, unsigned long long vid_low)
    {
        userid = user;
        visid_high = vid_high;
        visid_low = vid_low;
        
        // Traffic Info
        lastHitTimeGMT = 0;
        lastHitTimeVisitorLocal = 0;
        firstHitTimeGMT = 0;
        lastVisitNum = 0;
        
        // Ecommerce Info
        firstHitReferrer.clear();
        firstHitPageUrl.clear();
        firstHitPagename.clear();
        lastPurchaseTimeGMT = 0;
        lastPurchaseNum = 0;
        purchaseIds.clear();
        
        merchandising.clear();
        
        relVar.clear();
        relVarSort.clear();
    }
    
    bool    IsNewCookie () const                         { return newCookie; }
    bool    IsModified () const                          { return modified; }
    bool    IsTrafficModified () const                   { return trafficModified; }
    bool    IsEcommerceModified () const                 { return ecommerceModified; }
    bool    IsMerchandisingModified () const             { return merchandisingModified; }
    bool    IsRelVarModified () const                    { return relVarModified; }
    
    void    SetFirstHitTimeGMT (time_t t)                { modified = trafficModified = true;    firstHitTimeGMT = t; }
    void    SetLastHitTimeGMT (time_t t)                 { modified = trafficModified = true;    lastHitTimeGMT = t; }
    void    SetLastHitTimeVisitorLocal (time_t t)        { modified = trafficModified = true;    lastHitTimeVisitorLocal = t; }
    void    SetLastVisitNum (unsigned i)                 { modified = trafficModified = true;    lastVisitNum = i; }

    void    SetLastPurchaseTimeGMT (time_t t)            { modified = ecommerceModified = true;    lastPurchaseTimeGMT = t; }
    void    SetFirstHitReferrer (std::string const &s)   { modified = ecommerceModified = true;    firstHitReferrer = s; }
    void    SetFirstHitUrl (std::string const &s)        { modified = ecommerceModified = true;    firstHitPageUrl = s; }
    void    SetFirstHitPagename (std::string const &s)   { modified = ecommerceModified = true;    firstHitPagename = s; }
    void    SetLastPurchaseNum (unsigned i)              { modified = ecommerceModified = true;    lastPurchaseNum = i; }

    void    SetMerchandising (std::string const &s)      { modified = merchandisingModified = true;    merchandising = s; }

    // The last N purchase IDs are actually stored in the cookie (N is currently 5)
    bool    SetPurchaseId (std::string const & purchase_id)
    {
        // returns false if purchase_id is already in the list, but move it to the end
        for (PID::iterator i=purchaseIds.begin(); i != purchaseIds.end(); ++i ) {
            if (*i == purchase_id) {
                if (i+1 != purchaseIds.end()) {
                    for (PID::iterator j = i+1; j != purchaseIds.end(); ++j, ++i) {
                        *i = *j;
                    }
                    *i = purchase_id;
                }
                return false;
            }
        }
        if (purchaseIds.size() == NUM_SAVED_PURCHASE_IDS) {
            purchaseIds.pop_front();
        }
        purchaseIds.push_back (purchase_id);
        modified = ecommerceModified = true;
        return true;
    }

    unsigned GetUser () const                                { return userid; }
    unsigned long long GetVisIdHigh () const                 { return visid_high; }
    unsigned long long GetVisIdLow () const                  { return visid_low; }
    time_t    GetFirstHitTimeGMT () const                    { return firstHitTimeGMT; }
    time_t    GetLastHitTimeGMT () const                     { return lastHitTimeGMT; }
    time_t    GetLastHitTimeVisitorLocal () const            { return lastHitTimeGMT; }
    unsigned GetLastVisitNum () const                        { return lastVisitNum; }

    time_t    GetLastPurchaseTimeGMT () const                { return lastHitTimeGMT; }
    std::string const & GetFirstHitReferrer () const         { return firstHitReferrer; }
    std::string const & GetFirstHitUrl () const              { return firstHitPageUrl; }
    std::string const & GetFirstHitPagename () const         { return firstHitPagename; }
    unsigned            GetLastPurchaseNum () const          { return lastPurchaseNum; }

    std::string const & GetMerchandising () const            { return merchandising; }

    unsigned            GetPurchaseIdCount () const          { return static_cast<unsigned> (purchaseIds.size()); }
    std::string const &GetPurchaseId (unsigned index) const  { return purchaseIds.at(index); }

    // ---- e-Var/Rel Var stuff ------------------------------------

    void     ClearVar (RelationId relation_id)
    {
        VarId vid = FindRelationId (relation_id);
        if (vid != VAR_NOT_SET) {
            relVar[vid].var.resize(0);
            relVar[vid].modified = true;
        }
    }
    
    // The return value can be used to set additional instances of the variable when its type is ALLOC_TYPE_LINEAR using the AddLinearElement method.
    // You could also use this method to set additional elements in the linear variable, but it is less efficient.
    // It is expected that elevator would only ever use this method, as it will never set more than one value at a time.
    VarId    SetVar (RelationId relation_id, std::string const &val, time_t timestamp, unsigned char revision, AllocationType allocType, unsigned maxLinear=MAX_LINEAR_NOT_SET)
    {
        if (val.empty()) {
            if (allocType == ALLOC_TYPE_LAST) {
                ClearVar (relation_id);
            }
            return VAR_NOT_SET;
        }
        unsigned pos = FindRelationPos (relation_id);
        VarId vid = static_cast<VarId> (relVar.size());

        if (pos >= vid || relVar[relVarSort[pos]].relation_id != relation_id) {
            // Need to insert new element
            relVar.resize (vid+1);
            relVar[vid].relation_id = relation_id;
            relVarSort.insert (relVarSort.begin() + pos, vid);
        }
        else {
            vid = relVarSort[pos];
            if (allocType == ALLOC_TYPE_FIRST && relVar[vid].var.size() == 1) {
                // value already set
                return VAR_NOT_SET;
            }
        }
        RelVarImpl &rv (relVar[vid]);
        unsigned index;
        if (allocType != ALLOC_TYPE_LINEAR) {
            rv.var.resize(1);
            index = 0;
        }
        else {
            size_t curSize = rv.var.size();
            if (maxLinear != MAX_LINEAR_INFINITE) {
                while (curSize >= maxLinear && curSize > 0) {
                    rv.var.pop_front();
                    --curSize;
                }
            }
            rv.var.resize (curSize + 1);
            index = static_cast<unsigned>(curSize);
        }
        rv.modified = modified = relVarModified = true;
        rv.var[index].value = val;
        rv.var[index].timestamp = timestamp;
        rv.var[index].revision = revision;

        return allocType == ALLOC_TYPE_LINEAR ? vid : VAR_NOT_SET;
    }

    // Optimization avialable to VCookieStore implementations for setting additional elements in a more efficient manner
    void AddLinearElement (VarId id, std::string const &val, time_t timestamp, unsigned char revision)
    {
        if (static_cast<unsigned>(id) >= relVar.size()) {
            // Invalid id
            return;
        }
        RelVarImpl &rv (relVar[id]);
        size_t sz = rv.var.size();
        rv.var.resize (sz+1);

        rv.modified = modified = relVarModified = true;
        rv.var[sz].value = val;
        rv.var[sz].timestamp = timestamp;
        rv.var[sz].revision = revision;
    }

    unsigned GetVarElementCount (RelationId relation_id, VarId *id=0) const // 0 = var not set, 1 = Allocation type of first/last or linear with only one value so far, >1 = Linear
    {
        VarId vid = FindRelationId (relation_id);
        if (id) {
            *id = vid;
        }
         if (vid != VAR_NOT_SET) {
             unsigned sz = static_cast<unsigned> (relVar[vid].var.size());
             if (sz == 0 && id) {
                 *id = VAR_NOT_SET;
             }
             return sz;
        }
         return 0;
    }
    RelVar const* GetVar (RelationId relation_id, unsigned index=0) const
    {
        VarId vid = FindRelationId (relation_id);
        if (vid == VAR_NOT_SET) {
            return 0;
        }
        return GetVar (vid, index);
    }
    RelVar const* GetVar (VarId vid, unsigned index=0) const
    {
        if (static_cast<unsigned>(vid) >= relVar.size() || index >= relVar[vid].var.size()) {
            return 0;
        }
        return &relVar[vid].var[index];
    }

    RelationId GetFirstSetVar () const // return lowest valued relation_id that is set, return -1 if none set
    {
        return GetSetVar (0);
    }
    RelationId GetNextSetVar (RelationId relation_id) const // return -1 if no more are set
    {
        unsigned pos = FindRelationPos (relation_id);
        if (pos < relVarSort.size() && relVar[relVarSort[pos]].relation_id == relation_id ) {
            ++pos;
        }
        return GetSetVar (pos);
    }

    RelationId GetFirstModifiedVar () const // may include relation_ids for vars that were cleared
    {
        return GetModifiedVar (0);
    }
    RelationId GetNextModifiedVar (RelationId relation_id) const
    {
        unsigned pos = FindRelationPos (relation_id);
        if (pos < relVarSort.size() && relVar[relVarSort[pos]].relation_id == relation_id ) {
            ++pos;
        }
        return GetModifiedVar (pos);
    }
    
private:
    RelationId GetSetVar (unsigned pos) const
    {
        while (pos < relVarSort.size() && relVar[relVarSort[pos]].var.size() == 0) {
            ++pos;
        }
        if (pos < relVarSort.size()) {
            return relVar[relVarSort[pos]].relation_id;
        }
        return INVALID_RID;
    }
    RelationId GetModifiedVar (unsigned pos) const
    {
        while (pos < relVarSort.size() && !relVar[relVarSort[pos]].modified) {
            ++pos;
        }
        if (pos < relVarSort.size()) {
            return relVar[relVarSort[pos]].relation_id;
        }
        return INVALID_RID;
    }
    VarId FindRelationId (RelationId relation_id) const
    {
        unsigned pos = FindRelationPos (relation_id);
        return (pos < relVarSort.size()) ? relVarSort[pos] : VAR_NOT_SET;
    }

    unsigned FindRelationPos (RelationId relation_id) const
    {
        unsigned low=0;
        unsigned high = static_cast<unsigned> (relVarSort.size());
        while (low < high) {
            unsigned mid = (low + high) / 2;
            unsigned vid = relVar[relVarSort[mid]].relation_id;
            if (vid == relation_id) {
                return mid;
            }
            if (vid < relation_id) {
                if (mid == low) {
                    return high;
                }
                low = mid;
            }
            else {
                high = mid;
            }
        }
        return low;
    }
    
    // disallow copying - it shouldn't be needed
    VCookie(VCookie const &);
    VCookie const &operator = (VCookie const &);

    struct RelVarImpl {
        RelationId relation_id;
        bool modified;
        std::deque<RelVar> var;
    };

    bool newCookie;
    bool modified;
    unsigned userid;
    unsigned long long visid_high;
    unsigned long long visid_low;

    // Traffic Info
    time_t       lastHitTimeGMT;
    time_t       lastHitTimeVisitorLocal;
    time_t       firstHitTimeGMT;
    unsigned     lastVisitNum;
    bool         trafficModified;

    // Ecommerce Info
    std::string  firstHitReferrer;
    std::string  firstHitPageUrl;
    std::string  firstHitPagename;
    time_t       lastPurchaseTimeGMT;
    unsigned     lastPurchaseNum;
    
    typedef std::deque<std::string> PID;
    PID          purchaseIds;
    bool         ecommerceModified;

    std::string  merchandising;
    bool         merchandisingModified;

    bool         relVarModified;
    std::vector<RelVarImpl> relVar;
    std::vector<VarId> relVarSort;

    VCookieStore &vstore;
};

inline bool operator == (VCookie const &a, VCookie const &b)
{
    if (a.GetUser() != b.GetUser() ||
        a.GetVisIdHigh() != b.GetVisIdHigh() ||
        a.GetVisIdLow() != b.GetVisIdLow())
    {
        return false;
    }
    if (a.GetLastHitTimeGMT()           != b.GetLastHitTimeGMT() ||
        a.GetLastHitTimeVisitorLocal()  != b.GetLastHitTimeVisitorLocal() ||
        a.GetFirstHitTimeGMT()          != b.GetFirstHitTimeGMT() ||
        a.GetLastVisitNum()             != b.GetLastVisitNum())
    {
        return false;
    }
    if (a.GetFirstHitReferrer() != b.GetFirstHitReferrer() ||
        a.GetFirstHitUrl()      != b.GetFirstHitUrl() ||
        a.GetFirstHitPagename() != b.GetFirstHitPagename() ||
        a.GetLastPurchaseNum()  != b.GetLastPurchaseNum() ||
        a.GetPurchaseIdCount()  != b.GetPurchaseIdCount())
    {
        return false;
    }
    if (a.GetMerchandising()    != b.GetMerchandising()) {
        return false;
    }
    unsigned count = a.GetPurchaseIdCount();
    for (unsigned i=0; i < count; ++i) {
        if (a.GetPurchaseId(i) != b.GetPurchaseId(i)) {
            return false;
        }
    }
    VCookie::RelationId ar = a.GetFirstSetVar();
    VCookie::RelationId br = b.GetFirstSetVar();
    if (ar != br) {
        return false;
    }
    
    VCookie::VarId av, bv;
    while (ar != VCookie::INVALID_RID) {
        unsigned ac = a.GetVarElementCount(ar, &av);
        unsigned bc = b.GetVarElementCount(br, &bv);
        
        if (ac != bc) {
            return false;
        }
        for (unsigned i=0; i < ac; ++i) {
            const VCookie::RelVar *arv = a.GetVar (av, i);
            const VCookie::RelVar *brv = b.GetVar (bv, i);
            if (arv->value != brv->value ||
                arv->timestamp != brv->timestamp ||
                arv->revision != brv->revision)
            {
                return false;
            }
        }
        ar = a.GetNextSetVar (ar);
        br = a.GetNextSetVar(br);
        if (ar != br) {
            return false;
        }
    }
    return true;
}

inline bool operator != (VCookie const &a, VCookie const &b) { return !(a == b); }

#endif // VCOOKIE_HDR
