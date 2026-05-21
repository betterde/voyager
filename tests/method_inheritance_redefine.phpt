--TEST--
voyager_method_redefine propagates inherited parent methods without replacing child overrides
--SKIPIF--
<?php
if (!extension_loaded('voyager')) {
    die('skip voyager extension not loaded');
}
?>
--FILE--
<?php
class VoyagerParentMethodFixture {
    public function label(): string {
        return 'parent original';
    }
}

class VoyagerInheritedChildFixture extends VoyagerParentMethodFixture {
}

class VoyagerOverrideChildFixture extends VoyagerParentMethodFixture {
    public function label(): string {
        return 'child override';
    }
}

voyager_method_redefine(VoyagerParentMethodFixture::class, 'label', function(): string {
    return 'parent redefined';
});

echo (new VoyagerParentMethodFixture())->label(), "\n";
echo (new VoyagerInheritedChildFixture())->label(), "\n";
echo (new VoyagerOverrideChildFixture())->label(), "\n";

$parent = new ReflectionMethod(VoyagerParentMethodFixture::class, 'label');
$inherited = new ReflectionMethod(VoyagerInheritedChildFixture::class, 'label');
$override = new ReflectionMethod(VoyagerOverrideChildFixture::class, 'label');

echo $parent->getDeclaringClass()->getName(), "\n";
echo $inherited->getDeclaringClass()->getName(), "\n";
echo $override->getDeclaringClass()->getName(), "\n";
?>
--EXPECT--
parent redefined
parent redefined
child override
VoyagerParentMethodFixture
VoyagerParentMethodFixture
VoyagerOverrideChildFixture
