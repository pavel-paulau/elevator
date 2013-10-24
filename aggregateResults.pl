# aggregate the results from multiple clients on c3 into a single result list

use Statistics::Lite qw(:stats);
#use Getopt::Std;

#getopts('cj') || die("Plese select a valid option\n");
#$opt_j = 1 if (!$opt_c && !$opt_j);	# default to javascript

while(<>)
{
	chomp;
	
	if (/^\S{3} \S{3} \d{1,2} +(\d\d):(\d\d):(\d\d) P[DS]T \d{4}/)
	{
		# date stamp at start and end of the run
		$timestamp = ($1*60*60) + ($2*60) + $3;	# now in seconds
	}
	
	if (/^Adobe Visitor Profile Storage Test Harness 1.2 using (\S+) on (\S+)/i)
	{
		$store = $1;
		$server = $2;
		
		$store =~ s/^VC//;
		$store =~ s/^Store//;
		$store =~ s/Store$//;
	}
	
	if (/^Config: threads = (\d+); requests = (\d+); request rate = (\d+)/i)
	{
		$threads = $1;
		$requests = $2;
		$requestRate = $3;
		
		$test{$threads}{'general'}{'store'} = $store;
		$test{$threads}{'general'}{'requests'} = $requests;
		$test{$threads}{'general'}{'requestRate'} = $requestRate;
		
		$test{$threads}{$server}{'timestamp'} = $timestamp;	# start time
	}
	
	if (/^\d+: aggregate rate = (\d+(?:\s+\d+)*)\s*$/i)
	{
		@{$test{$threads}{$server}{'aggRate'}} = split(/\s+/, $1);
	}
	if (/^\d+: aggregate readAvgNS = (\d+(?:\s+\d+)*)\s*$/i)
	{
		@{$test{$threads}{$server}{'aggRead'}} = split(/\s+/, $1);
	}
	if (/^\d+: aggregate writeAvgNS = (\d+(?:\s+\d+)*)\s*$/i)
	{
		@{$test{$threads}{$server}{'aggWrite'}} = split(/\s+/, $1);
	}
	
	#warn "$_: $store, $server, $threads, $requests, $requestRate, @aggRate, @aggRead, @aggWrite\n" unless (/^\s*$/);
}

# process each thread set and output one record for all servers

for $threads (sort {$a <=> $b} keys %test)
{
	$store = $test{$threads}{'general'}{'store'};
	$requests = $test{$threads}{'general'}{'requests'};
	$requestRate = $test{$threads}{'general'}{'requestRate'};
	
	$servers = join(':', map {/(.+?)\./ ? $1 : ()} sort keys %{$test{$threads}});
	
	print "Adobe Visitor Profile Storage Test Harness 1.2 using $store on $servers\n\n";
	
	print "Config: threads = $threads; requests = $requests; request rate = $requestRate\n\n";
	
	# find latest start time
	$startTime = -1;
	for $server (keys %{$test{$threads}})
	{
		$startTime = max($startTime, $test{$threads}{$server}{'timestamp'});
	}
	
	# now we have the latest start time, process the list for each server
	#
	# employ a little trick here to not have to run through the list twice more
	# only put in as many entries as the list already has and trunc the list
	# at the min of the servers list length and the current list length
	#
	# this keeps us from having wild numbers off the end when maybe not all
	# the client nodes were running (some maybe started sooner or finished
	# faster than others)
	$numServers = 0;
	undef @aggRate;
	undef @aggRead;
	undef @aggWrite;
	for $server (keys %{$test{$threads}})
	{
		next if ($server eq 'general');
		
		$numServers++;
		
		# figure out the start offset for this server
		$offset = $startTime - $test{$threads}{$server}{'timestamp'};
		
		$len = scalar @{$test{$threads}{$server}{'aggRate'}} - $offset;
		
		# do we have less data than is currently held?
		# if so, reduce the data we are holding
		if ($len < scalar @aggRate)
		{
			$#aggRate = $len-1; # last index (0 based) rather than length
			$#aggRead = $len-1;
			$#aggWrite = $len-1;
		}
		
		# if the results array is less than our length, only copy
		# as much as we are currently holding
		if (scalar @aggRate < $len && scalar @aggRate)
		{
			$len = scalar @aggRate;
		}
		
		# aggregate this server's results with the others
		for ($i = 0; $i < $len; $i++)
		{
			$aggRate[$i] += ${$test{$threads}{$server}{'aggRate'}}[$i];
			$aggRead[$i] += ${$test{$threads}{$server}{'aggRead'}}[$i];
			$aggWrite[$i] += ${$test{$threads}{$server}{'aggWrite'}}[$i];
		}
	}

	# now we have to go back and average the read and write numbers
	# since it is a total for all servers
	for ($i = 0; $i < scalar @aggRead; $i++)
	{
		$aggRead[$i] = int ($aggRead[$i] / $numServers);
		$aggWrite[$i] = int ($aggWrite[$i] / $numServers);
	}
	
	# now output the aggregate data
	print "$numServers: aggregate rate = " . join(' ', @aggRate) . "\n";
	print "$numServers: aggregate readAvgNS = " . join(' ', @aggRead) . "\n";
	print "$numServers: aggregate writeAvgNS = " . join(' ', @aggWrite) . "\n";
	print "\n\n";
}

sub max
{
	return ((($_[1]) > ($_[0])) ? ($_[1]) : ($_[0]));
}