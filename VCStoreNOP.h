// this is the No Operation (NOP) version of the store
// it doesn't do anything but is useful for testing the max speed
// of the test harness
class VCStoreNOP: public VCookieStore
{
public:
   
	virtual bool SaveVCookie (VCookie const &vcookie)
    {
        return true;
    }
	virtual bool LoadVCookie (VCookie &vcookie)
    {
		return false;
    }
    virtual unsigned long long DeleteOldVCookies (time_t t)
    {
        return 0;
    }
	
	virtual bool DeleteVCookie (VCookie &vcookie)
    {
        return false;
    }
	virtual unsigned long long GetVCookieCount () const
    {
        return 0;
    }
    
	virtual bool GetVCookie (VCookie &vcookie, unsigned long long index) const
    {
		return false;
    }	
};

