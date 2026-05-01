<?php

/**
 * Stub for the voyager PHP extension
 *
 * Allows redefining existing functions and methods at runtime.
 *
 * @link https://github.com/betterde/voyager
 * @since PHP 8.0
 */

/**
 * Redefine an existing user-defined function at runtime.
 *
 * @param string $function_name The function to redefine
 * @param Closure|string $closure_or_args Either a Closure or argument list string
 * @param string|null $code_or_doc_comment Code body (if $closure_or_args is string) or doc comment
 * @param bool|null $return_by_reference Whether the function should return by reference
 * @param string|null $doc_comment Doc comment for the function
 * @return bool True on success, false on failure
 *
 * @example
 * <code>
 * // Closure mode
 * voyager_function_redefine("greet", function($name) {
 *     return "Hi, $name!";
 * });
 *
 * // String body mode
 * voyager_function_redefine("greet", '$name', 'return "Hi, $name!";', false, "/** Greets someone */");
 * </code>
 */
function voyager_function_redefine(
    string $function_name,
    $closure_or_args,
    ?string $code_or_doc_comment = null,
    ?bool $return_by_reference = null,
    ?string $doc_comment = null
): bool {}

/**
 * Redefine an existing method on a user-defined class at runtime.
 *
 * Changes on parent methods are automatically propagated to child classes.
 *
 * @param string $class_name The class containing the method
 * @param string $method_name The method to redefine
 * @param Closure|string $closure_or_args Either a Closure or argument list string
 * @param int|string|null $code_or_flags Code body (if $closure_or_args is string) or visibility flags (ZEND_ACC_PUBLIC, etc.)
 * @param int|string|null $flags_or_doc_comment Visibility flags or doc comment
 * @param string|null $doc_comment Doc comment for the method
 * @return bool True on success, false on failure
 *
 * @example
 * <code>
 * // Closure mode with $this support
 * voyager_method_redefine("UserService", "findUser", function(int $id): array {
 *     return ['id' => $id, 'name' => 'mock'];
 * });
 *
 * // String body mode
 * voyager_method_redefine("MyClass", "process", '$input', 'return strtoupper($input);', ZEND_ACC_PUBLIC);
 * </code>
 */
function voyager_method_redefine(
    string $class_name,
    string $method_name,
    $closure_or_args,
    $code_or_flags = null,
    $flags_or_doc_comment = null,
    ?string $doc_comment = null
): bool {}
