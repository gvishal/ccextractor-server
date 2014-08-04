<?php
require("config.php");

function pr_link($time, $id, $ext, $name)
{
	global $cfg;
	$path = $cfg["archive_files_dir"] . strftime("/%Y/%m/%d/", $time) . $id . "." . $ext;

	if (!file_exists($path))
		return;

	echo "\t\t\t<a href=\"" . $path . "\">" . $name . "</a> \n";
}

$link = mysqli_connect($cfg["mysql_host"], $cfg["mysql_user"], $cfg["mysql_password"], $cfg["mysql_db_name"]);
if (mysqli_connect_errno()) {
	printf("Unable to connect to db: %s\n", mysqli_connect_error());
	exit();
}

$q_actv_cli = "SELECT active_clients.id, clients.address, clients.port, programs.start_date, programs.name " .
	 "FROM active_clients " .
	 "LEFT JOIN (clients, programs) ON (active_clients.id = clients.id AND active_clients.program_id = programs.id);";

$q_all_pr = "SELECT id, UNIX_TIMESTAMP(start_date), start_date, end_date, name FROM programs ;";
?>

<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
<html>
<head>
	<meta http-equiv="content-type" content="text/html; charset=utf-8; no-store">

	<style>
		table,td,th {
			padding:2px 15px 2px 15px;
		}
	</style>
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

		echo "\t<tr><th>Start</th><th>Address</th><th>Program Name</th></tr>\n";

		while ($row = mysqli_fetch_row($result)) {
			$id = $row[0];
			$addr = $row[1];
			$port = $row[2];
			$start = $row[3];
			$pr_name = $row[4];

			echo "\t<tr>\n";

			echo "\t\t<td>" . $start . "</td>\n";

			echo "\t\t<td><a href=\"view_cc.php?id=" . $id .'">' . $addr . ':' . $port . "</a></td>\n";

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

	echo "<h4>All programs: </h4>\n";

	if (mysqli_num_rows($result) == 0)
	{
		echo "None\n";
	} else {
		echo "<table>\n";

		echo "\t<tr><th>Start</th><th>End</th><th>Name</th><th>Links</th></tr>\n";

		while ($row = mysqli_fetch_row($result)) {
			$id = $row[0];
			$etime = $row[1];
			$start = $row[2];
			$end = $row[3];
			$pr_name = $row[4];

			echo "\t<tr>\n";

			echo "\t\t<td>" . $start . "</td>\n";

			echo "\t\t<td>";
			if ($end != "")
				echo $end;
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

</body>
</html>

<?php
mysqli_close($link);
?>
