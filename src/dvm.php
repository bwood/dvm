<?php

require __DIR__ . '/../vendor/autoload.php';
require __DIR__ . '/terminus.inc';
use Symfony\Component\Yaml\Yaml;


// Ensure git installed
exec('which git', $output, $return);
if ($return != 0) {
  print "Error: Can't find git. Please install it or fix your $PATH.\y";
}

$env = array(
  'DVM_TERMINUS',  // path to terminus
  'DVM_PROJ_DIR',  // path to projects
);

$needed_vars = array();
foreach ($env as $var) {
  if (getenv($var) === FALSE) {
    $needed_vars[] = $var;
  }
  else {
    ${strtolower($var)} = getenv($var);
  }
}

if (count($needed_vars)) {
  print "The following environment variables must be defined:\n\n";
  foreach ($needed_vars as $var) {
    print "export $var=\n";
  }
  exit(1);
}

// Make sure a dvm config dir exists
$config_dir = $_SERVER['HOME'] . '/.dvm';
if (!is_dir($config_dir) || !is_writable($config_dir)) {
  if (!mkdir($config_dir)) {
    print "Error: Unable to create $config_dir.\n";
    exit(1);
  }
}
// Ensure Vagrant installed

// Ensure Vbox installed


// Load existing drupalvm config
$file = __DIR__ . "/../plugins/drupalvm_config.yml";
$yaml = Yaml::parse(file_get_contents($file));

  /////////////////////
 // Gather config ////
/////////////////////

// Site Name
//fixme $options must be defined for take input
$options = array();
$site_name = strtolower(take_input("Enter the pantheon site name"));

if (!terminus_site_info($site_name)) {
  print "Error: Either $site_name doesn't exist, or you are not a team member on that site.\n";
  exit(1);
}

$site_dir = $dvm_proj_dir . '/' . $site_name;
if (!is_dir($site_dir)) {
  if (file_exists($site_dir)) {
    print "Error: $site_dir appears to be a plain file. Please move or remove it.\n";
    exit(1);
  }

  //TODO: clone the site
}
else {
  if (!is_writable($site_dir)) {
    print "Error: $site_dir isn't writable. Please fix that.\n";
    exit(1);
  }
  if (!file_exists($site_dir . '/.git')) {
    print "Error: $site_dir isn't a git repo. Please make it one, or move it out of the way.\n";
    exit(1);
  }

  print "Using git to pull the latest code into $site_dir.\n";
  $cmd = "cd $site_dir;git pull";
  exec($cmd, $output, $return);
  if ($return != 0) {
    print "Error: Could not run '$cmd'\n";
    print implode("\n", $output);
  }

}

$yaml['vagrant_synced_folders'][]['local_path'] = $site_dir;
$yaml['vagrant_synced_folders'][]['destination'] = '/var/www/' . $site_name;
$yaml['vagrant_synced_folders'][]['type'] = 'nfs';
$yaml['vagrant_synced_folders'][]['create'] = 1;

// Write config.yml for drupalvm

$dumper = new Symfony\Component\Yaml\Dumper();
$yaml = $dumper->dump($yaml, 2);
print $yaml . "\n";
