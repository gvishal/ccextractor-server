<?php
require("config.php");

function pr_link($time, $id, $ext, $name)
{
	global $cfg;
	date_default_timezone_set("UTC");
	$path = $cfg["archive_files_dir"] . strftime("/%Y/%m/%d/", $time) . $id . "." . $ext;

	if (!file_exists($path))
		return;

	echo "\t\t\t<a href=\"" . $path . "\">" . $name . "</a> \n";
}

$link = db_connect();

// TODO: joins in index page - not good
$q_actv_cli = 
	"SELECT active_clients.id, clients.address, clients.port, clients.cc_desc, programs.start_date, programs.name " .
	"FROM active_clients " .
	"LEFT JOIN (clients, programs) ON (active_clients.id = clients.id AND active_clients.program_id = programs.id);";

$q_all_pr = 
	"SELECT programs.id, UNIX_TIMESTAMP(programs.start_date), programs.start_date, programs.end_date, clients.cc_desc, programs.name " .
	"FROM programs " .
	"LEFT JOIN (clients) ON (clients.id = programs.client_id) " .
	"ORDER BY id DESC " .
	"LIMIT " . RESULTS_PER_PAGE . " ;";
?>

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
	<meta http-equiv="content-type" content="text/html; charset=utf-8; no-store">
	<link rel="stylesheet" type="text/css" href="index.css">
</head>

<body>

<?php
if ($result = mysqli_query($link, $q_actv_cli)) {

	echo "<h4>Active connections:</h4>\n";

	if (mysqli_num_rows($result) == 0)
	{
		echo "None\n";
	} else {
		echo "<table>\n";

		echo "\t<tr><th>Start</th><th>Address</th><th>Description</th><th>Program Name</th></tr>\n";

		while ($row = mysqli_fetch_row($result)) {
			$id = $row[0];
			$addr = $row[1];
			$port = $row[2];
			$cc_desc = $row[3];
			$start = $row[4];
			$pr_name = $row[5];

			echo "\t<tr>\n";

			echo "\t\t<td>" . $start . "</td>\n";

			echo "\t\t<td><a href=\"view_cc.php?id=" . $id .'">' . $addr . ':' . $port . "</a></td>\n";

			echo "\t\t<td>";
			if ($cc_desc != "")
				echo $cc_desc;
			else
				echo "Unknown";
			echo "</td>\n";

			echo "\t\t<td>";
			if ($pr_name != "")
				echo $pr_name;
			else
				echo "Unknown";
			echo "</td>\n";

			echo "\t</tr>\n";
		}

		echo "</table>";
	}

    mysqli_free_result($result);
}
?>

<?
if ($result = mysqli_query($link, $q_all_pr)) {

	echo "<h4>Last " . RESULTS_PER_PAGE . " programs: </h4>\n";

	if (mysqli_num_rows($result) == 0)
	{
		echo "None\n";
	} else {
		echo "<table>\n";

		echo "\t<tr><th>Start</th><th>End</th><th>Description</th><th>Name</th><th>Links</th></tr>\n";

		while ($row = mysqli_fetch_row($result)) {
			$id = $row[0];
			$etime = $row[1];
			$start = $row[2];
			$end = $row[3];
			$cc_desc = $row[4];
			$pr_name = $row[5];

			echo "\t<tr>\n";

			echo "\t\t<td>" . $start . "</td>\n";

			echo "\t\t<td>";
			if ($end != "")
				echo $end;
			else
				echo "Unknown";
			echo "</td>\n";

			echo "\t\t<td>";
			if ($cc_desc != "")
				echo $cc_desc;
			else
				echo "Unknown";
			echo "</td>\n";

			echo "\t\t<td>";
			if ($pr_name != "")
				echo $pr_name;
			else
				echo "Unknown";
			echo "</td>\n";

			echo "\t\t<td>\n";
			pr_link($etime, $id, "txt", "txt");
			pr_link($etime, $id, "srt", "srt");
			pr_link($etime, $id, "xds.txt", "xds");
			pr_link($etime, $id, "bin", "bin");
			echo "\t\t</td>\n";

			echo "\n\t</tr>\n";
		}
		echo "</table>";
	}

    mysqli_free_result($result);
}
?>

<a href="search.php">Search database</a>
<br>
<a href="grid.html">Grid</a>

</body>
</html>

<?php
mysqli_close($link);
?>
