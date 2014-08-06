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
	<script type="text/javascript" src="./view_cc.js"></script>
</script>
<link rel="stylesheet" type="text/css" href="view_cc.css">
</head>

<body>
<div id="ctrl">
	<div id="name_box">
		<div id="name_lbl">Current program: </div>
		<div id="pr_name">Unknown</div>
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
