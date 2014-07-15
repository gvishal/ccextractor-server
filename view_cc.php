<?php
if (!array_key_exists("id", $_GET))
	return;

$id = preg_replace('/[^0-9]/', '', $_GET["id"]);
?>

<html>
	<head>
		<script type="text/javascript" src="jquery-2.1.1.min.js"></script>
		<script type="text/javascript">
var CAPTIONS =       3;
var PROGRAM =        4;
var CONN_CLOSED =    101;
var PROGRAM_ID =     102;
var RESET_PROGRAM =  103;
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
					if (d.command == 4) {
						$("<div id=\"cc_line\">" + d.data + "</div>").prependTo("#cc");
						line = parseInt(d.line) + 1;
					} else if (d.command == CAPTIONS) {
						$(
							"<div id=\"new_program\">Program name: " + 
							"<span id=\"prg_name\">" + d.data + "</span>" +
							"</div>"
						).prependTo("#cc");
					} else if (d.command == RESET_PROGRAM) {
						$(
							"<div id=\"new_program\">Program changed: " + 
							"<span id=\"prg_name\">" + d.data + "</span>" +
							"<br>" +
							"<div id=\"cc_link\">Download link(s):<br></div>" + 
							"</div>"
						).prependTo("#cc");
					} else if (d.command == DOWNLOAD_LINKS) {
						$("<a href=\"" + d.filepath + "\">" + d.name + "</a><br>").appendTo("#cc_link");
					} else if (d.command == CONN_CLOSED) {
						$(
							"<div id=\"new_program\">Connection closed" + 
							"<br>" +
							"<div id=\"cc_link\">Download link(s):<br></div>" + 
							"</div>"
						).prependTo("#cc");

						clearInterval(rc);
					}
				});
			}
		});
	}
});
		</script>
	</head>
	<body>
		<div id="cc">

		</div>
	</body>
</html>
