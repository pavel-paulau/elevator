#include <pthread.h>

#include <time.h>

#include "abstraction/vcookiestore.h"
#include "abstraction/vcookie.h"

#define _TOSTRING(x) #x
#define TOSTRING(x) _TOSTRING(x)
#define _APPEND_H(x) x.h
#define APPEND_H(x) _APPEND_H(x)

// change for different storage engine
#if !defined(STORAGE_ENGINE)
	#define STORAGE_ENGINE	VCStoreInMemory
#endif

#include TOSTRING(APPEND_H(STORAGE_ENGINE))

#include <iostream>
#include <fstream>
#include <iterator>

#include <map>

#include <boost/cerrno.hpp>
#include <boost/utility.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include <boost/tokenizer.hpp>

#include <boost/regex.hpp>


// options processing
#include <boost/program_options.hpp>
namespace po = boost::program_options;

using namespace std;
using namespace boost;

// http://stefaanlippens.net/gearman_setting_worker_process_arguments_through_xargs
// how to pass command line options through gearman app

// run gearman server daemon and detatch from the console
// gearmand -d

// run gearman worker in the background
// gearman -w -f vcookieTestHarness ./testharness &

// submit a job to the worker
// gearman -f vcookieTestHarness < test.conf


const string VERSION_STRING	= "1.2";

// these constants define the number of the constant in a second
// a nanosecond is a billionth of a second so there are 1B in a second
const unsigned long NANOSECOND =  1000000000;	// billion
const unsigned long MICROSECOND =    1000000;	// million
const unsigned long MILLISECOND =       1000;	// thousand

pid_t parentPid;

pthread_mutex_t	fileReadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	consoleMutex = PTHREAD_MUTEX_INITIALIZER;

class hitSource;

/*
 * parameters passed to each thread
 * implemented as a struct in case we need to pass more stuff
 */
typedef struct
{
	int				pid;	// child thread ID
	hitSource 		*hits;	// the source of hits
	vector<unsigned long>	*aggregateRate;	// parents accumulated rates
	vector<unsigned long>	*aggregateReadTimer;
	vector<unsigned long>	*aggregateWriteTimer;
} threadParam_t;

/*
 * Options set by the configure reader
 */
struct options_t
{
	unsigned threads;
	int verbose;
	unsigned requests;
	unsigned long requestRate;
	unsigned long randomSeed;
	string configFile;
	float	replayRate;
	vector<string> replayFiles;
} options;


// little helper function to subtract two timespec values
// returns difference in nanoseconds
unsigned long operator-(struct timespec &left, struct timespec &right)
{
	// if left is less than right, return 0
	if (left.tv_sec < right.tv_sec ||
			(left.tv_sec == right.tv_sec && left.tv_nsec < right.tv_nsec))
		return 0L;
	
	return ((left.tv_sec - right.tv_sec) * NANOSECOND) +
			left.tv_nsec - right.tv_nsec;
}

// use this to control the rate of generation of requests
// this class is explicitly not copyable at the moment
class rateControl : private boost::noncopyable
{
	public:
		rateControl(unsigned long _eventsPerSecond) : mode(EVENT_BASED), eventsPerSecond(_eventsPerSecond) { Start(); }
		rateControl(float _replayRate) : mode(HIT_BASED), firstHitTime(0), replayRate(_replayRate) { }
		~rateControl(void) {}
		
	private:
		unsigned long	eventsCount;		// how many events have we seen

		// which way are we running the clock
		enum ClockType
		{
			EVENT_BASED = 1,
			HIT_BASED = 2
		} mode;
		
		// for EVENT_BASED clocks
		struct timespec clockPrev;			// the previous clock value
		unsigned long	eventsPerSecond;	// 0 = no limits

		// for HIT_BASED clocks
		time_t			firstHitTime;		// virtual time we started (for playback rate)
		time_t			startClockTime;		// what local clock time we started
		float			replayRate;			// multiplier for how fast to play back (1=100%)
	
		
	public:
		// start the timer
		void Start(void)
		{
			clock_gettime(CLOCK_REALTIME, &clockPrev);
			eventsCount = 0;
		}
		
		// increment the count of events and sleep until caller
		// should run again based on rate previously set
		void IncrementAndWait(unsigned long howMany, time_t hit_time_gmt)
		{
			eventsCount += howMany;
			
			if (mode == HIT_BASED)
			{
				if (firstHitTime == 0)
				{
					firstHitTime = hit_time_gmt;
					startClockTime = time(NULL);
					return;		// no delay for the first hit
				}
				
				// calculate the time for the next hit based on our virtual
				// clock and the replay rate
				
				// how many seconds have actually elapsed since we started
				// times the replay rate
				// plus the time of the first hit
				// equals the current 'virtual' time
				// if that is less than the next hit time, we need to wait
				// because our virtual clock hasn't caught up to the next hit yet
				time_t	virtualTime;
				while((virtualTime = ((int)((time(NULL) - startClockTime) * replayRate) + firstHitTime)) < hit_time_gmt)
				{
					// how many real seconds do we need to wait?
					long	sleepSeconds = (int)((hit_time_gmt - virtualTime) / replayRate);
					if (sleepSeconds > 1)
						usleep(MICROSECOND * (sleepSeconds-1));	// 2 or more: sleep for 1 less
					else
						pthread_yield();	// just yield and loop for the last fractional second
				}
			}
			else
			{
				// run "wide open" with no restrictions
				if (eventsPerSecond == 0)
					return;

				struct timespec clockNow;
				clock_gettime(CLOCK_REALTIME, &clockNow);
				
				// while nanoseconds per event * number of events so far 
				//		(how much time we should have spent for this many events)
				// is greater than the elapsed time
				// we should wait
				while ((NANOSECOND / eventsPerSecond) * eventsCount 
						> clockNow - clockPrev)
				{
					// calculate the time remaining before we go again in microseconds
					unsigned long uSecDiff = 
							((NANOSECOND / eventsPerSecond) * eventsCount 
							- (clockNow - clockPrev)) / 1000;	// NANOSECOND / 1000 = MICROSECOND

					if (uSecDiff)
						usleep(uSecDiff);	// sleep so we don't waste cycles reading the clock
					else
						pthread_yield();	// let others run

					clock_gettime(CLOCK_REALTIME, &clockNow);
				}
			}
		}

		// getter
		unsigned long EventsPerSecond(void) const
		{
			return eventsPerSecond;
		}
		
		// setter, 0 = no limit
		unsigned long EventsPerSecond(unsigned long _eventsPerSecond)
		{
			eventsPerSecond = _eventsPerSecond;
				
			return eventsPerSecond;
		}
};	// class rateControl


// use this to monitor the rate of requests
// this class is explicitly not copyable at the moment
class rateMonitor : private boost::noncopyable
{
	public:
		rateMonitor(void) {
			eventsPerSecond.reserve(60*60);		// preallocate space for 1 hour of records
			Start(); 
		}
		~rateMonitor(void) {}
		
	private:
		struct timespec clockPrev;			// the previous clock value
											// if clockPrev.tv_sec == 0, clock not set
		vector<unsigned long>	eventsPerSecond;
		unsigned long	eventsCount;		// events this second
	
	public:
		// start the timer
		void Start(void)
		{
			clock_gettime(CLOCK_REALTIME, &clockPrev);
			eventsCount = 0;
		}
		
		// increment the count of events
		void Increment(unsigned long howMany = 1)
		{
			if (clockPrev.tv_sec == 0)
				Start();

			struct timespec clockNow;
			clock_gettime(CLOCK_REALTIME, &clockNow);

			// has it been more than a second
			if (clockNow - clockPrev > NANOSECOND)
			{
				// create 1 entry for each full second
				while (clockNow - clockPrev > NANOSECOND)
				{
					// add a new entry to the vector with the current count per second
					// multiply the event count * 1B, 
					//	then divide by the clock difference in nanoseconds
					// this preserves the maximum accuracy when using integer math
					eventsPerSecond.push_back(eventsCount);
					
					// reset event count to 0 for next second
					eventsCount = 0;
					
					// increment seconds on start clock
					clockPrev.tv_sec += 1;
				}
			}
			
			eventsCount += howMany;
		}

		// stop the clock and tally up the final second(s) if any
		void Stop(void)
		{
			struct timespec clockNow;
			clock_gettime(CLOCK_REALTIME, &clockNow);

			// the following code is a little tricky
			// in reality we will EITHER execute the code in the WHILE
			// OR execute the code in the IF
			// OR maybe neither (unlikely, but possible), but never both!
			
			// create 1 entry for each full second
			while (clockNow - clockPrev > NANOSECOND)
			{
				// add a new entry to the vector with the current count per second
				eventsPerSecond.push_back(eventsCount);
				
				// reset event count to 0 for next second
				eventsCount = 0;

				// increment seconds on start clock
				clockPrev.tv_sec += 1;
			}
			
			// add 1 more entry for the last partial second (if > 5% of a second)
			if (eventsCount > 0 && clockNow - clockPrev > NANOSECOND / 20)
				// multiply the event count * 1B, 
				//	then divide by the clock difference in nanoseconds
				// this preserves the maximum accuracy when using integer math
				eventsPerSecond.push_back((eventsCount * NANOSECOND) / (clockNow - clockPrev));
				
			eventsCount = 0;
			clockPrev.tv_sec = 0;	// flag that clock is not set
		}
		
		// return the rate of events per second
		const vector<unsigned long> EventsPerSecond(void) const
		{
			return eventsPerSecond;
		}
};	// class rateMonitor



// use high resolution timing to accumulate time
// this class is explicitly not copyable at the moment
class hiResTimer : private boost::noncopyable
{
	public:
		hiResTimer(void) : eventsCount(0), ns(0) {
			nsPerSecond.reserve(60*60);		// preallocate space for 1 hour of records
		}
		~hiResTimer(void) {}
		
	private:
		struct timespec clockStart;			// start of this timing block
		struct timespec clockPrev;			// the previous clock value for seconds
											// if clockPrev.tv_sec == 0, clock not set
		unsigned long	eventsCount;		// events this second
		unsigned long	ns;					// cumulative count of ns this second
		
		vector<unsigned long>	nsPerSecond;	// each entry is the average number of nanoseconds counted in that second
		
	public:
		// start the timer
		void Start(void)
		{
			if (clockPrev.tv_sec == 0)
				clock_gettime(CLOCK_REALTIME, &clockPrev);
			clock_gettime(CLOCK_REALTIME, &clockStart);
		}
		
		// increment the count of events and ns
		// update the vector if more than 1s has elapsed
		void Stop()
		{
			struct timespec clockNow;
			clock_gettime(CLOCK_REALTIME, &clockNow);

			// has it been more than a second since the last update
			if (clockNow - clockPrev > NANOSECOND)
			{
				// create 1 entry for each full second
				while (clockNow - clockPrev > NANOSECOND)
				{
					// add a new entry to the vector with the average ns
					nsPerSecond.push_back(ns / (eventsCount == 0 ? 1 : eventsCount));
					
					// reset event count to 0 for next second
					eventsCount = 0;
					
					// reset ns to 0 as well
					ns = 0;
					
					// increment seconds on start clock
					clockPrev.tv_sec += 1;
				}
			}
			
			// update number of events and ns sum for this second
			eventsCount += 1;
			ns += clockNow - clockStart;
		}

		// return the average ns for each second
		const vector<unsigned long> NsPerSecond(void)
		{
			// see if we need to update the last second
			struct timespec clockNow;
			clock_gettime(CLOCK_REALTIME, &clockNow);

			// the following code is a little tricky
			// in reality we will EITHER execute the code in the WHILE
			// OR execute the code in the IF
			// OR maybe neither, but never both!
			
			// create 1 entry for each full second we may have missed
			while (clockNow - clockPrev > NANOSECOND)
			{
				// add a new entry to the vector with the average ns
				nsPerSecond.push_back(ns / (eventsCount == 0 ? 1 : eventsCount));
				
				// reset event count to 0 for next second
				eventsCount = 0;
				
				// reset ns to 0 as well
				ns = 0;
				
				// increment seconds on start clock
				clockPrev.tv_sec += 1;
			}
			
			// add 1 more entry for the last partial second (if any)
			if (eventsCount > 0)
				nsPerSecond.push_back(ns / eventsCount);
				
			return nsPerSecond;
		}
};	// class hiResTimer


typedef struct
{
	unsigned		rsid;				// report suite for this hit
	
	unsigned long	visid_high;
	unsigned long	visid_low;
	bool			visid_new;			// are we SURE this is a new visid
	time_t			hit_time_gmt;
	unsigned long	visit_num;
	string			referrer;
	string			page_url;
	string			page_name;			// defaults to page URL
	time_t			purchase_time_gmt;
	string			purchaseid;
	string			campaign;
	
	string			evar[75];
} hitData_t;



// Pure virtual base class used as an interface
// set it as not copyable
class hitSource : private boost::noncopyable
{
	public:
		hitSource(void) { }
		virtual ~hitSource(void);
		
		virtual bool NextHit(hitData_t &hit) = 0;
};	// class hitSource

// base dtor doesn't need to do anything
hitSource::~hitSource(void) {}


// implementation of hitSource for data warehouse files
class dwfileHitSource : public hitSource
{
	private:
		ifstream	infile;
		pthread_mutex_t	fileReadMutex;
		map<string,int>	fieldMap;
		vector<string> filenames;
		vector<string>::iterator currFilename;

		// precalculated field offsets
		int		f_userid,
				f_visid_new,
				f_visid_type,
				f_visid_high,
				f_visid_low,
				f_hit_time_gmt,
				f_visit_num,
				f_referrer,
				f_page_url,
				f_pagename,
				f_last_purchase_time_gmt,
				f_purchaseid,
				f_campaign,
				f_evar1;
				
		// how many fields on each line
		unsigned int		fieldCount;
				
		// stupid-simple hash function stolen from Stroustrup's book
		static unsigned hash(const string str)
		{
			unsigned h = 0;
			for (unsigned int i = 0; i < str.length(); i++)
				h = (h << 1) ^ str[i];
				
			return h;
		}
		
		void openNextFile()
		{
			if (infile.is_open())
				infile.close();

			if (currFilename < filenames.end())
			{
				infile.open((*currFilename++).c_str());
			}
		}
		
	public:
		dwfileHitSource(vector<string> files, pthread_mutex_t &readMutex) : fileReadMutex(readMutex)
		{
			// setup hash map of field names to index
			// assumes a specific format to the dw export file
			string fieldNames = "userid,visid_new,visid_type,post_visid_high,post_visid_low,hit_time_gmt,visit_num,post_referrer,post_page_url,post_pagename,last_purchase_time_gmt,post_purchaseid,post_campaign,post_evar1,post_evar2,post_evar3,post_evar4,post_evar5,post_evar6,post_evar7,post_evar8,post_evar9,post_evar10,post_evar11,post_evar12,post_evar13,post_evar14,post_evar15,post_evar16,post_evar17,post_evar18,post_evar19,post_evar20,post_evar21,post_evar22,post_evar23,post_evar24,post_evar25,post_evar26,post_evar27,post_evar28,post_evar29,post_evar30,post_evar31,post_evar32,post_evar33,post_evar34,post_evar35,post_evar36,post_evar37,post_evar38,post_evar39,post_evar40,post_evar41,post_evar42,post_evar43,post_evar44,post_evar45,post_evar46,post_evar47,post_evar48,post_evar49,post_evar50,post_evar51,post_evar52,post_evar53,post_evar54,post_evar55,post_evar56,post_evar57,post_evar58,post_evar59,post_evar60,post_evar61,post_evar62,post_evar63,post_evar64,post_evar65,post_evar66,post_evar67,post_evar68,post_evar69,post_evar70,post_evar71,post_evar72,post_evar73,post_evar74,post_evar75";

			vector<string>	fields;
			boost::split(fields, fieldNames, is_any_of(","));
			unsigned int i = 0;
			for (vector<string>::iterator curr = fields.begin();
					curr < fields.end();
					curr++)
				fieldMap[*curr] = i++;
			fieldCount = i;		// how many fields should there be on each line

			// extract all the field numbers into variables (saves lookup time on each line)
			f_userid = fieldMap["userid"];
			f_visid_new = fieldMap["visid_new"];
			f_visid_type = fieldMap["visid_type"];
			f_visid_high = fieldMap["post_visid_high"];
			f_visid_low = fieldMap["post_visid_low"];
			f_hit_time_gmt = fieldMap["hit_time_gmt"];
			f_visit_num = fieldMap["visit_num"];
			f_referrer = fieldMap["post_referrer"];
			f_page_url = fieldMap["post_page_url"];
			f_pagename = fieldMap["post_pagename"];
			f_last_purchase_time_gmt = fieldMap["last_purchase_time_gmt"];
			f_purchaseid = fieldMap["post_purchaseid"];
			f_campaign = fieldMap["post_campaign"];
			f_evar1 = fieldMap["post_evar1"];

			// setup files array and iterator
			filenames = files;
			currFilename = filenames.begin();
			
			// first file will be opened on call to NextHit
			// openNextFile();
		}
		~dwfileHitSource(void) 
		{ 
			infile.close();
		}
	
		virtual bool NextHit(hitData_t &hit);
};	// class dwfileHitSource

bool dwfileHitSource::NextHit(hitData_t &hit)
{
	pthread_mutex_lock(&fileReadMutex);
	
	// step to the next file if necessary
	if (!infile.is_open() || infile.eof())
		openNextFile();

	// should be good now, if there is any data left at all
	if (infile.is_open() && infile.good() && !infile.eof())
	{
		string line;
		
		getline(infile, line);

		// I know unlocking inside a conditional is not the safest way, but
		// it was much better than doing two tests.
		pthread_mutex_unlock(&fileReadMutex);

		if (infile.eof() && line.length() == 0)
			return NextHit(hit);

		string slash = "\\\\";	// escaped version of a slash
		string tab = "\\t";		// escaped version of a tab
		// account for escaped tabs in the line
		string escapedLine = boost::regex_replace(line, boost::regex(slash + slash + tab), string(" "));
		
		// split into fields
		vector<string>	fields;
		boost::split(fields, escapedLine, is_any_of("\t"));

		if (fields.size() != fieldCount)
		{
			if (0)
			{
				pthread_mutex_lock(&consoleMutex);
				cout << "\n\n***AbEnd: Field count " << fields.size() 
						<< " does not match anticipated count " << fieldCount << "\n"
						<< "\tLine = '" << boost::regex_replace(line, boost::regex(tab), string("|")) << "'\n"
						<< "\tEscaped line='" << boost::regex_replace(escapedLine, boost::regex(tab), string("|")) << "'\n\n";
				pthread_mutex_unlock(&consoleMutex);
				exit(1);
			}
			return NextHit(hit);
		}
		
		hit.rsid = lexical_cast<unsigned long>(fields[f_userid]);
		
		hit.visid_high = lexical_cast<unsigned long>(fields[f_visid_high]);
		hit.visid_low = lexical_cast<unsigned long>(fields[f_visid_low]);
		
		// rule according to Glen and Luby is that visid new is only valid
		// if the visid type is 3 (server generated)
		hit.visid_new = lexical_cast<unsigned long>(fields[f_visid_type]) == 3 && lexical_cast<char>(fields[f_visid_new]) == 'Y';
		
		hit.hit_time_gmt = lexical_cast<unsigned long>(fields[f_hit_time_gmt]);	// post_cust_hit_time_gmt?
		hit.visit_num = lexical_cast<unsigned long>(fields[f_visit_num]);
		hit.referrer = fields[f_referrer];
		hit.page_url = fields[f_page_url];
		hit.page_name = fields[f_pagename];
		hit.purchase_time_gmt = lexical_cast<unsigned long>(fields[f_last_purchase_time_gmt]);
		hit.purchaseid = fields[f_purchaseid];
		hit.campaign = fields[f_campaign];
		
		for (int i = 0; i < 75; i++)
		{
			hit.evar[i] = fields[f_evar1 + i];
		}

		return true;
	}

	// unlock here in the case that the file was closed, eof, etc.
	pthread_mutex_unlock(&fileReadMutex);
	
	// fail: file not open, EOF, etc.
	return false;
}

/*
 * Read config info
 */

// A helper function to simplify vector output.
template<class T>
ostream& operator<<(ostream& os, const vector<T>& v)
{
    copy(v.begin(), v.end(), ostream_iterator<T>(cout, " ")); 
    return os;
}

int Configure(int ac, char* av[])
{
    try {
        // Declare a group of options that will be 
        // allowed only on command line
        po::options_description commandLine("Command line options");
        commandLine.add_options()
            ("version", "display version string")
            ("help", "display help")
            ("verbose", po::value<int>(&options.verbose)->default_value(1), 
					"how noisy to be (0=errors only, 5=very chatty)")
            ;
    
        // Declare a group of options that will be 
        // allowed in the config file
        po::options_description config("Config file options");
        config.add_options()
            ("threads", po::value<unsigned>(&options.threads)->default_value(10), 
					"threads to create")
            ("requests", po::value<unsigned>(&options.requests)->default_value(10000),
					"requests to submit (per thread)")
            ("request-rate", po::value<unsigned long>(&options.requestRate)->default_value(1000),
					"requests per second (per thread), 0 = unlimited")
            ("replay-rate", po::value<float>(&options.replayRate)->default_value(0),
					"rate multiplier for playback from replay file (0.5=50%, 2=200%, 1=100%)")
            ("replay-file", 
					po::value< vector<string> >(&options.replayFiles)
					->composing(), 
					"recorded requests file to replay (multiple allowed)")
            ;

        // Hidden options will not be shown to the user.
        po::options_description hidden("Hidden options");
        hidden.add_options()
            ("config-file", po::value<string>(&options.configFile), "config file (defaults to stdin)")
            ;

        
        po::options_description cmdline_options;
        cmdline_options.add(commandLine).add(hidden);

        po::options_description config_file_options;
        config_file_options.add(config);

        po::options_description visible("Allowed options");
        visible.add(commandLine).add(config);
        
        po::positional_options_description p;
        p.add("config-file", -1);
        
        po::variables_map vm;
        store(po::command_line_parser(ac, av).
              options(cmdline_options).positional(p).run(), vm);

        if (vm.count("help")) {
            cout << visible << "\n";
            exit(0);
        }

        if (vm.count("version")) {
            cout << "Adobe Visitor Profile Storage Test Harness version " 
				<< VERSION_STRING 
				<< " (built " << __DATE__ << " " << __TIME__ << ")"
				<< "\n";
            exit(0);
        }
		notify(vm);

		if (vm.count("config-file"))
		{
			if (options.verbose >= 1)
				cout << "Reading config from " << options.configFile << "\n";

			ifstream ifs(options.configFile.c_str());
			store(parse_config_file(ifs, config_file_options), vm);
		}
		else
		{
			if (options.verbose >= 1)
				cout << "Reading config from stdin\n";

			// read from stdin for config file
			store(parse_config_file(cin, config_file_options), vm);
		}
		notify(vm);
    
        if (vm.count("include-path"))
        {
            cout << "Include paths are: " 
                 << vm["include-path"].as< vector<string> >() << "\n";
        }
    }
    catch(std::exception& e)
    {
        cout << e.what() << "\n";
        return 1;
    }    
    return 0;
}


/*
 * The actual worker process (forked as a child thread)
 */
void *WorkerThread(void *threadArg)
{
	threadParam_t *threadParam = (threadParam_t *) threadArg;

	pthread_mutex_lock(&consoleMutex);
	cout << parentPid << ": " << "Child " << threadParam->pid << " is alive at " << time(NULL) << "\n";
	cout << flush;
	pthread_mutex_unlock(&consoleMutex);

	hitSource	*hits = threadParam->hits;

	VCookieStore	*store = new STORAGE_ENGINE();		// change for different storage engine
	//VCookieStore	*store = new VCStoreNOP();		// change for different storage engine

	hiResTimer	readTimer,
				writeTimer;

	rateControl *controller;
	if (options.replayRate > 0)
		controller = new rateControl(options.replayRate);
	else
		controller = new rateControl(options.requestRate);
	rateMonitor monitor;
	
	controller->Start(); monitor.Start();
	for (unsigned i = 0; i < options.requests; i++)
	{
		if (hits)
		{
			hitData_t	hit;
			
			if (hits->NextHit(hit))
			{
				monitor.Increment(1);
				controller->IncrementAndWait(1, hit.hit_time_gmt);

				readTimer.Start();
				VCookie	cookie(hit.rsid, hit.visid_high, hit.visid_low, hit.visid_new, *store);
				readTimer.Stop();
				
				if (cookie.IsNewCookie())
				{
					cookie.SetFirstHitTimeGMT(hit.hit_time_gmt);
					cookie.SetFirstHitReferrer(hit.referrer);
					cookie.SetFirstHitUrl(hit.page_url);
					cookie.SetFirstHitPagename(hit.page_name);
				}
				cookie.SetLastHitTimeGMT(hit.hit_time_gmt);
				
				cookie.SetLastHitTimeVisitorLocal(hit.hit_time_gmt - 7*60*60);
				cookie.SetLastVisitNum(hit.visit_num);

				if (hit.purchase_time_gmt)
					cookie.SetLastPurchaseTimeGMT(hit.purchase_time_gmt);
					
				//cookie.SetMerchandising ( "asl;dfkjasd;flkjasdf;lkjasdf;lkajsdf;lkasjdf;laksdjf;alksdjfa;lksdjfa;lksdjfa;lsdkfja;lsdkfjas;ldkfjas;ldfkja;lsdkfja;sldkfjals;dkfjas;ldkfjasl;kdfjasl;kdfjas;ldkfjas;ldkfjasl;dkfja;sldkfjasl;kdjfal;skdjfasl;kdjfals;dkfjasl;dkfjasl;kdjqert;lkajsdg;laketn;aksdbnvxc;kgnasd;tkjnasd;gkjncb;akjsdnta;skdjgnb;lkdgjans;ckvjnad;ksfgajsd;flkgjvnc;kajsdgfna;skdfnv;kcxbm,vna;skedjtrhnasd;kgjvnxcz;kjgadnskgljbzncvlkjsmdng;kjdfnlkagsdmng;dkljcmnga;sdkjfnav;lckxj,mfgna;ldskgj;zldsvmbn;zxlkfmnfd;kglb,mnzx;flkjads,mnf;alksd,mgnvbc;kxj,fmasjdn;lkfjasnd;vkjnasfx;");
				
				if (hit.purchaseid.length() > 0)
				{
					cookie.SetLastPurchaseNum(cookie.GetLastPurchaseNum() + 1);
					cookie.SetPurchaseId(hit.purchaseid);
				}

				// evars
				for (int i = 0; i < 75; i++)
				{
					// is this evar set in the hit?
					if (hit.evar[i].length())
					{
						// get the cookie version of this var (may be null)
						VCookie::RelVar const * cVar = cookie.GetVar(i);
						
						// set if different from the cookie value
						if (cVar && cVar->value != hit.evar[i])
							cookie.SetVar(i, hit.evar[i], hit.hit_time_gmt, 1, ALLOC_TYPE_FIRST);
					}
					else
					{
						cookie.ClearVar(i);
					}
				}
				
				writeTimer.Start();
				cookie.Store();
				writeTimer.Stop();
			}
			else
				break;	// ran out of hits
		}
	}
	monitor.Stop();
	
	pthread_mutex_lock(&consoleMutex);
	cout << parentPid << ": " << "Child " << threadParam->pid << " is done at " << time(NULL) << "\n";
	cout << parentPid << "-" << threadParam->pid << ": rate = " << monitor.EventsPerSecond() << "\n";
	cout << parentPid << "-" << threadParam->pid << ": readAvgNS = " << readTimer.NsPerSecond() << "\n";
	cout << parentPid << "-" << threadParam->pid << ": writeAvgNS = " << writeTimer.NsPerSecond() << "\n";
	cout << flush;
	
	// now add our results to the aggregate results for the parent
	// we'll do this inside the console mutex because we want the vector to be
	// atomically udated
	vector<unsigned long> *aggRate = threadParam->aggregateRate;
	
	for (unsigned int i = 0; i < monitor.EventsPerSecond().size(); i++)
	{
		// either add it to the existing value or append the value
		if (i < aggRate->size())
			(*aggRate)[i] += monitor.EventsPerSecond()[i];
		else
			aggRate->push_back(monitor.EventsPerSecond()[i]);
	}
	
	// update the aggregate read and write timers too
	vector<unsigned long> *timer = threadParam->aggregateReadTimer;
	for (unsigned int i = 0; i < readTimer.NsPerSecond().size(); i++)
	{
		// can't add any that we don't have events for
		if (i < monitor.EventsPerSecond().size())
		{
			// either add it to the existing value or append the value
			if (i < timer->size())
				(*timer)[i] += monitor.EventsPerSecond()[i] * readTimer.NsPerSecond()[i];
			else
				timer->push_back(monitor.EventsPerSecond()[i] * readTimer.NsPerSecond()[i]);
		}
	}
	
	timer = threadParam->aggregateWriteTimer;
	for (unsigned int i = 0; i < writeTimer.NsPerSecond().size(); i++)
	{
		// can't add any that we don't have events for
		if (i < monitor.EventsPerSecond().size())
		{
			// either add it to the existing value or append the value
			if (i < timer->size())
				(*timer)[i] += monitor.EventsPerSecond()[i] * writeTimer.NsPerSecond()[i];
			else
				timer->push_back(monitor.EventsPerSecond()[i] * writeTimer.NsPerSecond()[i]);
		}
	}
	pthread_mutex_unlock(&consoleMutex);

	pthread_exit((void *) 0);
}


/*
 * main
 */
 
int main(int argc, char *argv[])
{
	char	hostname[256];
	gethostname(hostname, sizeof(hostname));
	
	cout << "Adobe Visitor Profile Storage Test Harness " << VERSION_STRING 
		<< " using " << TOSTRING(STORAGE_ENGINE)
		<< " on " << hostname
		<< "\n\n";
	
	Configure(argc, argv);

	cout << "Config:"
		<< " threads = " << options.threads
		<< "; requests = " << options.requests;
	if (options.replayRate > 0)
		cout << "; replay multiplier = " << options.replayRate;
	else
		cout << "; request rate = " << options.requestRate;
	cout << "\n\n";

	vector<pthread_t> childThread(options.threads);
	vector<threadParam_t> threadParam(options.threads);

	// pthread_mutex_init(&fileReadMutex, NULL);	// initialized at definition
	// pthread_mutex_init(&consoleMutex, NULL);	// initialized at definition

	parentPid = getpid();	// get parent's pid

	hitSource	*hits = NULL;

	// place to accumulate the sum of the events per second across all threads
	vector<unsigned long> aggregateRate;
	// sum of total read and write times across all threads
	vector<unsigned long> aggregateReadTimer;
	vector<unsigned long> aggregateWriteTimer;
	
	if (options.replayFiles.size() > 0)
	{
		hits = new dwfileHitSource(options.replayFiles, fileReadMutex);
	}
	
	for (unsigned i = 0; i < options.threads; i++)
	{
		threadParam[i].pid = i+1;
		threadParam[i].hits = hits;
		threadParam[i].aggregateRate = &aggregateRate;
		threadParam[i].aggregateReadTimer = &aggregateReadTimer;
		threadParam[i].aggregateWriteTimer = &aggregateWriteTimer;
		
		pthread_mutex_lock(&consoleMutex);
		cout << parentPid << ": " << "Creating thread " << threadParam[i].pid << "\n";
		pthread_mutex_unlock(&consoleMutex);

		int err = pthread_create(&childThread[i], NULL, WorkerThread, (void *) &threadParam[i]);
		
		if (err)
		{
			cout << parentPid << ": " << "ERROR creating threads: " << err << "\n";
			exit(-1);
		}
	}
	
	// all children running at this point
	for (unsigned i = 0; i < options.threads; i++)
	{
		void *status;
		
		int err = pthread_join(childThread[i], &status);
		
		if (err)
		{
			cout << parentPid << ": " << "ERROR joining thread " << i << ": " << err << "\n";
			exit(-1);
		}
	}
	// all children finished at this point

	// display aggregate rate of events per second
	// no need for console mutex, single threaded at this point
	cout << "\n" << parentPid << ": aggregate rate = " << aggregateRate << "\n";
	// turn these back into an average rate
	for (unsigned int i = 0; i < aggregateReadTimer.size(); i++)
	{
		// assume that the timer is no longer than the rate
		// because of how they were updated by the child processes
		if (aggregateRate[i] > 0)
			aggregateReadTimer[i] /= aggregateRate[i];
		else
			aggregateReadTimer[i] = 0;
	}
	
	for (unsigned int i = 0; i < aggregateWriteTimer.size(); i++)
	{
		// assume that the timer is no longer than the rate
		// because of how they were updated by the child processes
		if (aggregateRate[i] > 0)
			aggregateWriteTimer[i] /= aggregateRate[i];
		else
			aggregateWriteTimer[i] = 0;
	}
	cout << parentPid << ": aggregate readAvgNS = " << aggregateReadTimer << "\n";
	cout << parentPid << ": aggregate writeAvgNS = " << aggregateWriteTimer << "\n";

	pthread_mutex_destroy(&fileReadMutex);
	pthread_mutex_destroy(&consoleMutex);
	// pthread_exit(NULL);	// already joined all threads, this should be unnecessary

	return 0;
}
