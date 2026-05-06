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
#include "Zend/zend_compile.h"

/* {{{ _php_voyager_get_method_prototype
 */
static inline zend_function *_php_voyager_get_method_prototype(zend_class_entry *ce, zend_string *func_lower)
{
    zend_class_entry *pce = ce;
    zend_function *proto = NULL;

    while (pce) {
        if ((proto = zend_hash_find_ptr(&pce->function_table, func_lower)) != NULL) {
            return proto;
        }
        pce = pce->parent;
    }
    return NULL;
}
/* }}} */

/* {{{ php_voyager_fetch_class_int
 */
zend_class_entry *php_voyager_fetch_class_int(zend_string *classname)
{
    return zend_lookup_class_ex(classname, (zend_string *)NULL, (int)0);
}
/* }}} */

/* {{{ php_voyager_fetch_class
 */
zend_class_entry *php_voyager_fetch_class(zend_string *classname)
{
    zend_class_entry *ce;

    if ((ce = php_voyager_fetch_class_int(classname)) == NULL) {
        return NULL;
    }

    if (ce->type != ZEND_USER_CLASS) {
        php_error_docref(NULL, E_WARNING, "class %s is not a user-defined class", ZSTR_VAL(classname));
        return NULL;
    }

    if (ce->ce_flags & ZEND_ACC_INTERFACE) {
        php_error_docref(NULL, E_WARNING, "class %s is an interface", ZSTR_VAL(classname));
        return NULL;
    }

    return ce;
}
/* }}} */

/* {{{ php_voyager_fetch_class_method
 */
static int php_voyager_fetch_class_method(zend_string *classname, zend_string *fname, zend_class_entry **pce, zend_function **pfe)
{
    zend_class_entry *ce;
    zend_function *fe;
    zend_string *fname_lower;

    if ((ce = php_voyager_fetch_class_int(classname)) == NULL) {
        return FAILURE;
    }

    if (ce->type != ZEND_USER_CLASS) {
        php_error_docref(NULL, E_WARNING, "class %s is not a user-defined class", ZSTR_VAL(classname));
        return FAILURE;
    }

    if (pce) {
        *pce = ce;
    }

    fname_lower = zend_string_tolower(fname);

    if ((fe = zend_hash_find_ptr(&ce->function_table, fname_lower)) == NULL) {
        php_error_docref(NULL, E_WARNING, "%s::%s() not found", ZSTR_VAL(classname), ZSTR_VAL(fname));
        zend_string_release(fname_lower);
        return FAILURE;
    }

    zend_string_release(fname_lower);
    if (fe->type != ZEND_USER_FUNCTION) {
        php_error_docref(NULL, E_WARNING, "%s::%s() is not a user function", ZSTR_VAL(classname), ZSTR_VAL(fname));
        return FAILURE;
    }

    if (pfe) {
        *pfe = fe;
    }

    return SUCCESS;
}
/* }}} */

/* {{{ php_voyager_update_children_methods_foreach */
void php_voyager_update_children_methods_foreach(HashTable *ht, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_function *fe, zend_string *fname_lower, zend_function *orig_fe)
{
    zend_class_entry *ce;
    ZEND_HASH_FOREACH_PTR(ht, ce) {
        php_voyager_update_children_methods(ce, ancestor_class, parent_class, fe, fname_lower, orig_fe);
    } ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ php_voyager_inherit_magic */
inline static void php_voyager_inherit_magic(zend_class_entry *ce, const zend_function *fe, const zend_function *orig_fe)
{
    if ((ce)->__get == (orig_fe) && (ce)->parent && (ce)->parent->__get == (fe)) {
        (ce)->__get = (ce)->parent->__get;
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if ((ce)->__set == (orig_fe) && (ce)->parent && (ce)->parent->__set == (fe)) {
        (ce)->__set = (ce)->parent->__set;
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if ((ce)->__unset == (orig_fe) && (ce)->parent && (ce)->parent->__unset == (fe)) {
        (ce)->__unset = (ce)->parent->__unset;
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if ((ce)->__isset == (orig_fe) && (ce)->parent && (ce)->parent->__isset == (fe)) {
        (ce)->__isset = (ce)->parent->__isset;
        ensure_all_objects_of_class_have_magic_methods(ce);
    } else if ((ce)->__call == (orig_fe) && (ce)->parent && (ce)->parent->__call == (fe)) {
        (ce)->__call = (ce)->parent->__call;
    } else if ((ce)->__callstatic == (orig_fe) && (ce)->parent && (ce)->parent->__callstatic == (fe)) {
        (ce)->__callstatic = (ce)->parent->__callstatic;
    } else if ((ce)->__tostring == (orig_fe) && (ce)->parent && (ce)->parent->__tostring == (fe)) {
        (ce)->__tostring = (ce)->parent->__tostring;
    } else if ((ce)->clone == (orig_fe) && (ce)->parent && (ce)->parent->clone == (fe)) {
        (ce)->clone = (ce)->parent->clone;
    } else if ((ce)->destructor == (orig_fe) && (ce)->parent && (ce)->parent->destructor == (fe)) {
        (ce)->destructor = (ce)->parent->destructor;
    } else if ((ce)->constructor == (orig_fe) && (ce)->parent && (ce)->parent->constructor == (fe)) {
        (ce)->constructor = (ce)->parent->constructor;
    } else if ((ce)->__debugInfo == (orig_fe) && (ce)->parent && (ce)->parent->__debugInfo == (fe)) {
        (ce)->__debugInfo = (ce)->parent->__debugInfo;
    } else if ((ce)->__serialize == (orig_fe) && (ce)->parent && (ce)->parent->__serialize == (fe)) {
        (ce)->__serialize = (ce)->parent->__serialize;
    } else if ((ce)->__unserialize == (orig_fe) && (ce)->parent && (ce)->parent->__unserialize == (fe)) {
        (ce)->__unserialize = (ce)->parent->__unserialize;
    }
}
/* }}} */

/* {{{ php_voyager_update_children_methods
 */
void php_voyager_update_children_methods(zend_class_entry *ce, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_function *fe, zend_string *fname_lower, zend_function *orig_fe)
{
    zend_function *cfe = NULL;

    /* Skip internal classes and interfaces */
    if (ce->type != ZEND_USER_CLASS) {
        return;
    }
    if (ce->ce_flags & ZEND_ACC_INTERFACE) {
        return;
    }
    /* Must have a valid parent that matches */
    if (!ce->parent || ce->parent != parent_class) {
        return;
    }

    if ((cfe = zend_hash_find_ptr(&ce->function_table, fname_lower)) != NULL) {
        if (cfe->common.scope != ancestor_class) {
            cfe->common.prototype = _php_voyager_get_method_prototype(cfe->common.scope->parent, fname_lower);
            php_voyager_update_children_methods_foreach(EG(class_table),
                        ancestor_class, ce, fe, fname_lower, orig_fe);
            return;
        }
    }

    if (cfe) {
        php_voyager_remove_function_from_reflection_objects(cfe);
        if (zend_hash_del(&ce->function_table, fname_lower) == FAILURE) {
            php_error_docref(NULL, E_WARNING, "Error updating child class");
            return;
        }
    }

    if (zend_hash_add_ptr(&ce->function_table, fname_lower, fe) == NULL) {
        php_error_docref(NULL, E_WARNING, "Error updating child class");
        return;
    }
    function_add_ref(fe);
    php_voyager_inherit_magic(ce, fe, orig_fe);

    php_voyager_update_children_methods_foreach(EG(class_table),
                       ancestor_class, ce, fe, fname_lower, orig_fe);
}
/* }}} */

/* {{{ php_voyager_clean_children_methods_foreach */
void php_voyager_clean_children_methods_foreach(HashTable *ht, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_string *fname_lower, zend_function *orig_cfe)
{
    zend_class_entry *ce;
    ZEND_HASH_FOREACH_PTR(ht, ce) {
        php_voyager_clean_children_methods(ce, ancestor_class, parent_class, fname_lower, orig_cfe);
    } ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ php_voyager_clean_children_methods
 */
void php_voyager_clean_children_methods(zend_class_entry *ce, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_string *fname_lower, zend_function *orig_cfe)
{
    zend_function *cfe = NULL;

    /* Skip internal classes and interfaces */
    if (ce->type != ZEND_USER_CLASS) {
        return;
    }
    if (ce->ce_flags & ZEND_ACC_INTERFACE) {
        return;
    }
    /* Must have a valid parent that matches */
    if (!ce->parent || ce->parent != parent_class) {
        return;
    }

    if ((cfe = zend_hash_find_ptr(&ce->function_table, fname_lower)) != NULL) {
        if (cfe->common.scope != ancestor_class) {
            return;
        }
    }

    if (!cfe) {
        return;
    }

    php_voyager_clean_children_methods_foreach(EG(class_table), ancestor_class, ce, fname_lower, orig_cfe);

    php_voyager_remove_function_from_reflection_objects(cfe);

    zend_hash_del(&ce->function_table, fname_lower);

    PHP_VOYAGER_DEL_MAGIC_METHOD(ce, orig_cfe);
}
/* }}} */

/* {{{ php_voyager_method_add_or_update
 */
static void php_voyager_method_add_or_update(INTERNAL_FUNCTION_PARAMETERS, int add_or_update)
{
    zend_string *classname = NULL, *methodname = NULL, *arguments = NULL, *phpcode = NULL, *doc_comment = NULL;
    zend_class_entry *ce, *ancestor_class = NULL;
    zend_function *func, *fe, *source_fe = NULL, *orig_fe = NULL;
    zend_string *methodname_lower;
    long argc = ZEND_NUM_ARGS();
    long flags = 0;
    zval *args;
    long opt_arg_pos = 3;
    zend_bool remove_temp = 0;
    zend_bool flags_overridden = 0;

    if (argc < 2 || zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, 2, "SS", &classname, &methodname) == FAILURE ||
            !ZSTR_LEN(classname) || !ZSTR_LEN(methodname)) {
        php_error_docref(NULL, E_ERROR, "Class name and method name should not be empty");
        RETURN_FALSE;
    }

    if (argc < 3) {
        php_error_docref(NULL, E_ERROR, "Method body should be provided");
        RETURN_FALSE;
    }

    if (!php_voyager_parse_args_to_zvals(argc, &args)) {
        RETURN_FALSE;
    }

    if (!php_voyager_parse_function_arg(argc, args, 2, &source_fe, &arguments, &phpcode, &opt_arg_pos, "Method")) {
        efree(args);
        RETURN_FALSE;
    }

    if (argc > opt_arg_pos) {
        if (Z_TYPE(args[opt_arg_pos]) == IS_NULL) {
            /* Keep the original method flags when redefining unless flags are explicitly provided. */
        } else if (Z_TYPE(args[opt_arg_pos]) == IS_LONG) {
            convert_to_long_ex(&(args[opt_arg_pos]));
            flags = Z_LVAL(args[opt_arg_pos]);
            flags_overridden = 1;
            if (flags & PHP_VOYAGER_ACC_RETURN_REFERENCE && source_fe) {
                php_error_docref(NULL, E_WARNING, "VOYAGER_ACC_RETURN_REFERENCE flag is not applicable for closures");
            }
        } else if (Z_TYPE(args[opt_arg_pos]) == IS_STRING) {
            /* Could be code or doc_comment from string body */
        } else {
            php_error_docref(NULL, E_WARNING, "Flags should be a long integer or NULL");
        }
    }

    doc_comment = php_voyager_parse_doc_comment_arg(argc, args, opt_arg_pos + 1);

    efree(args);

    methodname_lower = zend_string_tolower(methodname);

    if (add_or_update == HASH_UPDATE) {
        if (php_voyager_fetch_class_method(classname, methodname, &ce, &fe) == FAILURE) {
            zend_string_release(methodname_lower);
            RETURN_FALSE;
        }
        ancestor_class = fe->common.scope;
        orig_fe = fe;

        if (php_voyager_check_call_stack(&fe->op_array) == FAILURE) {
            php_error_docref(NULL, E_WARNING, "Cannot redefine a method while that method is active.");
            zend_string_release(methodname_lower);
            RETURN_FALSE;
        }
    } else {
        if ((ce = php_voyager_fetch_class(classname)) == NULL) {
            zend_string_release(methodname_lower);
            RETURN_FALSE;
        }
        ancestor_class = ce;
        if ((fe = zend_hash_find_ptr(&ce->function_table, methodname_lower)) != NULL) {
            if (fe->common.scope == ce) {
                php_error_docref(NULL, E_WARNING, "%s::%s() already exists", ZSTR_VAL(classname), ZSTR_VAL(methodname));
                zend_string_release(methodname_lower);
                RETURN_FALSE;
            } else {
                php_voyager_remove_function_from_reflection_objects(fe);
                zend_hash_del(&ce->function_table, methodname_lower);
            }
        }
    }

    if (!flags_overridden) {
        if (orig_fe) {
            flags = orig_fe->common.fn_flags & (ZEND_ACC_PPP_MASK | ZEND_ACC_STATIC | PHP_VOYAGER_ACC_RETURN_REFERENCE);
        } else {
            flags = ZEND_ACC_PUBLIC;
        }
    }

    if (!source_fe) {
        if (php_voyager_generate_lambda_method(arguments, NULL, 0, phpcode, &source_fe,
                             (flags & PHP_VOYAGER_ACC_RETURN_REFERENCE) == PHP_VOYAGER_ACC_RETURN_REFERENCE,
                             ((flags & ZEND_ACC_STATIC) != 0)) == FAILURE) {
            zend_string_release(methodname_lower);
            RETURN_FALSE;
        }
        remove_temp = 1;
    }

    func = php_voyager_function_clone(source_fe, methodname, (orig_fe ? orig_fe->type : ZEND_USER_FUNCTION));

    if (flags & ZEND_ACC_PRIVATE) {
        func->common.fn_flags &= ~ZEND_ACC_PPP_MASK;
        func->common.fn_flags |= ZEND_ACC_PRIVATE;
    } else if (flags & ZEND_ACC_PROTECTED) {
        func->common.fn_flags &= ~ZEND_ACC_PPP_MASK;
        func->common.fn_flags |= ZEND_ACC_PROTECTED;
    } else {
        func->common.fn_flags &= ~ZEND_ACC_PPP_MASK;
        func->common.fn_flags |= ZEND_ACC_PUBLIC;
    }
    func->common.fn_flags &= ~ZEND_ACC_CLOSURE;
    func->common.fn_flags &= ~ZEND_ACC_STATIC;

    if (flags & ZEND_ACC_STATIC) {
        func->common.fn_flags |= ZEND_ACC_STATIC;
    }

    if (doc_comment == NULL && source_fe->op_array.doc_comment == NULL &&
       orig_fe && orig_fe->type == ZEND_USER_FUNCTION && orig_fe->op_array.doc_comment) {
        doc_comment = orig_fe->op_array.doc_comment;
    }
    php_voyager_modify_function_doc_comment(func, doc_comment);

    php_voyager_clear_all_functions_runtime_cache();

    if (orig_fe) {
        php_voyager_remove_function_from_reflection_objects(orig_fe);
    }

    /* Save original for request-scoped restoration (only on first redefine) */
    if (orig_fe && add_or_update == HASH_UPDATE) {
        zend_string *key = zend_strpprintf(0, "%p::%s", (void *)ce, ZSTR_VAL(methodname_lower));
        if (!zend_hash_exists(VOYAGER_G(request_method_restores), key)) {
            voyager_method_restore *restore = emalloc(sizeof(voyager_method_restore));
            restore->ce = ce;
            restore->methodname_lower = zend_string_copy(methodname_lower);
            restore->orig_fe = orig_fe;
            zend_hash_add_ptr(VOYAGER_G(request_method_restores), key, restore);
        }
        zend_string_release(key);
    }

    /* Save orig_fe data before the hash table may destroy the old entry */
    zend_function *orig_fe_saved = NULL;
    if (orig_fe) {
        orig_fe_saved = emalloc(sizeof(zend_function));
        memcpy(orig_fe_saved, orig_fe, sizeof(zend_function));
        /* Bump refcount on the function name so it survives the destructor */
        if (orig_fe->common.function_name) {
            zend_string_addref(orig_fe->common.function_name);
        }
    }

    /* Temporarily disable the destructor to prevent freeing the old function */
    dtor_func_t orig_dtor = ce->function_table.pDestructor;
    ce->function_table.pDestructor = NULL;
    if (voyager_zend_hash_add_or_update_ptr(&ce->function_table, methodname_lower, func, add_or_update) == NULL) {
        php_error_docref(NULL, E_WARNING, "Unable to add method to class");
        php_voyager_function_dtor(func);
        zend_string_release(methodname_lower);
        if (remove_temp) {
            php_voyager_cleanup_lambda_method();
        }
        ce->function_table.pDestructor = orig_dtor;
        if (orig_fe_saved) {
            if (orig_fe_saved->common.function_name) {
                zend_string_delref(orig_fe_saved->common.function_name);
            }
            efree(orig_fe_saved);
        }
        RETURN_FALSE;
    }
    ce->function_table.pDestructor = orig_dtor;

    if (remove_temp && php_voyager_cleanup_lambda_method() == FAILURE) {
        zend_string_release(methodname_lower);
        if (orig_fe_saved) {
            if (orig_fe_saved->common.function_name) {
                zend_string_delref(orig_fe_saved->common.function_name);
            }
            efree(orig_fe_saved);
        }
        RETURN_FALSE;
    }

    if ((fe = zend_hash_find_ptr(&ce->function_table, methodname_lower)) == NULL) {
        php_error_docref(NULL, E_WARNING, "Unable to locate newly added method");
        zend_string_release(methodname_lower);
        if (orig_fe_saved) {
            if (orig_fe_saved->common.function_name) {
                zend_string_delref(orig_fe_saved->common.function_name);
            }
            efree(orig_fe_saved);
        }
        RETURN_FALSE;
    }

    fe->common.scope = ce;
    fe->common.prototype = _php_voyager_get_method_prototype(ce->parent, methodname_lower);

    /* Update magic method pointers and propagate to child classes */
    PHP_VOYAGER_ADD_MAGIC_METHOD(ce, methodname_lower, fe, orig_fe_saved);
    php_voyager_update_children_methods_foreach(EG(class_table), ancestor_class, ce, fe, methodname_lower, orig_fe_saved);

    /* Clean up saved orig_fe */
    if (orig_fe_saved) {
        if (orig_fe_saved->common.function_name) {
            zend_string_delref(orig_fe_saved->common.function_name);
        }
        efree(orig_fe_saved);
    }

    zend_string_release(methodname_lower);

    RETURN_TRUE;
}
/* }}} */

/* *****************
   * Methods API   *
   ***************** */

/* {{{ proto bool voyager_method_redefine(string classname, string methodname, string args, string code[, long flags[, string doc_comment]])
       proto bool voyager_method_redefine(string classname, string methodname, closure code[, long flags[, string doc_comment]])
 */
PHP_FUNCTION(voyager_method_redefine)
{
    php_voyager_method_add_or_update(INTERNAL_FUNCTION_PARAM_PASSTHRU, HASH_UPDATE);
}
/* }}} */

/* {{{ php_voyager_request_method_restore_dtor */
void php_voyager_request_method_restore_dtor(zval *zv)
{
    voyager_method_restore *restore = (voyager_method_restore *)Z_PTR_P(zv);
    if (!restore) {
        return;
    }
    if (restore->methodname_lower) {
        zend_string_release(restore->methodname_lower);
    }
    efree(restore);
}
/* }}} */

/* {{{ php_voyager_restore_methods
       Restores all methods redefined during the current request to their original implementations.
       Called from RSHUTDOWN to ensure request-scoped redefinitions don't leak into subsequent requests.
 */
void php_voyager_restore_methods(void)
{
    HashTable *restores = VOYAGER_G(request_method_restores);
    if (!restores || zend_hash_num_elements(restores) == 0) {
        return;
    }

    voyager_method_restore *restore;
    ZEND_HASH_FOREACH_PTR(restores, restore) {
        zend_class_entry *ce = restore->ce;
        zend_string *methodname_lower = restore->methodname_lower;
        zend_function *orig_fe = restore->orig_fe;

        if (!orig_fe) {
            continue;
        }

        /* Get current (redefined) function */
        zend_function *redefined_fe = zend_hash_find_ptr(&ce->function_table, methodname_lower);

        if (redefined_fe) {
            php_voyager_remove_function_from_reflection_objects(redefined_fe);
        }

        /* Fix magic method pointers on the parent class */
        if (redefined_fe) {
            PHP_VOYAGER_DEL_MAGIC_METHOD(ce, redefined_fe);
        }
        PHP_VOYAGER_ADD_MAGIC_METHOD(ce, methodname_lower, orig_fe, NULL);

        /* Disable destructor to prevent freeing orig_fe during hash update */
        dtor_func_t dtor = ce->function_table.pDestructor;
        ce->function_table.pDestructor = NULL;

        /* Replace with original */
        voyager_zend_hash_add_or_update_ptr(&ce->function_table, methodname_lower, orig_fe, HASH_UPDATE);

        /* Re-enable destructor */
        ce->function_table.pDestructor = dtor;

        /* Propagate restoration to child classes */
        php_voyager_update_children_methods_foreach(EG(class_table),
            orig_fe->common.scope, ce, orig_fe, methodname_lower, redefined_fe);

        /* Mark as restored so the hash table destructor won't free orig_fe */
        restore->orig_fe = NULL;
    } ZEND_HASH_FOREACH_END();

    php_voyager_clear_all_functions_runtime_cache();
}
/* }}} */
