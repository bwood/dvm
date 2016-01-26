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

  if (getenv("DVM_TERMINUS")) {
    $command = str_replace('terminus ', getenv("DVM_TERMINUS") . ' ', $command);
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
 * Log the user in to terminus v1 and v2
 */
function terminus_auth() {
  global $drush;
  $user = take_input("Pantheon username (email)");
  $pass = take_input("Pantheon dashboard password (won't echo to screen)", "", FALSE);
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

function terminus_site_backups_get($site_name, $env, $element, $to, $latest=TRUE) {
  ($latest) ? $latest = '--latest' : $latest = '';
  $terminus_backup_cmd = "terminus site backups get --site=$site_name --env=$env --element=$element --to=$to $latest";
  terminus_exec($terminus_backup_cmd, $output, $return, '2>&1', "", TRUE);
  if ($return !== 0) {
    print "\nError: There was a problem with the last command\n";
    return FALSE;
  }
  else {
    foreach ($output as $out) {
      if (strpos($out, 'target: ') !== FALSE) {
        $parts = explode(": '", $out);
        $path = str_replace("'", "", trim($parts[1]));
        return $path;
      }
    }
  }
  return FALSE;
}