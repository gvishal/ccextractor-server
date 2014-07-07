<?php
$conn_file = "connections.txt";

if (!file_exists($conn_file))
	return;

if (($fp = fopen($conn_file, "r")) == 0)
	return;

while ($line = fgets($fp)) {
	sscanf($line, "%d %s %d %s\n", $id, $addr, $time, $prg_name);
	echo date("d/m/y H:i ");
	echo $prg_name;
	echo '<a href="view_cc.php?id='.$id.'">'.$addr.'</a><br>';
}
?>

Start the server: <b><i>./server</i></b> <br>
Connect to it: <b><i>./client 127.0.0.1 2048 the.tudors.208-ALANiS.txt</i></b> <br>
Links will be shown here.
