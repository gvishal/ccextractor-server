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

$q_actv_cli = 
	"SELECT cli_id, pr_id, pr_name, pr_start_date, cli_description " .
	"FROM active_clients; ";
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

		echo "\t<tr><th>Channel</th><th>Program</th><th>Started</th></tr>\n";

		while ($row = mysqli_fetch_row($result)) {
			$cli_id = $row[0];
			$pr_id = $row[1];
			$pr_name = $row[2];
			$pr_start_date = $row[3];
			$cli_description = $row[4];

			echo "\t<tr>\n";

			echo "\t\t<td><a href=\"view_cc.php?id=" . $cli_id .'">';
            if ($cli_description != "")
                echo $cli_description;
            else
				echo "Unknown";
			echo "</a></td>\n";

			echo "\t\t<td>";
			if ($pr_name != "")
				echo $pr_name;
			else
				echo "Unknown";
			echo "</td>\n";

			echo "\t\t<td>" . $pr_start_date . "</td>\n";

			echo "\t</tr>\n";
		}

		echo "</table>";
	}

    mysqli_free_result($result);
}
?>

<br>
<br>
<a href="search.php">Search database</a>
<br>
<a href="grid.html">Grid</a>

</body>
</html>

<?php
mysqli_close($link);
?>
