/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */

/**
 * Implementation of the VCookieStore that utilize Couchbase to store the
 * objects in.
 *
 * @author Trond Norbye
 */
#ifndef LCB_STORE_H
#define LCB_STORE_H

#include "abstraction/vcookie.h"
#include "abstraction/vcookiestore.h"
#include <libcouchbase/couchbase.h>

class VCCouchbaseStore: public VCookieStore
{
public:
    VCCouchbaseStore();
    virtual ~VCCouchbaseStore();


    void SaveVCookies(std::vector<const VCookie*> &cookies,
                      VCookieProcessedCallback callback);
    virtual void LoadVCookies(std::vector<VCookie*> &cookies,
                              VCookieProcessedCallback callback);

	virtual bool SaveVCookie(VCookie const &vcookie);
	virtual bool LoadVCookie(VCookie &vcookie);
	virtual bool DeleteVCookie(VCookie &vcookie);
    virtual unsigned long long DeleteOldVCookies(time_t t);
	virtual unsigned long long GetVCookieCount() const;
	virtual bool GetVCookie(VCookie &vcookie, unsigned long long index) const;

private:
    lcb_t instance;
};

#endif
