#ifndef VCOOKIE_USER_H
#define VCOOKIE_USER_H

#include "vcookie.h"
#include "user.h"

// This interface is intended for use by the elevator code so hide the User (report suite) specific logic for evar/Relation Id variables.
// The User data structure defined in the elevator code has calls to determine the allocation type for these variables and for linear variables,
// calls to determine the maximum number of linear items to keep. 
// The SetVar method here simplifies using the regular VCookie class implementation by hiding that additional complexity from the user of this class.
// This class is not implemented for the prototype, but is expected to be used when the code is used by elevator.
// When implemented, SetVar simply looks up the appropriate allocation type and then turns around and calls VCookie::SetVar, except for non-evars that
// don't have a linear allocation. For those it simply returns without doing anything as they don't need to be persisted by the vcookie.

class VCookieUser : public VCookie {
public:
	VCookieUser (
		const User &user;
		const std::string &visid_high,
		const std::string &visid_low,
		bool newVisitor,  // set if we know this is a new visitor who will have no record in the database. If false, visitor still may not have a record.
		VCookieStore &store
		);
	
	// All of the functions below have identical interfaces
	~VCookieUser () {}
	
	// This is similar to the SetVar method of VCookie, but has a different signature
	// (it doesn't have parameters to set the allocationType or the maxLinear values).
	// This method gets the appropriate values from the User class and passes them to the VCookie versions
	void 	SetVar (int relation_id, std::string const &val, time_t timestamp, int revision);

private:
};
#endif // VCOOKIE_USER_H