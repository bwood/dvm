<?php

require __DIR__ . '/../vendor/autoload.php';
require __DIR__ . '/terminus.inc';
use Symfony\Component\Yaml\Yaml;


$usage = <<<EOT

USAGE:

php $argv[0]

  --site=site-name               # REQUIRED: Name of Pantheon site.
                                 # E.g For "dev-example.pantheon.io" this would
                                 # be "example".

  --no-files

  --notify                       # Display MacOS notification when script
                                 # finishes.

  --no-sound                     # Don't allow notifications to play sounds.

  -h (--help)                    # Print help and exit

EOT;

// process args
$longopts = array(
  "site:",
  "no-files",
  "notify",
  "no-sound"
);
$shortopts = "";
$options = array();
build_options($options, $shortopts, $longopts, $usage);

// Check the user's options
validate_options();

// Requirements
// Ensure required commands available
$errors = FALSE;
//TODO: check for vbox
$required_commands = array('git', 'tar', 'gzip', 'vagrant');
foreach ($required_commands as $cmd) {
  exec("which $cmd", $output, $return);
  if ($return != 0) {
    print "Error: Can't find required command '$cmd'. Please install it or fix your $PATH.\n";
    $errors = TRUE;
  }
}


if ($errors) {
  exit(1);
}

$disable_notifications = FALSE;
if (in_array('notify', array_keys($options))) {
  exec("which osascript", $output, $return);
  if ($return != 0) {
    print "Warning: Can't find required command 'osascript'. Notafications disabled.\n";
    $disable_notifications = TRUE;
  }
}

$env = array(
  'DVM_TERMINUS',  // path to terminus
  'DVM_PROJ_DIR',  // path to projects
  'DVM_DVM_DIR'    // path to drupal-vm installation
);

$needed_vars = array();
foreach ($env as $var) {
  if (getenv($var) === FALSE) {
    //TODO: if var ends in _DIR strip trailing slash if exists
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

// Load existing drupalvm config
//TODO: if file doesn't exist in ~/.dvm, create from template
$file = __DIR__ . "/../plugins/drupalvm_config.yml";
$yaml = Yaml::parse(file_get_contents($file));

//TODO: Ensure the user is authed to terminus


/////////////////////
// Gather config ////
/////////////////////

// Site Name
if (in_array('site', array_keys($options))) {
  $site_name = $options['site'];
}
else {
  $site_name = strtolower(take_input("Enter the pantheon site name"));
}


if (!terminus_site_info($site_name)) {
  print "Error: Either $site_name doesn't exist, or you are not a team member on that site.\n";
//TODO: enable
  exit(1);
}

$site_dir = $dvm_proj_dir . '/' . $site_name;
$site_dir_vm = '/var/www/' . $site_name;
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
  exec($cmd, $output, $return);
  if ($return != 0) {
    print "Error: Could not run '$cmd'\n";
    print implode("\n", $output);
  }

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
//print $yaml_dumped . "\n";

//backup
//TODO: commit to git?
rename("$dvm_dvm_dir/config.yml", "$dvm_dvm_dir/config.yml-" . time());
file_put_contents("$dvm_dvm_dir/config.yml", $yaml_dumped);

// Restart the VM //
print "Restarting VM with new configuration.\n";
//TODO: enable
$cmd = "cd $dvm_dvm_dir;vagrant halt;vagrant up --provision";
exec($cmd, $output, $return);
if ($return != 0) {
  print "Error: Failed to reload VM.\n";
  print implode("\n", $output);
  exit(1);
}

// Configure settings.php
$db_conf = <<<EOT
<?php

\$databases['default']['default'] = array (
  'database' => '$site_name',
  'username' => 'drupal',
  'password' => 'drupal',
  'prefix' => '',
  'host' => 'localhost',
  'port' => '',
  'driver' => 'mysql',
);
EOT;

$settings_local = $site_dir . "/sites/default/settings_local.php";
if (!file_exists($settings_local)) {
  if (!touch($settings_local)) {
    print "Error: Couldn't create $settings_local\n";
    exit(1);
  }
}
if (file_put_contents($settings_local, $db_conf) === FALSE) {
  print "Error: Couldn't write to $settings_local\n";
  exit(1);
}

$settings_local_include = array(
  '// localhost development by dvm',
  'if (file_exists(__DIR__ . "/settings_local.php")) {',
  '  include_once("settings_local.php");',
  '} //dvm unique line', //without comment, this will match other lines.
);

$settings_existing = $site_dir . "/sites/default/settings.php";
$settings_original = file($settings_existing, FILE_IGNORE_NEW_LINES);
//TODO: improve?  assume first line is '<?php'
array_shift($settings_original);
$intersection = array_intersect($settings_local_include, $settings_original);
if (count($intersection) == count($settings_local_include)) {
  print "Conditional include exists in setting.php.\n";
}
elseif (count($intersection) == 0) {
  print "Adding conditional include to settings.php\n";
  $settings_modified = array_merge($settings_local_include, $settings_original);
  // replace '<?php'
  array_unshift($settings_modified, '<?php');
  $settings_modified[] = ""; //add a newline at end of file
  if (file_put_contents($settings_existing, implode("\n", $settings_modified)) === FALSE) {
    print "Error: Couldn't write $settings_existing\n";
    exit(1);
  }
}
else {
  print wordwrap("Conditional include code partially matched content of existing settings.php. Edit the file manually and ensure that the below code exists in the file.\n\n", 80);
  print $settings_local_include . "\n\n";
}


//////////////
// Database //
//////////////

//TODO: get tunnel into background. fork?


// Load latest db for site
//TODO: If no live env, try test
//TODO: make $to configurable
$to = '/tmp';
print "Getting the latest live database backup from Pantheon...\n";
$path = terminus_site_backups_get($site_name, 'live', 'database', $to);
if ($path !== FALSE) {
  // unpack db
  exec("cd $to; gzip -d $path", $output, $return);
  if ($return != 0) {
    print "Error: Couldn't unzip db.\n";
    exit(1);
  }
  $db_dump = preg_replace("/.gz$/", "", $path);

  // Create ssh tunnel to VM
  $cmd_ssh = "ssh -f -i ~/.vagrant.d/insecure_private_key -L 33060:localhost:3306 vagrant@" . $yaml['vagrant_hostname'] . " sleep 30 >> ~/tmp/sshlog";
  $cmd_ps = "ps -e |grep 'vagrant@'";
  exec($cmd_ps, $output, $return);
  if ($return != 0) {
    print "Error: Couldn't get process info.\n";
    exit(1);
  }
  $found = FALSE;
  foreach ($output as $out) {
    //print "$out\n";
    if (strpos($out, $cmd_ssh) !== FALSE) {
      $found = TRUE;
      break;
    }
  }
  if (!$found) {
// ssh tunnel doesn't exist, create one
    print "creating tunnel\n";
    exec($cmd_ssh, $output, $return);

    if ($return != 0) {
      print "Error: Couldn't initiate ssh tunnel to VM.\n";
      exit(1);
    }
  }
  else {
    print "found tunnel\n";
  }

  //hack.  ansible shuld do this.
  $cmd = "mysql -uroot -proot -P33060 -h127.0.0.1 $site_name < 'update db set Db=\'%\' where User=\'drupal\';mysql flush privileges;'";
  exec($cmd, $output, $return);
  if ($return != 0) {
    print "Warning: Couldn't set privs on db.\n";
    //exit(1);
  }

  // load db
  print "Loading database...\n";
  $cmd = "mysql -uroot -proot -P33060 -h127.0.0.1 $site_name < $db_dump";
  print "$cmd\n";
  //TODO: enable

  exec($cmd, $output, $return);
  if ($return != 0) {
    print "Error: Couldn't load db.\n";
    exit(1);
  }
  // clean up
  if (!unlink($db_dump)) {
    print "Warning: Couldn't clean up $db_dump\n";
  }

}

if (!in_array('no-files', array_keys($options))) {
// Load latest files for the site
  $to = '/tmp';
  print "Getting the latest live files backup from Pantheon...\n";
  $path = terminus_site_backups_get($site_name, 'live', 'files', $to);
  if ($path !== FALSE) {
    // unpack db
    //TODO: have gzip? have tar?
    print "Loading files backup...\n";
    $sites_default = "$site_dir/sites/default";
    // might not be writable after git clone from Pantheon
    //TODO: use php function
    exec("chmod -R a+xw $sites_default", $output, $return);
    if ($return != 0) {
      print "Error: Couldn't set permissions on $sites_default.\n";
      print implode("/n", $output);
      exit(1);
    }
    //TODO: improve. php builtins?
    exec("rm -rf $sites_default/files_live");
    exec("rm -rf $sites_default/files");
    exec("cd $sites_default ;tar zxf $path;mv files_live files", $output, $return);
    if ($return != 0) {
      print "Error: Couldn't untar files.\n";
      print implode("/n", $output);
      exit(1);
    }
    // clean up
    if (!unlink($path)) {
      print "Warning: Couldn't clean up $path\n";
    }
  }
}

// Display notification
if ((!$disable_notifications) && (in_array('notify', array_keys($options)))) {
  $cmd = "osascript -e 'display notification \"$site_name\" with title \"dvm\" subtitle \"Site(s) loaded on your VM\"'";
  if (!in_array('no-sound', array_keys($options))) {
    $cmd .= " sound name \"Purr\"";
  }
  unset($output);
  exec($cmd, $output, $return);
  if ($return != 0) {
    print "Warning: Couldn't create notification.\n";
    print implode("/n", $output);
  }
}

sleep(2);
$cmd = "open http://$site_name.localhost";
exec($cmd);