<?php

/** @generate-class-entries */

/**
 * Redefine an existing user-defined function at runtime.
 *
 * @param string $function_name The function to redefine
 * @param Closure|string $closure_or_args Either a Closure or argument list string
 * @param string|null $code_or_doc_comment Code body (if $closure_or_args is string) or doc comment
 * @param bool|null $return_by_reference Whether the function should return by reference
 * @param string|null $doc_comment Doc comment for the function
 * @return bool
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
 * @param string $class_name The class containing the method
 * @param string $method_name The method to redefine
 * @param Closure|string $closure_or_args Either a Closure or argument list string
 * @param int|string|null $code_or_flags Code body (if closure_or_args is string) or visibility flags
 * @param int|string|null $flags_or_doc_comment Visibility flags or doc comment
 * @param string|null $doc_comment Doc comment for the method
 * @return bool
 */
function voyager_method_redefine(
    string $class_name,
    string $method_name,
    $closure_or_args,
    $code_or_flags = null,
    $flags_or_doc_comment = null,
    ?string $doc_comment = null
): bool {}
