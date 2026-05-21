--TEST--
voyager_method_redefine preserves return-by-reference methods by default
--SKIPIF--
<?php
if (!extension_loaded('voyager')) {
    die('skip voyager extension not loaded');
}
?>
--FILE--
<?php
class VoyagerMethodRefFixture {
    public function &defaultCase(): string {
        static $value = 'original default';
        return $value;
    }

    public function &nullCase(): string {
        static $value = 'original null';
        return $value;
    }
}

$fixture = new VoyagerMethodRefFixture();

voyager_method_redefine(
    VoyagerMethodRefFixture::class,
    'defaultCase',
    '',
    'static $value = "redefined default"; return $value;'
);

voyager_method_redefine(
    VoyagerMethodRefFixture::class,
    'nullCase',
    '',
    'static $value = "redefined null"; return $value;',
    null
);

$default =& $fixture->defaultCase();
$default = 'changed default';

$null =& $fixture->nullCase();
$null = 'changed null';

echo $fixture->defaultCase(), "\n";
echo $fixture->nullCase(), "\n";

var_dump((new ReflectionMethod(VoyagerMethodRefFixture::class, 'defaultCase'))->returnsReference());
var_dump((new ReflectionMethod(VoyagerMethodRefFixture::class, 'nullCase'))->returnsReference());
?>
--EXPECT--
changed default
changed null
bool(true)
bool(true)
