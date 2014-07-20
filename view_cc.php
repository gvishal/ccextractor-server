<?php
define('ARCH_INFO_FILENAME', "info.txt");
if (!array_key_exists("id", $_GET))
	return;

$id = preg_replace('/[^0-9]/', '', $_GET["id"]);

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
		echo "<a href=\"" . $srt . "\">srt</a>";
	}

	fclose($fp);
}
?>

<html>
	<head>
		<script type="text/javascript" src="jquery-2.1.1.min.js"></script>
		<script type="text/javascript">
var CONN_CLOSED =    101;
var CAPTIONS =       104;
var XDS =            105;
var DOWNLOAD_LINKS = 201;

var rc;
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
					} else if (d.command == DOWNLOAD_LINKS) {
						$("<a href=\"" + d.filepath + "\">" + d.name + "</a><br>").appendTo("#cc_link");
					} else if (d.command == CONN_CLOSED) {
						$( "<div id=\"new_program\">Connection closed</div>").prependTo("#cc");

						clearInterval(rc);
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
			<input type="checkbox" id="xds"> Print XDS
			<? links($id) ?>
		</div>
		<div id="cc">

		</div>
	</body>
</html>
