<?php
define('ARCH_DIR', "./cc"); //XXX: parse config file?
define('ARCH_INFO_FILENAME', "info.txt");
define('INT_LEN', 10);

if (!array_key_exists("id", $_GET))
	return;

$id = preg_replace('/[^0-9]/', '', $_GET["id"]);

if (!array_key_exists("st", $_GET))
	return;

if ($_GET["st"] == "0")
	$st_flag = 0;
else 
	$st_flag = 1;

//FIXME: wrong day
$info_file = ARCH_DIR . "/" . date("Y/m-M/d/") . ARCH_INFO_FILENAME;

if (!file_exists($info_file))
	return;

if (($fp = fopen($info_file, "r")) == 0)
	return;

echo "[\n";
while (1) {
	fscanf($fp, "%d %d %s %s %s\n", $t, $cur_id, $cc_filepath, $addres, $prgr_name);

	fgetc($fp); 
	if (feof($fp)) 
		break;
	fseek($fp, -1, SEEK_CUR);

	// if ($cur_id != $id)
	// 	continue;

	if (0 == $st_flag) {
		echo "{";
		echo "\"name\": \"" . $prgr_name . "\",";
		echo "\"time\": \"" . date("H:i", $t) . "\",";
		echo "\"link\": \"" . $cc_filepath . "\"";
		echo "},\n";
	}
}

echo "{";
echo "\"name\": \"" . $prgr_name . "\",";
echo "\"time\": \"" . date("H:i", $t) . "\",";
echo "\"link\": \"" . $cc_filepath . "\"";
echo "}\n";
echo "]";

?>
