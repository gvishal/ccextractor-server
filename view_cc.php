<?php
require("config.php");

if (!array_key_exists("id", $_GET))
	return;

$id = intval($_GET["id"]);
if (0 == $id)
	return;

function links($id)
{
	date_default_timezone_set("UTC");

	$cfg = parse_ini_file("./server.ini");
	if (!array_key_exists("archive_files_dir", $cfg))
		$cfg["archive_files_dir"] = "./cc";

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

		sscanf($line, "%d %d %s %s %s %s", $t, $cur_id, $addres, $bin, $txt, $srt);

		if ($cur_id != $id)
			continue;

		echo "<a href=\"" . $bin . "\">bin</a> ";
		echo "<a href=\"" . $txt . "\">txt</a> ";
		echo "<a href=\"" . $srt . "\">srt</a> ";
	}

	fclose($fp);
}
?>

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
	<meta http-equiv="content-type" content="text/html; charset=utf-8; no-store">
	<script type="text/javascript" src="jquery-2.1.1.min.js"></script>
	<script type="text/javascript">
var CONN_CLOSED =     101;
var CAPTIONS =        104;
var XDS =             105;
var DOWNLOAD_LINKS =  201;
var PROGRAM_NEW =     103;
var PROGRAM_CHANGED = 106;

// define('CONN_CLOSED',     101);
// define('PROGRAM_ID',      102);
// define('PROGRAM_NEW',     103);
// define('PROGRAM_CHANGED', 106);
// define('PROGRAM_START',   108);
// define('CAPTIONS',        104);
// define('XDS',             105);
// define('DOWNLOAD_LINKS',  201);

var rc;
var cur_links = "";
$(function() {
	var line = 0;
	rc = setInterval(update, 1000);

	function update() {
		var done = false;
		$.ajax({
			type: "GET",
			url: "cc.php",
			dataType: "json",
			data: {
				id: "<?= $id ?>",
				st: line
			},
			success: function(data) {
				$.each(data, function(i, d) {
					if (d.command == CAPTIONS) {
						$("<div id=\"cc_line\">" + d.data + "</div>").prependTo("#cc");
					} else if (d.command == XDS && $('#xds').prop('checked')) {
						$("<div id=\"cc_line\">" + d.data + "</div>").prependTo("#cc");
					} else if (d.command == CONN_CLOSED) {
						$( "<div id=\"new_program\">Connection closed</div>").prependTo("#cc");

						clearInterval(rc);
					} else if (d.command == PROGRAM_CHANGED) {
						$(
							"<div id=\"new_program\">Program changed: " + 
							"<span id=\"prg_name\">" + d.data + "</span>" +
							"</div>" +
							"<div id=\"cc_link\">Download links: " + cur_links + "</div>"
						).prependTo("#cc");
						cur_links = "";
					} else if (d.command == PROGRAM_NEW) {
						$(
							"<div id=\"new_program\">Program name: " + 
							"<span id=\"prg_name\">" + d.data + "</span>" +
							"</div>"
						).prependTo("#cc");
						cur_links = "";
					} else if (d.command == DOWNLOAD_LINKS) {
						cur_links += "<a href=\"" + d.filepath + "\">" + d.name + "</a> ";
					}

					if (d.command == CAPTIONS || d.command == XDS)
						line = parseInt(d.line) + 1;
				});
			}
		});
	}
});
	</script>
</head>

<body>
	<div id="ctrl">
		<input type="checkbox" checked id="xds"> Print XDS
		<? links($id) ?>
	</div>
	<div id="cc">

	</div>
</body>
</html>
