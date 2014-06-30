<?php
if (!array_key_exists("id", $_GET))
	return;

$id = preg_replace('/[^a-z0-9:.\-]/', '', $_GET["id"]);
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
					$("<div id=\"cc_line\">" + d.cc + "</div>").appendTo("#cc");
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
