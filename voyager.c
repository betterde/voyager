/*
+----------------------------------------------------------------------+
| Copyright (c) 2018-2026 George King                                  |
+----------------------------------------------------------------------+
| This source file is subject to the MIT license,
| that is bundled with this package in the file LICENSE, and is        |
| available through the world-wide-web at the following url:           |
| https://opensource.org/license/MIT
+----------------------------------------------------------------------+
| Author: George King <george@betterde.com>                            |
+----------------------------------------------------------------------+
*/

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_voyager.h"
#include "voyager.h"
#include "voyager_arginfo.h"

ZEND_DECLARE_MODULE_GLOBALS(voyager)

/* {{{ PHP_INI */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("voyager.internal_override", "0", PHP_INI_SYSTEM,
        OnUpdateBool, internal_override, zend_voyager_globals, voyager_globals)
PHP_INI_END()
/* }}} */

/* {{{ php_voyager_init_globals */
static void php_voyager_init_globals(void *voyager_globals)
{
    zend_voyager_globals *globals = (zend_voyager_globals *)voyager_globals;
    globals->internal_override = 0;
    globals->misplaced_internal_functions = NULL;
    globals->replaced_internal_functions = NULL;
    globals->module_moved_to_front = 0;
    globals->original_func_resource_handle = 0;
    globals->removed_function = NULL;
    globals->removed_method = NULL;
    globals->name_str = "name";
    globals->removed_function_str = "removed function";
    globals->removed_method_str = "removed method";
    globals->removed_parameter_str = "removed parameter";
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(voyager)
{
    ZEND_INIT_MODULE_GLOBALS(voyager, php_voyager_init_globals, NULL);
    REGISTER_INI_ENTRIES();

    // Create stub functions for reflection cleanup
    {
        zend_function *fe;
        fe = pemalloc(sizeof(zend_function), 1);
        memset(fe, 0, sizeof(zend_function));
        fe->type = ZEND_INTERNAL_FUNCTION;
        fe->common.function_name = zend_string_init("removed function", sizeof("removed function") - 1, 1);
        fe->internal_function.module = &voyager_module_entry;
        VOYAGER_G(removed_function) = fe;

        fe = pemalloc(sizeof(zend_function), 1);
        memset(fe, 0, sizeof(zend_function));
        fe->type = ZEND_INTERNAL_FUNCTION;
        fe->common.function_name = zend_string_init("removed method", sizeof("removed method") - 1, 1);
        fe->internal_function.module = &voyager_module_entry;
        VOYAGER_G(removed_method) = fe;
    }

    VOYAGER_G(original_func_resource_handle) = zend_register_list_destructors_ex(NULL, NULL, "original function", 0);

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(voyager)
{
    if (VOYAGER_G(removed_function)) {
        if (VOYAGER_G(removed_function)->common.function_name) {
            zend_string_release(VOYAGER_G(removed_function)->common.function_name);
        }
        pefree(VOYAGER_G(removed_function), 1);
    }
    if (VOYAGER_G(removed_method)) {
        if (VOYAGER_G(removed_method)->common.function_name) {
            zend_string_release(VOYAGER_G(removed_method)->common.function_name);
        }
        pefree(VOYAGER_G(removed_method), 1);
    }
    if (VOYAGER_G(misplaced_internal_functions)) {
        zend_hash_destroy(VOYAGER_G(misplaced_internal_functions));
        FREE_HASHTABLE(VOYAGER_G(misplaced_internal_functions));
    }
    if (VOYAGER_G(replaced_internal_functions)) {
        zend_hash_destroy(VOYAGER_G(replaced_internal_functions));
        FREE_HASHTABLE(VOYAGER_G(replaced_internal_functions));
    }
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(voyager)
{
#if defined(ZTS) && defined(COMPILE_DL_VOYAGER)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION */
PHP_RSHUTDOWN_FUNCTION(voyager)
{
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(voyager)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "voyager support", "enabled");
    php_info_print_table_row(2, "version", PHP_VOYAGER_VERSION);
    php_info_print_table_end();
}
/* }}} */

/* {{{ voyager_module_entry */
zend_module_entry voyager_module_entry = {
    STANDARD_MODULE_HEADER,
    "voyager",
    voyager_functions,
    PHP_MINIT(voyager),
    PHP_MSHUTDOWN(voyager),
    PHP_RINIT(voyager),
    PHP_RSHUTDOWN(voyager),
    PHP_MINFO(voyager),
    PHP_VOYAGER_VERSION,
    PHP_MODULE_GLOBALS(voyager),
    php_voyager_init_globals,
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_VOYAGER
ZEND_GET_MODULE(voyager)
#endif
