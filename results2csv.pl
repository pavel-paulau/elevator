# convert test result files into CSV file

use Statistics::Lite qw(:stats);
use Getopt::Std;

getopts('cj') || die("Plese select a valid option\n");
$opt_j = 1 if (!$opt_c && !$opt_j);	# default to javascript

while(<>)
{
	chomp;
	if (/^Adobe Visitor Profile Storage Test Harness 1.2 using (\S+) on (\S+)/i)
	{
		$store = $1;
		$server = $2;
		
		$store =~ s/^VC//;
		$store =~ s/^Store//;
		$store =~ s/Store$//;
		
		$storeNames{$store} = 1;
	}
	
	if (/^Config: threads = (\d+); requests = (\d+); request rate = (\d+)/i)
	{
		$threads = $1;
		$requests = $2;
		$requestRate = $3;
	}
	
	if (/^\d+: aggregate rate = (\d+(?:\s+\d+)*)\s*$/i)
	{
		@aggRate = split(/\s+/, $1);
	}
	if (/^\d+: aggregate readAvgNS = (\d+(?:\s+\d+)*)\s*$/i)
	{
		@aggRead = split(/\s+/, $1);
	}
	if (/^\d+: aggregate writeAvgNS = (\d+(?:\s+\d+)*)\s*$/i)
	{
		@aggWrite = split(/\s+/, $1);
	}
	
	#warn "$_: $store, $server, $threads, $requests, $requestRate, @aggRate, @aggRead, @aggWrite\n" unless (/^\s*$/);
	
	# got all the data?
	if (scalar(@aggWrite))
	{
		# calculate stats from data
		%aggRate = statshash(@aggRate);
		%aggRead = statshash(@aggRead);
		%aggWrite = statshash(@aggWrite);
		
		$opt_c && print qq!"$store", "$server", $threads, $requests, $requestRate, $aggRate{'mean'},$aggRate{'stddevp'}, $aggRead{'mean'},$aggRead{'stddevp'}, $aggWrite{'mean'},$aggWrite{'stddevp'}\n!;

		# convert to integer values
		%aggRate = map { $_ => int($aggRate{$_}) } keys %aggRate;
		%aggRead = map { $_ => int($aggRead{$_}) } keys %aggRead;
		%aggWrite = map { $_ => int($aggWrite{$_}) } keys %aggWrite;
		
		$chartData[$threads]{$store}{'rate'} = "$aggRate{'mean'},${\(max(0,$aggRate{'mean'}-$aggRate{'stddevp'}))},${\($aggRate{'mean'}+$aggRate{'stddevp'})}";
		$chartData[$threads]{$store}{'read'} = "$aggRead{'mean'},${\(max(0,$aggRead{'mean'}-$aggRead{'stddevp'}))},${\($aggRead{'mean'}+$aggRead{'stddevp'})}";
		$chartData[$threads]{$store}{'write'} = "$aggWrite{'mean'},${\(max(0,$aggWrite{'mean'}-$aggWrite{'stddevp'}))},${\($aggWrite{'mean'}+$aggWrite{'stddevp'})}";
		
		# reset values
		undef $store;
		undef $server;
		undef $threads;
		undef $requests;
		undef $requestRate;
		undef @aggRate;
		undef @aggRead;
		undef @aggWrite;
	}
}


# JS output
if ($opt_j)
{
	#############
	# Rate chart
	#############
	print <<EOS;


	function drawChartRate() {
		var data = new google.visualization.DataTable();
		data.addColumn('number', 'Threads');

EOS

	for $store (sort keys %storeNames)
	{
		print <<EOS;
		data.addColumn('number', '$store Rate');
		data.addColumn({type: 'number', role: 'interval'}, 'min');
		data.addColumn({type: 'number', role: 'interval'}, 'max');

EOS
	}

	print "\t\tdata.addRows([\n";

	for ($i = 1; $i <= $#chartData; $i++)
	{
		print "\t\t\t[$i";
		for $store (sort keys %storeNames)
		{
			print ", " . (exists($chartData[$i]{$store}{'rate'}) ? $chartData[$i]{$store}{'rate'} : ',,');
		}
		print "],\n";
	}

	print <<EOS;
		]);
		
		var chart = new google.visualization.LineChart(document.getElementById('chartRate'));
		chart.draw(data, {width: 1200, height: 800, focusTarget:"category", title:"Request Rate", legend:'right', vAxes:{0:{title:'Rate'}}, hAxis:{title:'Threads PER CLIENT'}});
	}
EOS


	#############
	# read/write NS chart
	#############
	print <<EOS;


	function drawChartReadWrite() {
		var data = new google.visualization.DataTable();
		data.addColumn('number', 'Threads');

EOS

	for $store (sort keys %{$chartData[1]})
	{
		print <<EOS;
		data.addColumn('number', '$store ReadNS');
		data.addColumn({type: 'number', role: 'interval'}, 'min');
		data.addColumn({type: 'number', role: 'interval'}, 'max');
		data.addColumn('number', '$store WriteNS');
		data.addColumn({type: 'number', role: 'interval'}, 'min');
		data.addColumn({type: 'number', role: 'interval'}, 'max');

EOS
	}

	print "\t\tdata.addRows([\n";

	for ($i = 1; $i <= $#chartData; $i++)
	{
		print "\t\t\t[$i";
		for $store (sort keys %{$chartData[$i]})
		{
			print ", " . (exists($chartData[$i]{$store}{'read'}) ? $chartData[$i]{$store}{'read'} : ',,');
			print ", " . (exists($chartData[$i]{$store}{'write'}) ? $chartData[$i]{$store}{'write'} : ',,');
		}
		print "],\n";
	}

	print <<EOS;
		]);
		
		var chart = new google.visualization.LineChart(document.getElementById('chartReadWrite'));
		chart.draw(data, {width: 1200, height: 800, focusTarget:"category", title:"Read Write time (NS)", legend:'right', vAxes:{0:{title:'Read/Write NS'}}, hAxis:{title:'Threads PER CLIENT'}});
	}
EOS


}

sub max
{
	return $_[1] > $_[0] ? $_[1] : $_[0];
}