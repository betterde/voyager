--TEST--
voyager_method_redefine preserves static methods by default
--SKIPIF--
<?php
if (!extension_loaded('voyager')) {
    die('skip voyager extension not loaded');
}
?>
--FILE--
<?php
class VoyagerStaticMethodFixture {
    public static function closureCase(): string {
        return 'original closure';
    }

    public static function stringCase(): string {
        return 'original string';
    }
}

voyager_method_redefine(VoyagerStaticMethodFixture::class, 'closureCase', function(): string {
    return 'redefined closure';
});

voyager_method_redefine(VoyagerStaticMethodFixture::class, 'stringCase', '', "return 'redefined string';");

echo VoyagerStaticMethodFixture::closureCase(), "\n";
echo VoyagerStaticMethodFixture::stringCase(), "\n";

$closureReflection = new ReflectionMethod(VoyagerStaticMethodFixture::class, 'closureCase');
$stringReflection = new ReflectionMethod(VoyagerStaticMethodFixture::class, 'stringCase');
var_dump($closureReflection->isStatic());
var_dump($stringReflection->isStatic());
?>
--EXPECT--
redefined closure
redefined string
bool(true)
bool(true)
