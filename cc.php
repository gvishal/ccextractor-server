<?php

define('INT_LEN', 10);

// $_GET["id"] = "localhost.localdomain:45115";
// $_GET["st"] = "18";

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

echo "[\n";

while (1) {
	$str = fread($fp, INT_LEN);
	$line = intval($str);
	$len = intval(fread($fp, INT_LEN));
	$cc = fread($fp, $len);

	// I have no ideas why is there one char before eof.
	// $cc = fread($fp, $len); read whole line to the end
	// including \r \n, so next char belongs to $line
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
