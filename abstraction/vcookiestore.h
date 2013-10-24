#ifndef VCOOKIE_STORE_HDR
#define VCOOKIE_STORE_HDR

#include "time.h"
#include <vector>

class VCookie;

typedef void (*VCookieProcessedCallback)(bool success, const VCookie &cookie);

class VCookieStore {
public:
    virtual ~VCookieStore () {}

    /**
     * Save a number of cookies in a batched operation.
     *
     * @param cookies the batch of cookies to store
     * @param callback a callback function to call for each cookie
     *                 to track the status of each individual cookie
     */
    virtual void SaveVCookies(std::vector<const VCookie*> &cookies,
                              VCookieProcessedCallback callback) {
        std::vector<const VCookie*>::iterator ii;
        for (ii = cookies.begin(); ii != cookies.end(); ++ii) {
            const VCookie& cookie = *(*ii);
            bool success = SaveVCookie(cookie);
            if (callback != NULL) {
                callback(success, cookie);
            }
        }
    }

    /**
     * Batch load a number of cookies
     *
     * @param cookies the batch of cookies to load
     * @param callback a callback function to call for each cookie
     *                 to track the status of each individual cookie
     */
    virtual void LoadVCookies(std::vector<VCookie*> &cookies,
                              VCookieProcessedCallback callback) {
        std::vector<VCookie*>::iterator ii;
        for (ii = cookies.begin(); ii != cookies.end(); ++ii) {
            VCookie& cookie = *(*ii);
            bool success = LoadVCookie(cookie);
            if (callback != NULL) {
                callback(success, cookie);
            }
        }
    }

    virtual bool SaveVCookie (VCookie const &vcookie) = 0;

    // Only VCookie::VCookie should ever call LoadVCookie
    virtual bool LoadVCookie (VCookie &vcookie) = 0;

    // returns the number of cookies that were deleted (if determining
    // the number is expensive we can probably change the return type
    // to void) Solutions have a lot of ways to implement
    // this. Deletes really only need to happen once per month, so a
    // solution could create a new DB/table each month and any new or
    // updated vcookies are written to this new table. After 12
    // months, simply delete the oldest table.  However, this does
    // require more space, because active users will have versions of
    // their vcookies in multiple tables. The size increase depends on
    // how many visitors return regularly and how long cookies survive
    // in their browser environment.
    virtual unsigned long long DeleteOldVCookies (time_t purgeOlderThanThis) = 0;

    // These functions are not required. We don't currently have a use
    // case for them but they may facilitate testing. When we deploy a
    // final implemantion, these will likely not be part of it, and
    // therefore efficiency is not a major concern for these
    // functions.
    virtual bool DeleteVCookie (VCookie &vcookie) = 0;
    virtual unsigned long long GetVCookieCount () const = 0;
    virtual bool GetVCookie (VCookie &vcookie, unsigned long long index) const = 0;

    // If a VCookie implementation uses Key/Value pairs, it can use
    // these serialization functions for the value portion The key
    // would be the userid/visid. We will likely optimize these in the
    // future, possibly including compression of the data blob.  Since
    // "last hit time" is used to expire a cookie, most implemenations
    // will probably want to store it separately from the blob so that
    // they can search/delete old vcookies without having to
    // deserialize the whole vcookie
    static void Serialize (VCookie const &vcookie, std::vector<char> &buffer, bool saveLastHitTime=true);
    static bool Deserialize (VCookie &vcookie, const std::vector<char> &buffer);
};
#endif // VCOOKIE_STORE_HDR
