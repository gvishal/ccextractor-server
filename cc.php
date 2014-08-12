<?php
require("config.php");

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

function print_link($pr_id, $time, $ext, $name, $comma)
{
	global $cfg;
	date_default_timezone_set("UTC");
	$path =
		$cfg["archive_files_dir"] .
		strftime("/%Y/%m/%d/", $time) .
	   	$pr_id . "." . $ext;

	if (!file_exists($path))
		return $comma;

	if ($comma)
		echo ", ";

	echo "{";
	echo "\"name\": \"" . nice_str($name) . "\", ";
	echo "\"path\": \"" . $path . "\"";
	echo "}";

	return true;
}

function print_links($command, $pr_id, $time, $name, $comma, $line)
{
	if ($name == "")
		$name = "Unknown";
	else
		$name = nice_str($name);

	if ($comma)
		echo ", ";

	echo "{";
	echo "\"line\": \"" . $line . "\", ";

	echo "\"command\": \"" . $command . "\", ";
	echo "\"name\": \"" . $name . "\", ";
	echo "\"links\": [";
	$c = print_link($pr_id, $time, "txt", "txt", false);
	$c = print_link($pr_id, $time, "srt", "srt", $c);
	$c = print_link($pr_id, $time, "xds.txt", "xds", $c);
	$c = print_link($pr_id, $time, "bin", "bin", $c);
	echo "]";
	echo "}";

	return true;
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
	exit();

$client_id = intval($_GET["id"]);

if (!array_key_exists("st", $_GET))
	exit();

if (($start = intval($_GET["st"])) < 0)
	$start = 0;

$buf_path = $cfg["buffer_files_dir"] . "/" . $client_id . ".txt";
if (!file_exists($buf_path)) {
	echo "[";
	echo "{";
	echo "\"line\": \"" . $start . "\", ";
	echo "\"command\": \"" . CONN_CLOSED. "\"";
	echo "}";

	$link = mysqli_connect($cfg["mysql_host"], $cfg["mysql_user"], $cfg["mysql_password"], $cfg["mysql_db_name"]);
	if (mysqli_connect_errno()) {
		echo "]";
		exit();
	}

	$q =
		// "SELECT id, UNIX_TIMESTAMP(CONVERT_TZ(start_date, @@session.time_zone, '+00:00')), name " .
		"SELECT id, UNIX_TIMESTAMP(start_date), name " .
		"FROM programs " .
		"WHERE client_id = " . $client_id . " ;";

	if ($result = mysqli_query($link, $q)) {
		while ($row = mysqli_fetch_row($result)) {
			$pr_id = $row[0];
			$time = $row[1];
			$name = $row[2];

			print_links(CONN_CLOSED_LINKS, $pr_id, $time, $name, true, ++$start);
		}
		mysqli_free_result($result);
	}

	echo "]";

	mysqli_close($link);
	exit();
}

if (($fp = fopen($buf_path, "r")) == 0)
	return;

echo "[\n";

$comma = false;

if (0 == $start)
{
	$link = mysqli_connect($cfg["mysql_host"], $cfg["mysql_user"], $cfg["mysql_password"], $cfg["mysql_db_name"]);
	if (mysqli_connect_errno()) {
		echo "]";
		exit();
	}

	$q =
		// "SELECT id, UNIX_TIMESTAMP(CONVERT_TZ(start_date, @@session.time_zone, '+00:00')), name " .
		"SELECT id, UNIX_TIMESTAMP(start_date), name " .
		"FROM programs " .
		"WHERE client_id = " . $client_id . " " .
		"ORDER BY id DESC " .
		"LIMIT 1 ;";

	if ($result = mysqli_query($link, $q)) {
		$row = mysqli_fetch_row($result);
		if (!$row) {
			mysqli_close($link);
			exit();
		}

		$pr_id = $row[0];
		$time = $row[1];
		$name = $row[2];

		$comma = print_links(LINKS_QUIET, $pr_id, $time, $name, $comma, $start);

		mysqli_free_result($result);
	}

	mysqli_close($link);
}

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

$pr_id = 0;
$pr_start = 0;

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
		$pr_id = intval($cc);
		continue;
	} else if ($command == PROGRAM_DIR) {
		$pr_start = intval($cc);
		continue;
	} else if ($command == PROGRAM_CHANGED || $command == PROGRAM_NEW) {
		$comma = print_links($command, $pr_id, $pr_start, $cc, $comma, $line);
		continue;
	}

	if ($comma)
		echo ",";

	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"command\": \"" . $command . "\",";
	echo "\"data\": \"" . nice_str($cc) . "\"";
	echo "}";

	$comma = true;
}

echo "]";

?>
