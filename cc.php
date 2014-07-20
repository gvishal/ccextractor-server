<?php
define('INT_LEN', 10);

define('PROGRAM_ID',     102);
define('RESET_PROGRAM',  103);
define('CONN_CLOSED',    101);
define('DOWNLOAD_LINKS', 201);

define('OFFSET', 800);

define('ARCH_INFO_FILENAME', "info.txt");

function json_quote($str)
{
	$l = strlen($str);
	$ret = "";
	for ($i = 0; $i < $l; $i++) {
		if (strstr("\"\\", $str[$i]))
			$ret .= "\\";
		$ret .= $str[$i];
	}
	return $ret;
}

function nice_str($str)
{
	return json_quote(htmlspecialchars(trim($str)));
}

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

$client_id = intval($_GET["id"]);

if (!array_key_exists("st", $_GET))
	return;

$filepath = "./tmp/" . $client_id . ".txt";
if (!file_exists($filepath)) {
	echo "[\n";
	echo "{";
	echo "\"command\": \"" . CONN_CLOSED. "\"";
	echo "}";
	echo "]";
	return;

}

if (($fp = fopen($filepath, "r")) == 0)
	return;

$start = intval($_GET["st"]);

$fp_pos = ftell($fp);
$line = intval(stream_get_line($fp, INT_LEN, " "));
fseek($fp, $fp_pos);
if ($line < $start) {
	$line = PHP_INT_MAX;
	$of = 0;
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
}

$last_prgm_id = 0;

echo "[\n";
$pr_comma = false;
while (1) {
	fgetc($fp); 
	if (feof($fp)) 
		break;
	fseek($fp, -1, SEEK_CUR);

	$line = intval(stream_get_line($fp, INT_LEN, " "));
	$command = intval(stream_get_line($fp, INT_LEN, " "));
	$len = intval(stream_get_line($fp, INT_LEN, " "));
	$cc = fread($fp, $len);
	fread($fp, 2);


	if ($line < $start)
		continue;

	if ($command == PROGRAM_ID) {
		$last_prgm_id = intval($cc);
		continue;
	}

	if ($pr_comma)
		echo ",\n";
	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"command\": \"" . $command . "\",";
	echo "\"data\": \"" . nice_str($cc) . "\"";
	echo "}";
	$pr_comma = true;
}

echo "]";

?>
