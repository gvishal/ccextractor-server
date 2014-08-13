<?php
$cfg = parse_ini_file("./server.ini");

if (!array_key_exists("mysql_host", $cfg))
	$cfg["mysql_host"] = "localhost";

if (!array_key_exists("mysql_user", $cfg))
	$cfg["mysql_user"] = "root";

if (!array_key_exists("mysql_password", $cfg))
	$cfg["mysql_password"] = "root";

if (!array_key_exists("mysql_db_name", $cfg))
	$cfg["mysql_db_name"] = "cce";

if (!array_key_exists("archive_files_dir", $cfg))
	$cfg["archive_files_dir"] = "./cc";

if (!array_key_exists("buffer_files_dir", $cfg))
	$cfg["buffer_files_dir"] = "./tmp";

define('INT_LEN',          10);

define('CONN_CLOSED',      101);
define('PROGRAM_ID',       102);
define('PROGRAM_NEW',      103);
define('PROGRAM_CHANGED',  106);
define('PROGRAM_DIR',      108);
define('CAPTIONS',         104);
define('XDS',              105);
define('DOWNLOAD_LINKS',   201);
define('CONN_CLOSED_LINKS',202);
define('LINKS_QUIET',      205);
define('CC_DESC',          206);

define('OFFSET',           800);
define('RESULTS_PER_PAGE', 10);

function db_connect()
{
	global $cfg;

	$link = mysqli_connect($cfg["mysql_host"], $cfg["mysql_user"], $cfg["mysql_password"], $cfg["mysql_db_name"]);
	if (mysqli_connect_errno()) {
		printf("Unable to connect to db: %s\n", mysqli_connect_error());
		exit();
	}

	mysqli_query($link, "SET time_zone='+00:00';");

	return $link;
}
?>
