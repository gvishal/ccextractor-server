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

function pr_link($dir, $id, $ext, $name, $comma)
{
	$path = $dir . "/" . $id . "." . $ext;
	if (!file_exists($path))
		return $comma;

	if ($comma)
		echo ",\n";

	echo "{";
	echo "\"command\": \"" . DOWNLOAD_LINKS. "\",";
	echo "\"filepath\": \"" . $path . "\",";
	echo "\"name\": \"" . $name . "\"";
	echo "}";

	return true;
}

function pr_link_np($time, $id, $ext, $name, $comma)
{
	global $cfg;
	$path = $cfg["archive_files_dir"] . strftime("/%Y/%m/%d/", $time) . $id . "." . $ext;

	if (!file_exists($path))
		return $comma;

	if ($comma)
		echo ", ";

	echo "{";
	echo "\"name\": \"" . $name . "\", ";
	echo "\"path\": \"" . $path . "\"";
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

$filepath = $cfg["buffer_files_dir"] . "/" . $client_id . ".txt";
if (!file_exists($filepath)) {
	global $cfg;
	global $client_id;

	echo "[";
	echo "{\"command\": \"" . CONN_CLOSED. "\"}";

	$link = mysqli_connect($cfg["mysql_host"], $cfg["mysql_user"], $cfg["mysql_password"], $cfg["mysql_db_name"]);
	if (mysqli_connect_errno()) {
		echo "]";
		exit();
	}

	$q =
		"SELECT id, UNIX_TIMESTAMP(start_date), name " .
		"FROM programs " .
		"WHERE client_id = " . $client_id . " ;";

	if ($result = mysqli_query($link, $q)) {
		while ($row = mysqli_fetch_row($result)) {
			$pr_id = $row[0];
			$time = $row[1];
			$name = $row[2];

			echo ",{";
			echo "\"command\": \"" . CONN_CLOSED_LINKS . "\", ";
			echo "\"name\": \"" . $name . "\", ";
			echo "\"links\": [";
			$c = pr_link_np($time, $pr_id, "txt", "txt", false);
			$c = pr_link_np($time, $pr_id, "xds.txt", "xds", $c);
			$c = pr_link_np($time, $pr_id, "bin", "bin", $c);
			echo "]";
			echo "}";
		}
	}

	echo "]";
	exit();
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

echo "[\n";
$pr_id = 0;
$comma = false;
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
	}

	if ($command == PROGRAM_DIR) {
		if (0 == $pr_id)
			continue;

		$comma = pr_link($cc, $pr_id, "txt", "txt", $comma);
		$comma = pr_link($cc, $pr_id, "xds.txt", "xds", $comma);
		$comma = pr_link($cc, $pr_id, "bin", "bin", $comma);

		continue;
	}

	if ($comma)
		echo ",\n";

	echo "{";
	echo "\"line\": \"" . $line . "\",";
	echo "\"command\": \"" . $command . "\",";
	echo "\"data\": \"" . nice_str($cc) . "\"";
	echo "}";
	$comma = true;
}

echo "]";

?>
