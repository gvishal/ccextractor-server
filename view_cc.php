<?php
if (!array_key_exists("id", $_GET))
	return;

$id = preg_replace('/[^0-9]/', '', $_GET["id"]);
?>

<html>
	<head>
		<script type="text/javascript" src="jquery-2.1.1.min.js"></script>
		<script type="text/javascript">
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
					if (d.command == 11) {
						$("<div id=\"cc_line\">" + d.data + "</div>").prependTo("#cc");
						line = parseInt(d.line) + 1;
					} else if (d.command == 12) {
						$(
							"<div id=\"new_program\">Program name: " + 
							"<span id=\"prg_name\">" + d.data + "</span>" +
							"</div>"
						).prependTo("#cc");
					} else if (d.command == 17) {
						$(
							"<div id=\"new_program\">Program changed: " + 
							"<span id=\"prg_name\">" + d.data + "</span>" +
							"<br>" +
							"<div id=\"cc_link\">Download link(s):<br></div>" + 
							"</div>"
						).prependTo("#cc");

					} else if (d.command == 115) {
						$("<a href=\"" + d.filepath + "\">" + d.name + "</a><br>").appendTo("#cc_link");
					} else if (d.command == 13) {
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
