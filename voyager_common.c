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

#include "voyager.h"

/* {{{ */
void ensure_all_objects_of_class_have_magic_methods(zend_class_entry *ce)
{
    ce->ce_flags |= ZEND_ACC_USE_GUARDS;
}
/* }}} */

/* {{{ PHP_VOYAGER_ADD_MAGIC_METHOD */
void PHP_VOYAGER_ADD_MAGIC_METHOD(zend_class_entry *ce, zend_string *lcmname, zend_function *fe, const zend_function *orig_fe)
{
    if (zend_string_equals_literal(lcmname, ZEND_CLONE_FUNC_NAME)) {
        (ce)->clone = (fe);
    } else if (zend_string_equals_literal(lcmname, ZEND_CONSTRUCTOR_FUNC_NAME)) {
        if (!(ce)->constructor || (ce)->constructor == (orig_fe)) {
            (ce)->constructor = (fe);
        }
    } else if (zend_string_equals_literal(lcmname, ZEND_DESTRUCTOR_FUNC_NAME)) {
        (ce)->destructor = (fe);
    } else if (zend_string_equals_literal(lcmname, ZEND_GET_FUNC_NAME)) {
        (ce)->__get = (fe);
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if (zend_string_equals_literal(lcmname, ZEND_SET_FUNC_NAME)) {
        (ce)->__set = (fe);
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if (zend_string_equals_literal(lcmname, ZEND_CALL_FUNC_NAME)) {
        (ce)->__call = (fe);
    } else if (zend_string_equals_literal(lcmname, ZEND_UNSET_FUNC_NAME)) {
        (ce)->__unset = (fe);
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if (zend_string_equals_literal(lcmname, ZEND_ISSET_FUNC_NAME)) {
        (ce)->__isset = (fe);
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if (zend_string_equals_literal(lcmname, ZEND_CALLSTATIC_FUNC_NAME)) {
        (ce)->__callstatic = (fe);
    } else if (zend_string_equals_literal(lcmname, ZEND_TOSTRING_FUNC_NAME)) {
        (ce)->__tostring = (fe);
    } else if (zend_string_equals_literal(lcmname, ZEND_DEBUGINFO_FUNC_NAME)) {
        (ce)->__debugInfo = (fe);
    } else if (zend_string_equals_literal(lcmname, "__serialize")) {
        (ce)->__serialize = (fe);
    } else if (zend_string_equals_literal(lcmname, "__unserialize")) {
        (ce)->__unserialize = (fe);
    }
}
/* }}} */

/* {{{ PHP_VOYAGER_DEL_MAGIC_METHOD */
void PHP_VOYAGER_DEL_MAGIC_METHOD(zend_class_entry *ce, const zend_function *fe)
{
    if (ce->constructor == fe) {
        ce->constructor = NULL;
    } else if (ce->destructor == fe) {
        ce->destructor = NULL;
    } else if (ce->__get == fe) {
        ce->__get = NULL;
    } else if (ce->__set == fe) {
        ce->__set = NULL;
    } else if (ce->__unset == fe) {
        ce->__unset = NULL;
    } else if (ce->__isset == fe) {
        ce->__isset = NULL;
    } else if (ce->__call == fe) {
        ce->__call = NULL;
    } else if (ce->__callstatic == fe) {
        ce->__callstatic = NULL;
    } else if (ce->__tostring == fe) {
        ce->__tostring = NULL;
    } else if (ce->__debugInfo == fe) {
        ce->__debugInfo = NULL;
    } else if (ce->clone == fe) {
        ce->clone = NULL;
    } else if (ce->__serialize == fe) {
        ce->__serialize = NULL;
    } else if (ce->__unserialize == fe) {
        ce->__unserialize = NULL;
    }
}
/* }}} */
