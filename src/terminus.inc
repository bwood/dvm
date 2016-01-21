<?php
/**
 * Created by PhpStorm.
 * User: bwood
 * Date: 3/3/15
 * Time: 4:57 PM
 */

/**
 * Regular expressions for Terminus output strings that should be ignored.
 * TODO: Adjust per https://github.com/pantheon-systems/cli/issues/413
 *
 * @return array
 */
function terminus_output_ignore() {
  return array(
    'Warning: There is a newer version of Terminus.*$',
    '"INFO","message":"An update to Terminus is available',
    '[info] An update to Terminus is available.',
  );
}

/**
 * @param $string
 * @return bool
 */
function terminus_output_filter($string) {
  foreach (terminus_output_ignore() as $pattern) {
    if (preg_match("/$pattern/", $string)) {
      // if there's a match, return false to filter the match from the output
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Filter terminus output
 * Refer to https://github.com/pantheon-systems/cli/issues/413
 *
 * @param $command
 * @param $output
 * @param $return
 */
function terminus_exec($command, &$output, &$return, $io_redirection = '', $force_terminus_version = '', $verbose = FALSE) {

  if (getenv("ISTDRUPAL_TERMINUS")) {
    $command = str_replace('terminus ', getenv("ISTDRUPAL_TERMINUS") . ' ', $command);
    if (!empty($force_terminus_version)) {
      $command = preg_replace('/(terminus)-\d+/',  '$1' . "-$force_terminus_version", $command);
      // make sure that terminus executable exists
      $parts = explode(' ', $command);
      if (!is_file($parts[0]) || !is_executable($parts[0])) {
        print "Error: Terminus version $force_terminus_version is required for this command:\n";
        print "$command\n";
        print "The file " . $parts[0] . " either doesn't exist or isn't executable.\n";
        exit(1);
      }
    }

  }

  if (!empty($io_redirection)) {
    $command .= " " . $io_redirection;
  }

  if ($verbose) {
    print "$command\n";
  }
  exec($command, $output, $return);
  $output = array_filter($output, 'terminus_output_filter');
  // make sure the array keys still start at 0 and there are no gaps
  $output = array_values($output);
}

/**
 * @param bool|FALSE $json
 * @return mixed
 */
function terminusCliVersion($json = FALSE) {

  $terminusCliVersionCmd = "terminus cli version";
  $terminusCliVersionCmd .= ($json) ? " --format=json" : "";
  terminus_exec($terminusCliVersionCmd, $output, $result);
  return $output;
}

/**
 * @param string $version
 * @return bool
 */
function checkRequiredVersion($version = '') {

  if (empty($version)) {
    $data = terminusCliVersion(TRUE);
    $data = json_decode($data[0], TRUE);
    if (!count($data)) {
      //This is an older unsupported version of Terminus which doesn' support
      //--json
      return FALSE;
    }
    $version = $data['version'];
  }

  $currentVersionParts = explode('-', $version);
  //Discard pre-release info like -beta1 -alpha2
  $currentVersionParts = explode('.', $currentVersionParts[0]);

  $minVersionParts = explode('-', TERMINUS_REQUIRED_VERSION);
  //Discard pre-release info like -beta1 -alpha2
  $minVersionParts = explode('.', $minVersionParts[0]);

  if (
    ($currentVersionParts[0] != $minVersionParts[0]) ||
    ($currentVersionParts[1] != $minVersionParts[1]) ||
    ($currentVersionParts[2] != $minVersionParts[2])
  ) {
    return FALSE;
  }

  return TRUE;
}

/**
 * Validate user's options
 */
function validate_options() {
  global $argv, $longopts, $shortopts;
  $args = $argv;
  array_shift($args);
  $longs = array();
  $shorts = array();
  $bad_long = array();
  $bad_short = array();
  foreach ($args as $arg) {
    if (preg_match("/^--\w{2,}/", $arg)) {
      $arg = preg_replace('/^--/', '', $arg);
      $parts = explode('=', $arg);
      $longs[] = array_shift($parts);
    }
    // -B2  is valid, so just consider the first character
    if (preg_match("/^-\w/", $arg)) {
      $shorts[] = substr($arg, 1, 1);
    }
  }

  $opts_long = array();
  foreach ($longopts as $lopt) {
    $opts_long[] = str_replace(':', '', $lopt);
  }

  $bad_long = array_diff($longs, $opts_long);
  array_walk($bad_long, function (&$value, $key) {
    $value = "--$value";
  });
  $opts_short = str_replace(':', '', $shortopts);
  $bad_short = array_diff($shorts, str_split($opts_short));
  array_walk($bad_short, function (&$value, $key) {
    $value = "-$value";
  });

  $badness = array_merge($bad_short, $bad_long);
  if (count($badness)) {
    print "\nError: Unknown options were passed:\n\n";
    print implode("\n", $badness) . "\n\n";
    print "Use '" . $argv[0] . " -h' for help.\n";
    exit(1);
  }

}

/**
 * @param $question
 * @param null $default
 * @param bool $echo
 * @param array $list
 * @return null|string
 */
function take_input($question, $default = NULL, $echo = TRUE, $list = array()) {
  global $options;

  if (count($list)) {
    print "Choices:\n\n";
    foreach ($list as $key => $value) {
      print "\t[$key]";
      ($key < 10) ? print "  " : print " ";
      print "$value\n";
    }
  }

  (!empty($default)) ? $default_prompt = "(Default=$default)" : $default_prompt = NULL;
  (!empty($default_prompt)) ? $question = $question . " $default_prompt: " : $question = $question . ": ";
  print wordwrap($question, 80);

  // -y for non-interactive
  if ((in_array('y', array_keys($options))) && (!empty($default))) {
    // linefeed and response after the question
    print "$default\n";
    return $default;
  }

  $handle = fopen("php://stdin", "r");
  if (!$echo) {
    exec('stty -echo', $output, $return);
    if ($return !== 0) {
      print "Error: Can't obscure password with 'stty'.\n";
      exit(1);
    }
    unset($output);
    unset($return);
  }
  $input = trim(fgets($handle));
  if (!$echo) {
    exec('stty echo', $output, $return);
  }
  if (empty($input)) {
    if (!empty($default)) {
      return $default;
    }
  }

  if (count($list)) {
    if (strtolower($input) == "skip") {
      return strtolower($input);
    }
    if (!is_numeric($input) || !isset($list[$input])) {
      print "\nSorry '$input' is not a valid choice.\n";
      take_input($question, $default, $echo, $list);
    }
    else {
      return $list[$input];
    }
  }
  return $input;
}

/*
 * Dual-purpose Yes/No function: continues/exits script (default) or returns boolean value
 *
 * @param $question string
 * @param $boolean boolean
 */
/**
 * @param $question
 * @param bool $boolean
 * @return bool|void
 */
function yesno($question, $boolean = FALSE) {
  global $options;

  // -y for non-interactive
  if (in_array('y', array_keys($options))) {
    // Nice to see where the question was.
    echo $question . " (y/n): y\n";
    return TRUE;
  }

  $line = NULL;
  while ((strtolower(substr(trim($line), 0, 1)) != 'y') && (strtolower(substr(trim($line), 0, 1)) != 'n')) {
    if ($line !== NULL) {
      print "Please answer with \"y\" or \"n\"\n";
    }
    echo $question . " (y/n): ";
    $handle = fopen("php://stdin", "r");
    $line = fgets($handle);
  }
  if (strtolower(substr(trim($line), 0, 1)) != 'y') {
    if ($boolean) {
      return FALSE;
    }
    else {
      exit(0);
    }
  }
  if ($boolean) {
    return TRUE;
  }
  else {
    echo "\nContinuing...\n";
  }
  return;
}

/**
 * @param $options
 * @param $shortopts
 * @param $longopts
 * @param $usage
 */
function build_options(&$options, &$shortopts, &$longopts, $usage) {
  $shortopts = $shortopts . 'h';
  $longopts[] = 'help';
  $options = getopt($shortopts, $longopts);
  if ((in_array('h', array_keys($options))) || (in_array('help', array_keys($options)))) {
    print $usage . "\n";
    exit(0);
  }
}

/**
 * Make sure they are logged in to terminus v2
 */
function ensure_terminus_auth() {
  global $drush;
  //make sure the user is logged in to terminus v2
  $terminus_auth_whoami_cmd = "terminus auth whoami";
  terminus_exec($terminus_auth_whoami_cmd, $output, $return);

  if ($return != 0)  {
    print "\nYou are not fully authenticated Terminus.\n";
    terminus_auth();
  }
}

/**
 * Make sure site_name exists and user has access.
 *
 * @param $site_name
 * @return bool
 */
function ensure_site_access($site_name) {
  $info = terminus_site_info($site_name, FALSE);
  if ($info === FALSE) {
    print wordwrap("\nError: Either there is no site named $site_name, or you are not a team member on that site. (Currently you need to add yourself as a team member via the Pantehon dashboard.)\n\n", 80);
    exit(1);
  }
  return $info;
}

/**
 * @param $group
 * @param $site_name
 * @param $env
 * @return string
 */
function construct_drush_alias($group, $site_name, $env) {
  if (!empty($group)) {
    $group = "$group.";
  }
  if (!empty($env)) {
    $env = ".$env";
  }
  return '@' . $group . $site_name . $env;
}

/**
 * Make sure user has an alias for the site.
 *
 * @param $site_name
 * @param string $group
 * @param string $env
 * @return bool
 */
function ensure_drush_alias($site_name, $group = "pantheon", $env = "dev") {
  $aliases = drush_sa();
  if (!$aliases) {
    print "\nError: Could not run drush sa\n\n";
    return FALSE;
  }
  if (!is_array($aliases)) {
    return FALSE;
  }
  $site_alias = construct_drush_alias($group, $site_name, $env);
  if (!in_array($site_alias, $aliases)) {
    print "\nYou don't have a drush alias for $site_name.\n";
    if (yesno("Want to refresh your Pantheon aliases?", TRUE)) {
      terminus_sites_aliases();
    }
  }
}

/**
 * Log the user in to terminus v1 and v2
 */
function terminus_auth() {
  global $drush;
  $user = take_input("Pantheon username (email)");
  $pass = take_input("Pantheon dashboard password (won't echo to screen)", "", FALSE);

  /*
  print wordwrap("\nCurrently you need to login to both (yes, both) versions of terminus.(Logging in again won't hurt anything.)\n\n", 80);
  $terminus_v1_cmd = "$drush pauth " . trim($user) . " --password='" . trim($pass) . "'";
  print "\nAuthenticating to Terminus v1...\n";
  terminus_exec($terminus_v1_cmd, $output, $return);
  if ($return !== 0) {
    "Error authenticating to terminus v1\n";
    exit(1);
  }
  else {
    print "Success!\n";
  }
  unset($output);
  unset($return);
  */
  $terminus_v2_cmd = "terminus auth login " . trim($user) . " --password='" . trim($pass) . "'";
  print "\n\nAuthenticating to Terminus...\n";
  terminus_exec($terminus_v2_cmd, $output, $return);
  if ($return !== 0) {
    print "Error: Authentication failed.\n";
    exit(1);
  }
  else {
    print "Success!\n";
  }
}

// Script functions

/**
 * Wrapper for terminus site info
 *
 * @param $site_name
 * @param bool $verbose
 * @return bool|mixed
 */
function terminus_site_info($site_name, $verbose = TRUE) {
  $terminus_site_info_cmd = "terminus --format=json site info --site=$site_name";
  terminus_exec($terminus_site_info_cmd, $output, $return);
  if ($return != 0) {
    if ($verbose) {
      print implode("\n", $output) . "\n";
      print "\nError: There was a problem with the last command\n";
    }
    return FALSE;
  }
  $site_info = json_decode($output[0], TRUE);
  return $site_info;
}

/**
 * @param $local_paths
 * @return string
 */
function script_pathologic($local_paths) {
  $script = "\$local_paths =<<<EOT\n";
  $script .= $local_paths . "\n";
  $script .= "EOT;\n";
  $script .= <<<'EOT'
if (module_exists("openberkeley_wysiwyg_override")) {
  openberkeley_wysiwyg_override_update_pathologic_paths($local_paths);
}
else {
  print "Error: Module openberkeley_wysiwyg_override doesn't exist.\n";
}

EOT;

  $script_file = "/tmp/" . __FUNCTION__ . ".php";
  $fp = fopen($script_file, "w");
  fwrite($fp, $script);
  fclose($fp);

  return $script_file;
}

/**
 * Some modules have unexpected machine names. E.g:
 *
 * $ drush pmi googleanalytics --fields=extension,project,configure
 * Extension   :  googleanalytics
 * Project     :  google_analytics
 * Configure        :  admin/config/system/googleanalytics
 *
 * If we get the project as input, return the extension
 *
 * @param $string apparent machine name of module
 * @return mixed
 */
function module_name_translate($string) {
  $need_translation = array(
    'google_analytics' => 'googleanalytics',
  );
  if (in_array($string, array_keys($need_translation))) {
    return $need_translation[$string];
  }
  return $string;
}

/**
 * Wrapper for terminus auth whoami.
 * Returns the logged in user's email or FALSE.
 *
 * @return bool|mixed
 */
function terminus_auth_whoami() {
  $terminus_auth_whoami_cmd = "terminus auth whoami";
  print $terminus_auth_whoami_cmd . "\n";
  terminus_exec($terminus_auth_whoami_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  if (!isset($output[0])) {
    return FALSE;
  }
  else {
    return str_replace('You are authenticated as: ', '', $output[0]);
  }
}

/**
 * Wrapper for terminus site team add-member
 * @param $email
 * @param string $role
 * @return array|bool
 */
function terminus_site_team_add_member($email, $role = "admin") {
  global $site_name;
  $output = array();
  $terminus_site_team_add_member_cmd = "terminus site team add-member --site=$site_name --member=$email --role=$role";
  print $terminus_site_team_add_member_cmd . "\n";
  terminus_exec($terminus_site_team_add_member_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  return $output;
}


/**
 * Wrapper for terminus site team list
 *
 * @return array|bool
 */
function terminus_site_team_list() {
  global $site_name;
  $output = array();
  $terminus_site_team_list_cmd = "terminus site team list --site=$site_name";
  print $terminus_site_team_list_cmd . "\n";
  terminus_exec($terminus_site_team_list_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  return $output;
}

/**
 * Wrapper for terminus site upstream_updates.
 *
 * @param $site_name
 * @param string $op_env
 * @param bool $yes
 * @param bool $accept_upstream
 * @param string $action "apply" or "list"
 * @return array|bool
 */
function terminus_site_upstream_updates($site_name, $op_env = "", $yes = FALSE, $accept_upstream = TRUE, $action = "apply") {
  $output = array();
  ($yes) ? $yes = '--yes' : $yes = "";
  (!empty($op_env)) ? $op_env = "--env=$op_env" : $op_env = "";
  ($accept_upstream) ? $accept_upstream = "--accept-upstream" : $accept_upstream = "";
  $terminus_upstream_cmd = "terminus site upstream-updates $action --site=$site_name $op_env $yes $accept_upstream";
  print $terminus_upstream_cmd . "\n";
  print "This takes a few minutes...\n";
  terminus_exec($terminus_upstream_cmd, $output, $return, "2>&1");
  if ($return !== 0) {
    print "\nError: There was a problem with the last command. Here's the output:\n\n";
    // Something failed, so show output.
    print implode("\n", $output);
    return FALSE;
  }
  print implode("\n", $output) . "\n";
  if ((!empty($op_env)) && (!empty($yes))) {
    // We wanted to apply the updates to an environment. Did we succeed?

    // strpos easier if needle is followed by "   [success]" etc.
    $output_flat = implode(' ', $output);
    if (strpos($output_flat, '[info] Applied upstream updates') !== FALSE) {
      print "\n$site_name is up-to-date with its upstream repository.\n\n";
      return TRUE;
    }
    else {
      return FALSE;
    }
  }
  // We wanted to return upstream_updates data
  return $output;
}

/**
 * Wrapper for terminus site backup create.
 *
 * We've observed reports of success when backups have actually failed. Use
 * $verify argument to check the backup against the list of backups.
 *
 * @param $site_name
 * @param $env
 * @param string $element
 * @param bool $verify
 * @param $retry
 * @return array|bool
 */
function terminus_site_backups_create($site_name, $env, $element = "all", $verify = FALSE, $retry = 0, $keep_for = 30) {
  $errors_retry = array(
    // Add retry conditions should they be discovered.
  );
  $max_retries = 5;
  $terminus_backup_cmd = "terminus site backups create --site=$site_name --env=$env --element=$element --keep-for=$keep_for";
  print $terminus_backup_cmd . "\n";
  terminus_exec($terminus_backup_cmd, $output, $return);
  $output = replace_strings($output);
  print implode("\n", $output) . "\n";
  if ($return !== 0) {
    $first_line = array_shift($output);
    if ((in_array(trim($first_line), $errors_retry)) && ($retry < $max_retries)) {
      print "Detected known Pantheon error. Retrying...\n";
      sleep(5);
      $retry++;
      print "Backup retry pass: $retry";
      terminus_site_backups_create($site_name, $env, $element, $verify, $retry);
    }
    else {
      print "\nError: There was a problem with the last command\n";
      return FALSE;
    }
  }
  if ($verify) {
    date_default_timezone_set('UTC');
    $max_difference = 60 * 2;
    $backup_complete_unix_time = time();
    $backup_complete_time = date("Y-m-d H:i:s", $backup_complete_unix_time);
    $verify_backups = terminus_site_backups_list($site_name, $env, $element, TRUE);
    $difference = $backup_complete_unix_time - $verify_backups['last_backup'];
    if ($difference < $max_difference) {
      return TRUE;
    }
    else {
      print wordwrap("\nProblem: We couldn't find a backup with a timestamp within $max_difference seconds of the completion time ($backup_complete_time UTC) of our backup attempt.\n", 80);
      // Calling code can optionally allow the user to view $verify_backups['all_backups']
      return $verify_backups;
    }
  }
  else {
    return TRUE;
  }
}

/**
 * Wrapper for terminus site backup list.
 *
 * @param $site_name
 * @param $env
 * @param string $elements
 * @param bool $verify
 * @return array|bool
 */
function terminus_site_backups_list($site_name, $env, $elements = "all", $verify = FALSE) {
  $terminus_backup_cmd = "terminus site backups list --site=$site_name --env=$env --format=json";
  if ($verify) {
    print "Verifying the last backup...\n";
  }
  print $terminus_backup_cmd . "\n";
  terminus_exec($terminus_backup_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  if ($verify) {
    date_default_timezone_set('UTC');
    if ($elements == "all") {
      $elements = array('code', 'database', 'files');
    }
    else {
      $elements = array($elements);
    }
    $backups = json_decode(implode("\n", $output), TRUE);
    // calculate the negative number since we want N items from the end of the array
    $offset = 0 - count($elements);
    $last_backups = array_slice($backups, $offset);
    foreach ($last_backups as $backup) {
      // timestamp should be the last element.
      $timestamp = array_pop($backup);
      $unix_timestamp = strtotime($timestamp);
      if ($unix_timestamp) {
        $element_timestamps[$unix_timestamp] = $timestamp;
      }
      else {
        print "\nError: Couldn't parse timestamp.\n\n";
        exit(1);
      }
    }
    // sort by key (unix timestamp) lowest to highest
    ksort($element_timestamps);
    // return the latest timestamp for comparison.
    // e.g. if files element backup finished last, return that timestamp
    $keys = array_keys($element_timestamps);
    $last_backup = array_pop($keys);
    return array(
      'last_backup' => $last_backup,
      'all_backups' => $backups
    );
  }
  else {
    return $output;
  }
}

/**
 * @return bool
 */
function terminus_sites_aliases() {
  $terminus_sites_aliases_cmd = "terminus sites aliases";
  terminus_exec($terminus_sites_aliases_cmd, $output, $return);
  print implode("\n", $output) . "\n";
  if ($return != 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  return TRUE;
}

/**
 * Wrapper for terminus site code log
 *
 * @param $site_name
 * @param $env
 * @param bool $json
 * @return bool
 */
function terminus_site_code_log($site_name, $env, $json = FALSE) {
  ($json) ? $json = '--format=json' : $json = '';
  $terminus_site_code_log_cmd = "terminus $json site code log --site=$site_name --env=$env";
  print $terminus_site_code_log_cmd . "\n";
  terminus_exec($terminus_site_code_log_cmd, $output, $return);
  if ($return !== 0) {
    return FALSE;
  }
  return $output;
}

/**
 * Wrapper for terminus site clone-env
 *
 * @param $site_name
 * @param $from_env
 * @param $to_env
 * @param bool $yes
 * @return bool
 */
function terminus_site_clone_content($site_name, $from_env, $to_env, $yes = TRUE) {
  ($yes) ? $yes = '--yes' : $yes = "";
  $terminus_clone_cmd = "terminus site clone-content --site=$site_name --from-env=$from_env --to-env=$to_env $yes";
  print $terminus_clone_cmd . "\n";
  terminus_exec($terminus_clone_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  print implode("\n", $output) . "\n";
  return TRUE;
}


function terminus_sites_create($name, $label, $upstream, $org) {
  global $site_name;
  $terminus_sites_create_cmd = "terminus --yes sites create --site=$site_name --name=$name --label=$label --upstream=$upstream --org=$org";
  print $terminus_sites_create_cmd . "\n";
  terminus_exec($terminus_sites_create_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  print implode("\n", $output) . "\n";
  return TRUE;
}

/**
 * @param $site_name
 * @param $env
 * @param bool $print
 * @return bool
 */
function terminus_site_dashboard($site_name, $env, $print = TRUE, $yes = FALSE) {
  ($yes) ? $yes = '--yes' : $yes = "";
  ($print) ? $print = '--print' : $print = "";
  $terminus_dashboard_cmd = "terminus $yes site dashboard --site=$site_name --env=$env $print";
  print $terminus_dashboard_cmd . "\n";
  terminus_exec($terminus_dashboard_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  print implode("\n", $output) . "\n";
  return TRUE;
}


/**
 * Wrapper for terminus site deploy
 *
 * @param $site_name
 * @param $to_env
 * @param string $note
 * @return bool
 * @internal param $from_env
 */
function terminus_site_deploy($site_name, $to_env, $note = "") {
  //$to_env implies $from_env.  If env=live from is test. If env=test from is dev.
  $terminus_deploy_cmd = "terminus site deploy --site=$site_name --env=$to_env --cc --updatedb --note=\"$note\"";
  print $terminus_deploy_cmd . "\n";
  terminus_exec($terminus_deploy_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  print implode("\n", $output) . "\n";
  return TRUE;
}

/**
 * Wrapper for terminus site envionrments.
 *
 * @param $site_name
 * @param string $environment
 * @param bool|TRUE $json
 * @return bool|mixed
 */
function terminus_site_environments($site_name, $environment = '', $json = TRUE) {
  ($json) ? $json = '--format=json' : $json = '';
  $terminus_site_environments_cmd = "terminus site environments --site=$site_name $json";
  terminus_exec($terminus_site_environments_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  $data = json_decode($output[0], TRUE);
  if (!empty($environment)) {
    foreach ($data as $env) {
      if ($env['name'] == $environment) {
        return $env;
      }
    }
  }
  return $data;
}


/**
 * Wrapper for terminus site connection-mode
 *
 * @param $site_name
 * @param $env
 * @param $mode
 * @param bool $verify
 * @return bool
 */
function terminus_site_connection_mode($site_name, $env, $mode, $verify = TRUE) {
  $mode = strtolower($mode);
  $terminus_cmode_cmd = "terminus site set-connection-mode --site=$site_name --env=$env --mode=$mode";
  print $terminus_cmode_cmd . "\n";
  terminus_exec($terminus_cmode_cmd, $output, $return);
  print "This takes a minute or two...\n";
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  print implode("\n", $output) . "\n";
  unset($return);
  unset($output);
  if (!$verify) {
    return TRUE;
  }
  //Verify the cmode is set!
  $terminus_cmode_verify_cmd = "terminus site environment-info --site=$site_name --env=$env --field=connection_mode";
  $got_cmode = "";
  terminus_exec($terminus_cmode_verify_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the verification command\n";
    return FALSE;
  }
  $output_parts = explode(': ', $output[0]);
  $got_cmode = trim(strtolower(array_pop($output_parts)));

  if ($got_cmode == $mode) {
    return TRUE;
  }
  else {
    print "Connection mode change failed. Current setting: $got_cmode\n";
    return FALSE;
  }
}


/**
 * @param $site_name
 * @param $env
 * @param string $message
 * @return bool
 */
function terminus_site_code_commit($site_name, $env, $message = "") {
  $terminus_commit_cmd = "terminus --yes site code commit --site=$site_name --env=$env --message=\"$message\"";
  print "\n" . $terminus_commit_cmd . "\n";
  terminus_exec($terminus_commit_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  print implode("\n", $output) . "\n";
  return TRUE;
}

/**
 * @param $site_alias
 * @param bool|FALSE $fire_bazooka
 * @param bool|FALSE $no_cache_clear
 * @return array|bool
 */
function drush_rr($site_alias, $fire_bazooka = FALSE, $no_cache_clear = FALSE) {
  global $drush;
  $output = array();
  $drush_rr_cmd = "$drush $site_alias registry-rebuild";

  if ($fire_bazooka) {
    $drush_rr_cmd .= " --fire-bazooka";
  }

  if ($no_cache_clear) {
    $drush_rr_cmd .= " --no-cache-clear";
  }

  // Takes a while. Let the user know what's going on.
  print "\n" . $drush_rr_cmd . "\n";

  exec($drush_rr_cmd, $output, $return);
  if ($return !== 0) {
    print implode("\n", $output) . "\n";
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  return $output;
}

/**
 * Wrapper for drush site-alias.
 *
 * @return array|bool
 */
function drush_sa() {
  global $drush;
  $output = array();
  $drush_sa_cmd = "$drush sa ";
  // print $drush_sa_cmd . "\n";
  exec($drush_sa_cmd, $output, $return);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command.\n";
    print "\nTry re-downloading your Pantheon aliases.\n";
    return FALSE;
  }
  return $output;
}

/**
 * Wrapper for drush sqlq
 *
 * @param $site_alias
 * @param $sql
 * @return array|bool
 */
function drush_sqlq($site_alias, $sql) {
  global $drush;
  $output = array();
  $drush_sqlq_cmd = "$drush $site_alias sqlq \"$sql\"";

  exec($drush_sqlq_cmd, $output, $return);
  if ($return !== 0) {
    print $drush_sqlq_cmd . "\n";
    print implode("\n", $output) . "\n";
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  return $output;
}

/**
 * Wrapper for drush pm-updatecode
 *
 * @param $site_alias
 * @param array $modules
 * @param bool $yes
 * @return bool
 */
function drush_upc($site_alias, $modules = array(), $yes = FALSE) {
  global $drush;
  if (count($modules) == 0) {
    print "No modules to update.\n";
    return TRUE;
  }
  $modules_to_update = implode(" ", $modules);
  $say_yes = "";
  if ($yes) {
    $say_yes = '-y';
  }
  // Even though we use -y we still need to echo in a second "y" for
  $drush_upc_cmd = "$drush $site_alias upc $modules_to_update $say_yes";
  print $drush_upc_cmd . "\n";
  exec($drush_upc_cmd, $output, $return);
  if ($return !== 0) {
    return $output;
  }
  //filter output so we only see lines pertaining to the modules passed
  foreach ($output as $out) {
    $done = FALSE;
    foreach ($modules as $module) {
      if ((!$done) && (strpos($out, $module) !== FALSE)) {
        print $out . "\n";
        $done = TRUE;
      }
    }
  }
  return TRUE;
}

/**
 * Wrapper for drush pm-updatestatus.
 *
 * @param $site_name
 * @param string $env
 * @param bool $security_only
 * @param array $projects
 * @param bool $field_labels
 * @param array $fields
 * @param array $lock_projects
 * @param string $lock_message
 * @param array $unlock_projects
 * @return bool|mixed
 */
function drush_pm_updatestatus($site_name, $env = "", $security_only = FALSE, $projects = array(), $field_labels = TRUE, $fields = array(), $lock_projects = array(), $lock_message = "", $unlock_projects = array()) {
  global $drush;

  (count($projects)) ? $projects = ' ' . implode(" ", $projects) . ' ' : $projects = "";
  ($security_only) ? $security_only = " --security-only " : $security_only = "";
  ($field_labels) ? $field_labels = '' : $field_labels = ' --field-labels=0 ';
  (count($fields)) ? $fields = ' --fields=' . implode(",", $fields) . ' ' : $fields = "";
  (count($lock_projects)) ? $lock_projects = ' --lock=' . implode(",", $lock_projects) . ' ' : $lock_projects = "";
  (!empty($lock_message)) ? $lock_message = " --lock-message=$lock_message " : $lock_message = '';
  (count($unlock_projects)) ? $locked_projects = ' --lock=' . implode(",", $unlock_projects) . ' ' : $unlock_projects = "";

  $site_alias = construct_drush_alias("pantheon", $site_name, $env);
  $drush_ups_cmd = "$drush $site_alias pm-updatestatus --format=json" . $projects . $security_only . $field_labels . $fields . $lock_projects . $lock_message . $unlock_projects;
  exec($drush_ups_cmd, $output, $return);
  if ($return != 0) {
    print implode("\n", $output) . "\n";
    return FALSE;
  }
  return json_decode(array_pop($output));
}


/**
 * Wrapper for drush updatedb.
 *
 * If updatedb returns the same message multiple times stop trying.
 *
 * @param $site_alias
 * @param bool $yes
 * @param bool $multiple Try until there are no more updates to resolve
 * @param bool $verbose
 * @param int $limit
 * @return bool
 */
function drush_updb($site_alias, $yes = TRUE, $multiple = FALSE, $verbose = TRUE, $limit = 10) {
  $i = 1;
  $done_message = 'No database updates required';
  $some_other_message = '';

  if (!$multiple) {
    $limit = 1;
  }

  while ($i <= $limit) {
    //TODO: use construct_site_alias
    $output = _drush_updb($site_alias, $yes);
    if ($output === FALSE) {
      return FALSE;
    }
    if (!$multiple) {
      return $output;
    }
    foreach ($output as $out) {
      if (strpos($out, $done_message) !== FALSE) {
        return TRUE;
      }
    }
    reset($output);
    $first_line = array_shift($output);
    if ($first_line == $some_other_message) {
      print "\nPossible problem: The last two runs of updatedb have yeilded the same message:\n\n";
      print $output[0] . "\n";
      print "\nYou should take a look at that.\n\n";
      return FALSE;
    }
    else {
      $some_other_message = $first_line;
    }
    $i++;
    if ($verbose) {
      print "\n--> Database updates pass $i\n\n";
    }
    unset($output);
  }

  if ($i == $limit) {
    print wordwrap("\nError: Reached updatedb limit ($limit). Updates unresolved. Better investigate.\n\n", 80);
    exit(1);
  }
  return TRUE;
}

/**
 * @return bool
 */
function drush_version() {
  $drush_version_cmd = 'drush --version';
  exec($drush_version_cmd, $output, $return);
  $output = replace_strings($output);
  if ($return !== 0) {
    return FALSE;
  }
  return $output[0];
}

/**
 * @param $minimum_version
 * @return bool
 */
function ensure_minimum_php_verson($minimum_version) {
  if (preg_match("/[^\d\.]/", $minimum_version)) {
    print "Error: PHP version can only contain numbers and periods.\n";
    return FALSE;
  }

  $version_parts = explode('.', phpversion());
  $minimum_version_parts = explode('.', $minimum_version);
  if (
    ($version_parts[0] < $minimum_version_parts[0]) ||
    (($version_parts[0] = $minimum_version_parts[0]) && ($version_parts[1] < $minimum_version_parts[1])) ||
    (($version_parts[0] = $minimum_version_parts[0]) && ($version_parts[1] = $minimum_version_parts[1])) &&($version_parts[2] < $minimum_version_parts[2])
  ) {
    return FALSE;
  }

  return TRUE;
}


/**
 * @param $minimum_version
 * @return bool
 */
function ensure_minimum_drush_version($minimum_version) {
  $version = drush_version();
  $parts = explode('  :  ', $version);
  if (count($parts) != 2) {
    return FALSE;
  }
  $parts = explode('-', $parts[1]);
  // lazily ignoring -betaN and -rcN and -alphaN
  $version_parts = explode('.', $parts[0]);
  if (count($version_parts) != 3) {
    return FALSE;
  }
  $minimum_version_parts = explode('.', $minimum_version);
  if (
    ($version_parts[0] < $minimum_version_parts[0]) ||
    (($version_parts[0] = $minimum_version_parts[0]) && ($version_parts[1] < $minimum_version_parts[1])) ||
    (($version_parts[0] = $minimum_version_parts[0]) && ($version_parts[1] = $minimum_version_parts[1])) && ($version_parts[2] < $minimum_version_parts[2])
  ) {
    return FALSE;
  }
  return TRUE;
}

/**
 * @param $site_alias
 * @param $yes
 * @return bool|mixed
 */
function _drush_updb($site_alias, $yes) {
  global $drush;
  $say_yes = "";
  if ($yes) {
    $say_yes = '-y';
  }
  $drush_updb_cmd = "$drush $site_alias updb " . $say_yes;
  print $drush_updb_cmd . "\n";
  exec($drush_updb_cmd, $output, $return);
  $output = replace_strings($output);
  print implode("\n", $output) . "\n";
  if ($return !== 0) {
    return FALSE;
  }
  return $output;
}

/**
 * Wrapper for drush features-list
 *
 * @param $site_alias
 * @param bool $exit_on_error
 * @return bool
 */
function drush_fl($site_alias, $exit_on_error = FALSE) {
  global $drush;

  //TODO: Might want to break this into two wrappers?
  $drush_fl_cmd = "$drush $site_alias fl";
  print $drush_fl_cmd . "\n";
  exec($drush_fl_cmd, $output, $return);
  print implode("\n", $output) . "\n";
  if ($return != 0) {
    if ($exit_on_error) {
      print "\nError: There was a problem with the last command.\n\n";
      exit(1);
    }
    return FALSE;
  }
  return $output;
}

/**
 * Parse drush_fl() output for overridden features
 *
 * @param array $output
 * @return array
 */
function drush_fl_parse_overrides(array $output) {
  $overrides = array();
  foreach ($output as $out) {
    //TODO: Improve very loose Overridden regexp which could produce false positives. (Wouldn't hurt anything if it did.)
    //FIXME: Overridden will appear one character per line. Can we tweek a terminal setting to avoid this?
    //Sometimes it's 2 spaces, sometimes 3...
    if (preg_match("/\s{2,}O/", $out) == 1) {
      $parts = explode('  ', $out);
      $parts_clean = array();
      foreach ($parts as $part) {
        if (!empty($part)) {
          $parts_clean[] = trim($part);
        }
      }
      if (array_key_exists(4, $parts_clean)) {
        $state = $parts_clean[4];
      }
      else {
        $state = "";
      }
      $overrides[$parts_clean[1]] = array(
        'name' => $parts_clean[0],
        'machine_name' => $parts_clean[1],
        'status' => $parts_clean[2],
        'version' => $parts_clean[3],
        'state' => $state
      );
    }
  }
  return $overrides;
}

/**
 * Wrapper for drush cache-clear.
 *
 * @param $site_name
 * @param $env
 * @param $cache
 * @return bool
 */
function drush_cc($site_name, $env, $cache) {
  global $drush;
  $site_alias = construct_drush_alias('pantheon', $site_name, $env);
  $drush_cc_cmd = "$drush $site_alias cc $cache";
  print $drush_cc_cmd . "\n";
  exec($drush_cc_cmd, $output, $return);
  print implode("\n", $output) . "\n";
  if ($return != 0) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Wrapper for drush features-diff
 *
 * @param $site_alias
 * @param $feature
 * @return bool
 */
function drush_fd($site_alias, $feature) {
  global $drush;

  $drush_fd_cmd = "$drush $site_alias fd $feature";
  print $drush_fd_cmd . "\n";
  exec($drush_fd_cmd, $output, $return);
  print implode("\n", $output) . "\n";
  if ($return != 0) {
    return FALSE;
  }
  return $output;
}


/**
 * Wrapper for drush features-revert
 *
 * @param $site_alias
 * @param $feature
 * @return bool
 * @internal param array $features
 */
function drush_fr($site_alias, $feature) {
  global $drush;
  $drush_fra_cmd = "$drush $site_alias fr $feature -y";
  print $drush_fra_cmd . "\n";
  exec($drush_fra_cmd, $output, $return);
  print implode("\n", $output) . "\n";
  if ($return != 0) {
    return FALSE;
  }
  return $output;
}

/**
 * Wrapper for drush features-revert-all
 *
 * @param $site_alias
 * @param string $overrides
 * @return bool
 */
function drush_fra($site_alias, $overrides = '') {
  global $drush;
  if (count($overrides)) {
    print "The following features are now overridden:\n\n";
    print implode("\n", $overrides) . "\n\n";
  }
  if (yesno("Would you like to revert all features?", TRUE)) {
    $drush_fra_cmd = "$drush $site_alias fra -y";
    print $drush_fra_cmd . "\n";
    exec($drush_fra_cmd, $output, $return);
    print implode("\n", $output) . "\n";
    if ($return != 0) {
      return FALSE;
    }
    return $output;
  }
}

/*
 * Common tasks
 */

/**
 * Perform common string replacements.
 *
 * @param $subject
 * @param array $user_patterns
 * @param array $replacements
 * @return mixed
 * @internal param array $output
 */
function replace_strings($subject, $user_patterns = array(), $replacements = array()) {
  $patterns = array(
    "/RSA host key for IP address '[\d]{1,3}.[\d]{1,3}.[\d]{1,3}.[\d]{1,3}' not in list of known hosts\./",
    "/^Warning: Permanently added .*$/"
  );
  $patterns = array_merge($patterns, $user_patterns);
  // Make replacements and remove empty items from array with array_filter
  return array_filter(preg_replace($patterns, $replacements, $subject));
}

/**
 * Check an environment for commits.
 *
 * @param $site_name
 * @param $env
 * @return bool|array
 */
function check_environment_commits($site_name, $env) {
  print "Checking for commits in the \"$env\" environment.\n";
  $commits = terminus_site_code_log($site_name, $env, TRUE);
  if ($commits === FALSE) {
    print "Error: Can't check commit log in the $env environment.\n";
    exit(1);
  }
  if (count($commits) > 0) {
    return json_decode(implode("\n", $commits), TRUE);
  }
  else {
    //no commits in the env
    return FALSE;
  }
}

/**
 * @param $site_alias
 * @return bool
 */
function check_environment_db($site_alias) {
  print "Checking database for $site_alias.\n";
  $tables = drush_sqlq($site_alias, "show tables");
  if ($tables === FALSE) {
    print "Error: Failed to check database in $site_alias.\n";
    exit(1);
  }
  //Will D8 have a system table?
  if ((count($tables) > 0) && (in_array('system', $tables))) {
    return TRUE;
  }
  else {
    //no db
    return FALSE;
  }
}

/**
 * Intelligent content clone.
 * Don't clone empty environments.
 * Attempt to fallback to Test if there is no Live source environment
 *
 * @param $site_name
 * @param $from_env
 * @param $to_env
 * @param string $fallback_env
 * @return bool
 * @internal param $env
 */
function clone_content($site_name, $from_env, $to_env, $fallback_env = "") {
  // Check database
  $site_alias = construct_drush_alias('pantheon', $site_name, $from_env);
  $db_has_tables = check_environment_db($site_alias, $from_env);

  if ($db_has_tables) {
    if (!terminus_site_clone_content($site_name, $from_env, $to_env)) {
      exit(1);
    }
  }
  else {
    print wordwrap("\nNo database found in the $from_env environment.\n\n", 80);
    if (!empty($fallback_env)) {
      if (yesno("Would you like to clone from the \"$fallback_env\" environment instead?", TRUE)) {
        clone_content($site_name, $fallback_env, $to_env);
      }
    }
    return FALSE;
  }
}

/**
 * Display interactive dialog for checking features overrides.
 *
 * @param $site_name
 * @param $env
 * @param $procedure_message
 * @param bool $diff
 */
function features_check_overrides($site_name, $env, $procedure_message, $diff = TRUE) {
  if (yesno("Would you like to look for overridden features?", TRUE)) {
    if (!drush_cc($site_name, $env, 'all')) {
      print "\nWarning: Failed to clear all caches.\n\n";
    }
    print "\n----------------------------------------------------------------------\n";
    print "BEGIN: " . strtoupper($env) . ": Features and customizations\n";
    print "(" . ucwords($procedure_message) . ")\n";
    print "----------------------------------------------------------------------\n";
    features_list_diff_revert($site_name, $env, $diff);
    print "\n----------------------------------------------------------------------\n";
    print "END: " . strtoupper($env) . ": Features and customizations\n";
    print "(" . ucwords($procedure_message) . ")\n";
    print "----------------------------------------------------------------------\n";
  }
}

/**
 * List and optionally diff/revert overridden features.
 *
 * @param $site_name
 * @param $env
 * @param bool $diff
 * @return bool
 * @internal param $site_alias
 */
function features_list_diff_revert($site_name, $env, $diff = TRUE) {
  ($diff) ? $diff = 'y' : $diff = 'n';
  $site_alias = construct_drush_alias('pantheon', $site_name, $env);
  $output = drush_fl($site_alias, TRUE);
  if ($output === FALSE) {
    print "Unable to list features.\n";
    return FALSE;
  }
  print "\nSee the features list above.\n";
  $overrides = array_keys(drush_fl_parse_overrides($output));
  if (!count($overrides)) {
    print "No overridden features detected.\n\n";
    return TRUE;
  }
  else {
    print "Overridden features detected.\n";
    if (!yesno("Want to look at diffs and/or revert features?", TRUE)) {
      return TRUE;
    }
    print "\nHelpful URLs (iTerm: Cmd+click):\n\n";
    print "\thttp://$env-$site_name.pantheon.berkeley.edu/cas\n";
    print "\thttp://$env-$site_name.pantheon.berkeley.edu/admin/structure/features\n\n";
  }
  foreach ($overrides as $machine_name) {
    if (strtolower(substr(take_input("Want to see features-diff for the overridden \"$machine_name\" in " . strtoupper($env) . "?", $diff), 0, 1)) == 'y') {
      print "\n";
      print <<<EOT
HELP:

'<' : Lines preceeded with '<' are showing the default state of the feature.
'>' : Lines preceeded with '>' are showing the differences present in the site.

EOT;
      print "\n";
      if (drush_fd($site_alias, $machine_name) === FALSE) {
        print "\nProblem diffing the feature.\n\n";
      }
    }
    if (strtolower(substr(take_input("Want revert the overridden $machine_name?", "N"), 0, 1)) == 'y') {
      if (drush_fr($site_alias, $machine_name) === FALSE) {
        print "\nProblem reverting the feature.\n\n";
      }
    }
  }
  return TRUE;
}

/**
 * Modify drush aliases file to allow setting $drush_script.
 * This has the effect of modify the value for every instance of
 * $path-aliases['%drush-script'] which governs the version of drush to be used
 * on the remote server.
 *
 * Greg Anderson said this would work, but it doesn't...
 *
 * @param $path_to_aliases
 */
function add_drush_script_to_aliases($path_to_aliases) {
  $php_code = '$drush_script = getenv(\'DRUSH_SCRIPT\'); if (!drush_script) { $drush_script = "drush"; }';
  $aliases_content = file_get_contents($path_to_aliases);
  if (strpos($aliases_content, $php_code) === FALSE) {
    file_put_contents($path_to_aliases, str_replace('<?php', "<?php\n" . $php_code, file_get_contents($path_to_aliases)));
  }
}

/**
 * Set shell environment variable governing the value of $drush_script.
 *
 * @see add_drush_script_to_aliases()
 * @param $drush_script
 */
function set_drush_script($drush_script) {
  putenv("DRUSH_SCRIPT=$drush_script");
}

/**
 * Replace strings in a file.
 *
 * E.g. update the %drush-script in the pantheon aliases file.
 *
 * @param $path_to_file
 * @param $find
 * @param $replace
 */
function file_find_replace($path_to_file, $find, $replace) {
  file_put_contents($path_to_file, str_replace($find, $replace, file_get_contents($path_to_file)));
}

