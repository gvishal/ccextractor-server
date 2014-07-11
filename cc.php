<?php
define('INT_LEN', 10);
define('OFFSET', 800);
define('CC', 11);
define('NEW_PRG', 12);
define('NEW_PRG_ID', 14);
define('DISCONN', 13);
define('DWNL_LINKS', 115);
define('ARCH_DIR', "./cc"); //XXX: parse config file?
define('ARCH_INFO_FILENAME', "info.txt");

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

function print_links($id, $last_prgm_id, $st)
{
	$last_prgm_id--;
	// FIXME: wrong day at 22:00 in local time
	$info_file = ARCH_DIR . "/" . date("Y/m-M/d/") . ARCH_INFO_FILENAME;

	if (!file_exists($info_file))
		return;

	if (($fp = fopen($info_file, "r")) == 0)
		return;

	while (1) {
		fscanf($fp, "%d %d %s %s %d %s\n", $t, $cur_id, $cc_filepath, $addres, $prgm_id, $pgrm_name);

		fgetc($fp); 
		if (feof($fp)) 
			break;
		fseek($fp, -1, SEEK_CUR);

		if ($cur_id != $id)
			continue;

		if (1 == $st && $prgm_id == $last_prgm_id)
			break;

		if (0 == $st) {
			echo "{";
			echo "\"command\": \"" . DWNL_LINKS. "\",";
			echo "\"filepath\": \"" . $cc_filepath . "\",";
			echo "\"name\": \"" . $pgrm_name . "\"";
			echo "},\n";
		}
	}

	echo "{";
	echo "\"command\": \"" . DWNL_LINKS. "\",";
	echo "\"filepath\": \"" . $cc_filepath . "\",";
	echo "\"name\": \"" . $pgrm_name . "\"";

	fclose($fp);
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
	echo "\"command\": \"" . DISCONN. "\"";
	echo "},\n";

	print_links($client_id, 0, 0);
	echo "}";
	echo "]";
	return;

}

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

$last_prgm_id = 0;

echo "[\n";
while (1) {
	$line = intval(stream_get_line($fp, INT_LEN, " "));
	$command = intval(stream_get_line($fp, INT_LEN, " "));
	$len = intval(stream_get_line($fp, INT_LEN, " "));
	$cc = fread($fp, $len);
	fread($fp, 2);

	fgetc($fp); 
	if (feof($fp)) 
		break;
	fseek($fp, -1, SEEK_CUR);

	if ($line < $start)
		continue;

	if ($command == NEW_PRG_ID) {
		$last_prgm_id = intval($cc);
		continue;
	}

	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"command\": \"" . $command . "\",";
	echo "\"data\": \"" . trim($cc) . "\"";
	echo "},\n";

	if ($command == NEW_PRG) {
		print_links($client_id, $last_prgm_id, 1);
		echo "},\n";
	}
}

if ($line >= $start) {
	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"command\": \"" . $command . "\",";
	echo "\"data\": \"" . trim($cc) . "\"";
}

if ($command == NEW_PRG) {
	print_links($client_id, $last_prgm_id, 1);
	echo "},\n";
}

echo "}\n";
echo "]";

?>
