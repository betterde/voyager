--TEST--
voyager_function_redefine preserves return-by-reference functions by default
--SKIPIF--
<?php
if (!extension_loaded('voyager')) {
    die('skip voyager extension not loaded');
}
?>
--FILE--
<?php
function &voyager_ref_default(): string {
    static $value = 'original default';
    return $value;
}

function &voyager_ref_null(): string {
    static $value = 'original null';
    return $value;
}

function &voyager_ref_false(): string {
    static $value = 'original false';
    return $value;
}

voyager_function_redefine('voyager_ref_default', '', 'static $value = "redefined default"; return $value;');
voyager_function_redefine('voyager_ref_null', '', 'static $value = "redefined null"; return $value;', null);
voyager_function_redefine('voyager_ref_false', '', 'static $value = "redefined false"; return $value;', false);

$default =& voyager_ref_default();
$default = 'changed default';

$null =& voyager_ref_null();
$null = 'changed null';

echo voyager_ref_default(), "\n";
echo voyager_ref_null(), "\n";
echo voyager_ref_false(), "\n";

var_dump((new ReflectionFunction('voyager_ref_default'))->returnsReference());
var_dump((new ReflectionFunction('voyager_ref_null'))->returnsReference());
var_dump((new ReflectionFunction('voyager_ref_false'))->returnsReference());
?>
--EXPECT--
changed default
changed null
redefined false
bool(true)
bool(true)
bool(false)
