--TEST--
voyager redefinition invalidates stale reflection objects
--SKIPIF--
<?php
if (!extension_loaded('voyager')) {
    die('skip voyager extension not loaded');
}
?>
--FILE--
<?php
function voyager_reflection_function_fixture(): string {
    return 'function original';
}

class VoyagerReflectionMethodFixture {
    public function value(): string {
        return 'method original';
    }
}

$staleFunction = new ReflectionFunction('voyager_reflection_function_fixture');
$staleMethod = new ReflectionMethod(VoyagerReflectionMethodFixture::class, 'value');

voyager_function_redefine('voyager_reflection_function_fixture', function(): string {
    return 'function redefined';
});

voyager_method_redefine(VoyagerReflectionMethodFixture::class, 'value', function(): string {
    return 'method redefined';
});

echo voyager_reflection_function_fixture(), "\n";
echo (new VoyagerReflectionMethodFixture())->value(), "\n";
echo $staleFunction->getName(), "\n";
echo $staleMethod->getName(), "\n";
echo (new ReflectionFunction('voyager_reflection_function_fixture'))->getName(), "\n";
echo (new ReflectionMethod(VoyagerReflectionMethodFixture::class, 'value'))->getName(), "\n";
?>
--EXPECT--
function redefined
method redefined
removed function
removed method
voyager_reflection_function_fixture
value
