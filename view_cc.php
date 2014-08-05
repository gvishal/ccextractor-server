<?php
require("config.php");

if (!array_key_exists("id", $_GET))
	return;

$id = intval($_GET["id"]);
if (0 == $id)
	return;
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

var rc;
var cur_links = "";
$(function() {
	var line = 0;
	update();
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
				} else if (d.command == XDS && $('#xds_chkbox').prop('checked')) {
					$("<div id=\"cc_line\">" + d.data + "</div>").prependTo("#cc");
				} else if (d.command == CONN_CLOSED) {
					$( "<div id=\"conn_closed_cc\">Connection closed</div>").prependTo("#cc");

					clearInterval(rc);
					$("#ctrl").remove();

				} else if (d.command == PROGRAM_NEW || d.command == PROGRAM_CHANGED) {
					$(
						"<div id=\"new_program\">Program name: " + 
						"<span id=\"pr_name_cc\">" + d.data + "</span>" +
						"</div>"
					).prependTo("#cc");

					if (d.command == PROGRAM_CHANGED)
						$( "<div id=\"cc_link\">Download links: " + cur_links + "</div>").prependTo("#cc");

					cur_links = "";
					$("#pr_name").html(d.data);
				} else if (d.command == DOWNLOAD_LINKS) {
					cur_links += "<a href=\"" + d.filepath + "\">" + d.name + "</a> ";
					$("#pr_links").html(cur_links);
				}

				if (d.command == CAPTIONS || d.command == XDS)
					line = parseInt(d.line) + 1;
			});
		}
		});
	}
});
</script>
<link rel="stylesheet" type="text/css" href="view_cc.css">
</head>

<body>
<div id="ctrl">
	<div id="name_box">
		<div id="name_lbl">Current program: </div>
		<div id="pr_name"></div>
	</div>
	<div id="links_box">
		<div id="links_lbl">Download links: </div>
		<div id="pr_links"></div>
	</div>
	<div id="xds_box">
		<input type="checkbox" checked id="xds_chkbox">
		<div id="xds_lbl"> Print XDS </div>
	</div>
</div>

<div id="cc">
</div>
</body>
</html>
