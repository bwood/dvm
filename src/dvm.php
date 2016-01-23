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
  'DVM_DVM_DIR'    // path to drupal-vm installation
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
//TODO: if file doesn't exist in ~/.dvm, create from template
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
$site_dir_vm = '/var/www/' . $site_dir;
if (!is_dir($site_dir)) {
  if (file_exists($site_dir)) {
    print "Error: $site_dir appears to be a plain file. Please move or remove it.\n";
    exit(1);
  }
  //TODO: clone the site
  print "clone site not implemented.\n";
  exit(1);
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

  print "Using git to pull the latest code into $site_dir...\n";
  $cmd = "cd $site_dir;git pull";
  //TODO: enable
  /***********
  * exec($cmd, $output, $return);
  * if ($return != 0) {
    * print "Error: Could not run '$cmd'\n";
    * print implode("\n", $output);
   * }
   *********/
}

//TODO: test that these won't be added multiple times
if (!array_search($site_dir, array_column($yaml['vagrant_synced_folders'], 'local_path'))) {
  $yaml['vagrant_synced_folders'][] = array(
    'local_path' => $site_dir,
    'destination' => $site_dir_vm,
    'type' => 'nfs',
    'create' => 1
  );
}

if (!array_search($site_dir_vm, array_column($yaml['apache_vhosts'], 'documentroot'))) {
  $yaml['apache_vhosts'][] = array(
    'servername' => "$site_name.localhost",
    'documentroot' => $site_dir_vm,
    'extra_parameters' => "ProxyPassMatch ^/(.*\.php(/.*)?)$ \"fcgi://127.0.0.1:9000$site_dir_vm\"",
  );
}

if (!array_search($site_name, array_column($yaml['mysql_databases'], 'name'))) {
  $yaml['mysql_databases'][] = array(
    'name' => $site_name,
    'encoding' => "utf8",
    'collation' => "utf8_general_ci"
  );
}
// Write config.yml for drupalvm

$dumper = new Symfony\Component\Yaml\Dumper();
$yaml_dumped = $dumper->dump($yaml, 2);
print $yaml_dumped . "\n";

//backup
//TODO: commit to git?
rename("$dvm_dvm_dir/config.yml", "$dvm_dvm_dir/config.yml-" . time());
file_put_contents("$dvm_dvm_dir/config.yml", $yaml_dumped);
