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
define('CC_NAME',          206);

define('OFFSET',           800);
define('RESULTS_PER_PAGE', 10);
?>
