while (<>)
{
	# print "$_";
	
	if (/^\d+: aggregate (rate|readAvgNS|writeAvgNS) = (.*)$/)
	{
		$type = $1;
		$rates = $2;
		
		@rates = split(/\s+/, $rates);
		$total = 0;
		for $rate (@rates)
		{
			$total += $rate;
		}
		$cnt = scalar(@rates);
		$avg = $total / $cnt;
		print "type = $type, avg = $avg\n";
	}
}