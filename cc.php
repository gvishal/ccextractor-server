<?php
define('INT_LEN', 10);
define('OFFSET', 800);
define('CC', 11);
define('NEW_PRGM', 12);
define('RESET_PRGM', 17);
define('NEW_PRG_ID', 14);
define('DISCONN', 13);
define('DWNL_LINKS', 115);

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

function print_links($id, $last_prgm_id, $pr_all = false, $pr_comma = true)
{
	date_default_timezone_set("UTC");

	$cfg = parse_ini_file("./server.ini");
	if (!array_key_exists("archive_files_dir", $cfg))
		$cfg["archive_files_dir"] = "./cc";

	$last_prgm_id--;
	$info_file = $cfg["archive_files_dir"] . "/" . date("Y/m-M/d/") . ARCH_INFO_FILENAME;

	if (!file_exists($info_file))
		return;

	if (($fp = fopen($info_file, "r")) == 0)
		return;

	while (1) {
		fgetc($fp); 
		if (feof($fp)) 
			break;
		fseek($fp, -1, SEEK_CUR);

		$line = fgets($fp);

		sscanf($line, "%d %d %s %s %d", $t, $cur_id, $cc_filepath, $addres, $prgm_id);

		$pgrm_name = "";
		if (($pos = strpos($line, "\"")) === false)
			return;

		for ($i = $pos; $line[$i] != "\n"; $i++) {
			if ($line[$i] == "\"")
				continue;
			$pgrm_name .= $line[$i];
		}
		if ($pgrm_name == "(null)")
			$pgrm_name = date("Y/m/d H:i", $t) . " " . $addres;

		if ($cur_id != $id)
			continue;

		if ((false == $pr_all && $prgm_id == $last_prgm_id) || true == $pr_all)
		{
			if ($pr_comma)
				echo ",\n";

			echo "{";
			echo "\"command\": \"" . DWNL_LINKS. "\",";
			echo "\"filepath\": \"" . $cc_filepath . "\",";
			echo "\"name\": \"" . $pgrm_name . "\"";
			echo "}";
			$pr_comma = true;

			if (false == $pr_all)
				break;
		}
	}

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
	echo "}";

	print_links($client_id, 0, true);
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

	if ($command == NEW_PRG_ID) {
		$last_prgm_id = intval($cc);
		continue;
	}

	if ($pr_comma)
		echo ",\n";
	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"command\": \"" . $command . "\",";
	echo "\"data\": \"" . trim($cc) . "\"";
	echo "}";
	$pr_comma = true;

	if ($command == RESET_PRGM) {
		print_links($client_id, $last_prgm_id);
	}
}

echo "]";

?>
