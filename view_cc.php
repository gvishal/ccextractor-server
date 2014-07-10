<?php
if (!array_key_exists("id", $_GET))
	return;

$id = preg_replace('/[^0-9]/', '', $_GET["id"]);
?>

<html>
	<head>
		<script type="text/javascript" src="jquery-2.1.1.min.js"></script>
		<script type="text/javascript">
$(function() {
	var line = 0;
	window.setInterval(function() {
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
					if (d.comd == 11) {
						$("<div id=\"cc_line\">" + d.data + "</div>").prependTo("#cc");
					} 
					if (d.comd == 12) {
						$("<div id=\"new_program\">Program changed: " + 
							"<span id=\"prg_name\">" + d.data + "</span>" +
							"</div>").prependTo("#cc");

						$.ajax({ //goddamit
							type: "GET",
							url: "download_links.php",
							dataType: "json",
							data: {
								id: "<?= $id ?>",
								st: "1"
							},
							success: function(data) {
								$.each(data, function(i, d) {
									$("<div id=\"d_links\">Download links:</div>").prependTo("#cc");
									$("<a href=\"" + d.link + "\">" + d.name + "</a>").appendTo("#d_links");
								});
							}
						});
					}
					line = parseInt(d.line) + 1;
				});
			}
		});
	}, 1000);
});
		</script>
	</head>
	<body>
		<div id="cc">

		</div>
	</body>
</html>
