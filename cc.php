<?php
define('INT_LEN', 10);
define('OFFSET', 800);

function seek_next_block($fp)
{
	$c1 = "";
	$c2 = fgetc($fp);
	while ($c1 != "\r" && $c2 != "\n") {
		$c1 = $c2;
		$c2 = fgetc($fp);

		fgetc($fp); 
		if (feof($fp)) 
			return -1;
		fseek($fp, -1, SEEK_CUR);
	}
	return 0;
}

if (!array_key_exists("id", $_GET))
	return;

if (!array_key_exists("st", $_GET))
	return;

$filename = preg_replace('/[^a-z0-9:.\-]/', '', $_GET["id"]);
$filepath = "./tmp/" . $filename . ".txt";

if (!file_exists($filepath))
	return;

if (($fp = fopen($filepath, "r")) == 0)
	return;

$start = intval($_GET["st"]);

$of = 0;
$line = PHP_INT_MAX;
do { //offset untill we find a block with $line <= $start
	$of -= OFFSET;
	if (fseek($fp, $of, SEEK_END) < 0) {
		fseek($fp, 0);
	} else {
		if (seek_next_block($fp) < 0) {
			continue;
		}
	}

	$fp_pos = ftell($fp);
	$line = intval(stream_get_line($fp, INT_LEN, " "));
	fseek($fp, $fp_pos);
} while ($line > $start);

echo "[\n";
while (1) {
	$line = intval(stream_get_line($fp, INT_LEN, " "));
	$len = intval(stream_get_line($fp, INT_LEN, " "));
	$cc = fread($fp, $len);
	fread($fp, 2);

	// I have no ideas why there is one char before eof.
	fgetc($fp); 
	if (feof($fp)) 
		break;
	fseek($fp, -1, SEEK_CUR);

	if ($line < $start)
		continue;

	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"cc\": \"" . trim($cc) . "\"";
	echo "},\n";
}

if ($line >= $start) {
	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"cc\": \"" . trim($cc) . "\"";
	echo "}\n";
}
echo "]";

?>
