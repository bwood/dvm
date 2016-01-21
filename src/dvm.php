<?php

require __DIR__ . '/../vendor/autoload.php';

use Symfony\Component\Yaml\Yaml;

$file = __DIR__ . "/../plugins/drupalvm_config.yml";
$yaml = Yaml::parse(file_get_contents($file));

$yaml['known_hosts_path'] = 'CHANGE ME!';

$dumper = new Symfony\Component\Yaml\Dumper();
$yaml = $dumper->dump($yaml, 2);
print $yaml;
