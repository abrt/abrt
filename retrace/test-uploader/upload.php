#!/usr/bin/php
<?php
##########################################################################
# upload.php crash_directory                                             #
#                                                                        #
# Only for testing purposes =).                                          #
# Sends crash from crash_directory to the retrace server                 #
#     specified by $hostname and $hostport.                              #
# Root permissions are not really needed, but they simplify the process. #
# Requires packages.py script in the current directory.                  #
##########################################################################

$hostname = "ssl://denisa.expresmu.sk";
$hostport = 443;

function usage()
{
  global $argv;
  echo "Usage: " . $argv[0] . " crash_directory\n"
    . "    Crash directory must contain coredump and package files.\n"
    . "    You must run the script with root permissions.\n";
}

if (!isset($_SERVER['HOME']) || $_SERVER['HOME'] != "/root")
{
  usage();
  exit(1);
}

if ($argc != 2)
{
  usage();
  exit(2);
}

if (!is_dir($argv[1]))
{
  usage();
  exit(3);
}

if (!file_exists($argv[1] . "/coredump") || !file_exists($argv[1] . "/package") || !file_exists("./packages.py"))
{
  usage();
  exit(4);
}

$filename = "crash.tar.xz";

echo "Generating packages file... ";
system("./packages.py " . $argv[1] . " > " . $argv[1] . "/packages");
echo "Done\n";

if (chdir($argv[1]))
{
  echo "Compressing files into .tar.xz archive... ";
  system("tar -cJf crash.tar.xz coredump packages");
  echo "Done\n";
}
else
{
  echo "Unable to change directory to " . $argv[1] . "\n";
  exit(5);
}

echo "Connecting to " . $hostname . ":" . $hostport . "... ";

if($socket = @fsockopen($hostname, $hostport))
{
  echo "Done\n"
    . "Sending request:\n-----\n";

  $request = "POST /create HTTP/1.1\r\n"
    . "Host: denisa.expresmu.sk\r\n"
    . "Content-Type: application/x-xz-compressed-tar\r\n"
    . "Content-Length: " . filesize($filename) . "\r\n"
    . "Connection: close\r\n"
    . "\r\n";

  echo $request . "[raw data]\n-----\n";

  fputs($socket, $request);
  fputs($socket, file_get_contents($filename));

  echo "Receiving response:\n-----\n";

  $response = "";
  while(!feof($socket))
  {
    $response .= fread($socket, 256);
  }

  echo $response . "\n-----\n";

  fclose($socket);
  echo "Socket closed.\n";
}
else
{
  echo "Error connecting to socket.\n";
}
?>
