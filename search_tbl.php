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

$where = "WHERE 1 ";
$join = "LEFT JOIN (clients) ON (programs.client_id = clients.id) ";

if (array_key_exists("from", $_GET))
{
	$from = mysqli_real_escape_string($link, $_GET['from']);
	$where .= "AND programs.start_date >= '" . $from . "' ";
}

if (array_key_exists("to", $_GET))
{
	$to = mysqli_real_escape_string($link, $_GET['to']);
	$where .= "AND programs.end_date <= '" . $to . "' ";
}

if (array_key_exists("addr", $_GET))
{
	$addr = mysqli_real_escape_string($link, $_GET['addr']);
	$where .= "AND clients.address LIKE '%" . $addr . "%' ";
}

if (array_key_exists("pr", $_GET))
{
	$pr = mysqli_real_escape_string($link, $_GET['pr']);
	$where .= "AND programs.name LIKE '%" . $pr . "%' ";
}

if (array_key_exists("text", $_GET))
{
	$text = mysqli_real_escape_string($link, $_GET['text']);
	$where .= "AND programs.cc LIKE '%" . $text . "%' ";
}

$page = 1;
if (array_key_exists("page", $_GET))
	$page = intval($_GET['page']);
if ($page <= 0)
	$page = 1;

$q_cnt =
	"SELECT COUNT(*) FROM programs " .
	$join .  $where . " ;";

$result = mysqli_query($link, $q_cnt);
if (!$result)
	die("query failed: " . $q_cnt);
$res_cnt = mysqli_fetch_row($result)[0];
$page_cnt = ceil($res_cnt / RESULTS_PER_PAGE);

if ($page > $page_cnt)
	$page = $page_cnt;

$prev_page = $page - 1;
$next_page = $page + 1;

$_GET["page"] = $prev_page;
$prev_page_url = $_SERVER["PHP_SELF"] . "?" . http_build_query($_GET);
$_GET["page"] = $next_page;
$next_page_url = $_SERVER["PHP_SELF"] . "?" . http_build_query($_GET);

$start = ($page - 1) * RESULTS_PER_PAGE;

$q =
	"SELECT programs.id, UNIX_TIMESTAMP(programs.start_date), programs.start_date, programs.end_date, programs.name, clients.address " .
	"FROM programs " .
	$join . $where .
	"LIMIT " . $start . ", " . RESULTS_PER_PAGE . " ;";

?>
<div id="ctrl_box">
	<div id="stat">
		<?=$res_cnt?> programs found. Page <?=$page?> of <?=$page_cnt?>
	</div>
	<div id="arrows">
		<div>
			<? if ($prev_page <= 0) { ?>
			&lt; Prev
			<? } else { ?>
			<a href="<?=$prev_page_url?>">&lt; Prev</a>
			<? } ?>
		</div>
		<div>
			<? if ($next_page > $page_cnt) { ?>
			Next &gt;
			<? } else { ?>
			<a href="<?=$next_page_url?>">Next &gt;</a>
			<? } ?>
		</div>
	</div>
</div>

<?php
if ($result = mysqli_query($link, $q)) {
?>
<table>
	<tr>
		<th>Start</th>
		<th>End</th>
		<th>Name</th>
		<th>Sender</th>
		<th>Links</th>
	</tr>
<?php
	while ($row = mysqli_fetch_row($result)) {
		$id = $row[0];
		$etime = $row[1];
		$start = $row[2];
		$end = $row[3];
		$pr_name = $row[4];
		$addr = $row[5];

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

		echo "\t\t<td>";
		echo $addr;
		echo "</td>\n";

		echo "\t\t<td>\n";
		pr_link($etime, $id, "txt", "txt");
		pr_link($etime, $id, "xds.txt", "xds");
		pr_link($etime, $id, "bin", "bin");
		echo "\t\t</td>\n";

		echo "\n\t</tr>\n";
	}
	echo "</table>";
	mysqli_free_result($result);
} else {
	echo "Not found\n";
}

mysqli_close($link);
?>
