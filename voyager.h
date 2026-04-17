/*
+----------------------------------------------------------------------+
| Copyright (c) 2018-2026 George King                                  |
+----------------------------------------------------------------------+
| This source file is subject to the MIT license,
| that is bundled with this package in the file LICENSE, and is        |
| available through the world-wide-web at the following url:           |
| http://www.opensource.org/licenses/MIT
+----------------------------------------------------------------------+
| Author: George King <george@betterde.com>                            |
+----------------------------------------------------------------------+
*/

#ifndef VOYAGER_H
#define VOYAGER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"

#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "Zend/zend_closures.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_object_handlers.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __GNUC__
extern PHPAPI ZEND_COLD void php_error_docref(const char *docref, int type, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
#endif

#ifdef DEBUGGING
#define debug_printf(...) printf(__VA_ARGS__)
#else
#define debug_printf(...) do { } while (0)
#endif

#define PHP_VOYAGER_VERSION "8.2.0"

PHP_MINIT_FUNCTION(voyager);
PHP_MSHUTDOWN_FUNCTION(voyager);
PHP_RINIT_FUNCTION(voyager);
PHP_RSHUTDOWN_FUNCTION(voyager);
PHP_MINFO_FUNCTION(voyager);

PHP_FUNCTION(voyager_function_redefine);
PHP_FUNCTION(voyager_method_redefine);

/* Module globals */
ZEND_BEGIN_MODULE_GLOBALS(voyager)
    HashTable *misplaced_internal_functions;
    HashTable *replaced_internal_functions;
    zend_bool internal_override;
    const char *name_str, *removed_method_str, *removed_function_str, *removed_parameter_str;
    zend_function *removed_function, *removed_method;
    zend_bool module_moved_to_front;
    int original_func_resource_handle;
ZEND_END_MODULE_GLOBALS(voyager)

/* Extern declaration for non-ZTS access */
extern zend_voyager_globals voyager_globals;

/* zend_closure struct (internal to Zend, needed for closure manipulation) */
typedef struct _zend_closure {
    zend_object    std;
    zend_function  func;
} zend_closure;

/* Helper: find bucket in hash table (not exported by PHP 8.x) */
static inline Bucket *voyager_zend_hash_find_bucket(HashTable *ht, zend_string *key)
{
    zend_ulong h;
    uint32_t nIndex;
    uint32_t idx;
    Bucket *p, *arData;

    h = zend_string_hash_val(key);
    arData = ht->arData;
    nIndex = h | ht->nTableMask;
    idx = HT_HASH_EX(arData, nIndex);
    while (EXPECTED(idx != HT_INVALID_IDX)) {
        p = HT_HASH_TO_BUCKET_EX(arData, idx);
        if (EXPECTED(p->key == key)) {
            return p;
        } else if (EXPECTED(p->h == h) &&
                   EXPECTED(p->key) &&
                   EXPECTED(ZSTR_LEN(p->key) == ZSTR_LEN(key)) &&
                   EXPECTED(memcmp(ZSTR_VAL(p->key), ZSTR_VAL(key), ZSTR_LEN(key)) == 0)) {
            return p;
        }
        idx = Z_NEXT(p->val);
    }
    return NULL;
}

#ifdef ZTS
#define VOYAGER_G(v) TSRMG(voyager_globals_id, zend_voyager_globals *, v)
#else
#define VOYAGER_G(v) (voyager_globals.v)
#endif

#define VOYAGER_IS_CALLABLE(cb_zv, flags, cb_sp) zend_is_callable((cb_zv), (flags), (cb_sp))

#ifdef ZEND_ACC_RETURN_REFERENCE
#     define PHP_VOYAGER_ACC_RETURN_REFERENCE ZEND_ACC_RETURN_REFERENCE
#else
#     define PHP_VOYAGER_ACC_RETURN_REFERENCE 0x4000000
#endif

#ifndef ALLOC_PERMANENT_ZVAL
# define ALLOC_PERMANENT_ZVAL(z) \
    (z) = (zval*)malloc(sizeof(zval))
#endif

/* Helper for zend_hash_add_or_update_ptr */
static inline void *voyager_zend_hash_add_or_update_ptr(HashTable *ht, zend_string *key, void *pData, uint32_t flag)
{
    zval tmp, *zv;
    ZVAL_PTR(&tmp, pData);
    zv = zend_hash_add_or_update(ht, key, &tmp, flag);
    if (zv) {
        ZEND_ASSUME(Z_PTR_P(zv));
        return Z_PTR_P(zv);
    } else {
        return NULL;
    }
}

/* {{{ php_voyager_modify_function_doc_comment */
static inline void php_voyager_modify_function_doc_comment(zend_function *fe, zend_string *doc_comment)
{
    if (fe->type == ZEND_USER_FUNCTION) {
        if (doc_comment) {
            zend_string *tmp = fe->op_array.doc_comment;
            zend_string_addref(doc_comment);
            fe->op_array.doc_comment = doc_comment;
            if (tmp) {
                zend_string_delref(tmp);
            }
        }
    }
}
/* }}} */

#define PHP_VOYAGER_FREE_INTERNAL_FUNCTION_NAME(fe) \
    if ((fe)->type == ZEND_INTERNAL_FUNCTION && (fe)->internal_function.function_name) { \
        zend_string_release((fe)->internal_function.function_name); \
        (fe)->internal_function.function_name = NULL; \
    }

/* This macro iterates through all instances of objects. */
#define PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i) { \
    if (EG(objects_store).object_buckets) { \
        for (i = 1; i < EG(objects_store).top; i++) { \
            if (EG(objects_store).object_buckets[i] && \
               IS_OBJ_VALID(EG(objects_store).object_buckets[i]) && (!(GC_FLAGS(EG(objects_store).object_buckets[i]) & IS_OBJ_DESTRUCTOR_CALLED))) { \
                zend_object *object; \
                object = EG(objects_store).object_buckets[i];

#define PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_END \
            } \
        }\
    } \
}

/* Return type parsing */
typedef struct {
    zend_string *return_type;
    zend_bool   valid;
} parsed_return_type;

/* Strict mode parsing */
typedef struct {
    zend_bool   overridden;
    zend_bool   is_strict;
    zend_bool   valid;
} parsed_is_strict;

/* Reflection structs (must match ext/reflection/php_reflection.c) */
typedef struct _property_reference {
    zend_property_info *prop;
    zend_string *unmangled_name;
} property_reference;

typedef struct _parameter_reference {
    uint32_t offset;
    uint32_t required;
    struct _zend_arg_info *arg_info;
    zend_function *fptr;
} parameter_reference;

struct _zend_attribute;
typedef struct {
    HashTable *attributes;
    struct _zend_attribute *data;
    zend_class_entry *scope;
    zend_string *filename;
    uint32_t target;
} attribute_reference;

typedef struct _type_reference {
    zend_type type;
    bool legacy_behavior;
} type_reference;

typedef enum {
    REF_TYPE_OTHER,
    REF_TYPE_FUNCTION,
    REF_TYPE_GENERATOR,
    REF_TYPE_FIBER,
    REF_TYPE_PARAMETER,
    REF_TYPE_TYPE,
    REF_TYPE_PROPERTY,
    REF_TYPE_CLASS_CONSTANT,
    REF_TYPE_ATTRIBUTE,
    REF_TYPE_COUNT,
} reflection_type_t;

typedef struct {
    zval dummy;
    zval obj;
    void *ptr;
    zend_class_entry *ce;
    reflection_type_t ref_type;
    zend_object zo;
} reflection_object;

#define PHP_VOYAGER_NOT_ENOUGH_MEMORY_ERROR php_error_docref(NULL, E_ERROR, "Not enough memory")

/* voyager_functions.c */
#define VOYAGER_TEMP_FUNCNAME "__voyager_temporary_function__"
int php_voyager_check_call_stack(zend_op_array *op_array);
void php_voyager_clear_all_functions_runtime_cache(void);
void php_voyager_fix_all_hardcoded_stack_sizes(zend_string *called_name_lower, zend_function *called_f);
void php_voyager_remove_function_from_reflection_objects(zend_function *fe);
zend_function *php_voyager_function_clone(zend_function *fe, zend_string *newname, char orig_fe_type);
void php_voyager_function_dtor(zend_function *fe);
int php_voyager_generate_lambda_function(const zend_string *arguments, const zend_string *return_type, const zend_bool is_strict, const zend_string *phpcode, zend_function **pfe, zend_bool return_ref);
int php_voyager_generate_lambda_method(const zend_string *arguments, const zend_string *return_type, const zend_bool is_strict, const zend_string *phpcode, zend_function **pfe, zend_bool return_ref, zend_bool is_static);
int php_voyager_cleanup_lambda_function(void);
int php_voyager_cleanup_lambda_method(void);

/* voyager_methods.c */
zend_class_entry *php_voyager_fetch_class(zend_string *classname);
zend_class_entry *php_voyager_fetch_class_int(zend_string *classname);
void php_voyager_clean_children_methods(zend_class_entry *ce, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_string *fname_lower, zend_function *orig_cfe);
void php_voyager_clean_children_methods_foreach(HashTable *ht, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_string *fname_lower, zend_function *orig_cfe);
void php_voyager_update_children_methods(zend_class_entry *ce, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_function *fe, zend_string *fname_lower, zend_function *orig_fe);
void php_voyager_update_children_methods_foreach(HashTable *ht, zend_class_entry *ancestor_class, zend_class_entry *parent_class, zend_function *fe, zend_string *fname_lower, zend_function *orig_fe);

/* voyager_common.c */
void PHP_VOYAGER_ADD_MAGIC_METHOD(zend_class_entry *ce, zend_string *lcmname, zend_function *fe, const zend_function *orig_fe);
void PHP_VOYAGER_DEL_MAGIC_METHOD(zend_class_entry *ce, const zend_function *fe);
void ensure_all_objects_of_class_have_magic_methods(zend_class_entry *ce);

/* {{{ php_voyager_parse_doc_comment_arg */
inline static zend_string *php_voyager_parse_doc_comment_arg(int argc, zval *args, int arg_pos)
{
    if (argc > arg_pos) {
        if (Z_TYPE(args[arg_pos]) == IS_STRING) {
            return Z_STR(args[arg_pos]);
        } else if (Z_TYPE(args[arg_pos]) != IS_NULL) {
            php_error_docref(NULL, E_WARNING, "Doc comment should be a string or NULL");
        }
    }
    return NULL;
}
/* }}} */

/* {{{ php_voyager_is_valid_return_type */
inline static zend_bool php_voyager_is_valid_return_type(const zend_string *return_type)
{
    const char *it = ZSTR_VAL(return_type);
    const char *const end = it + ZSTR_LEN(return_type);
    if (it >= end) {
        return 0;
    }
    if (*it == '?') {
        it++;
    }
    if (it >= end) {
        return 0;
    }
    if (*it == '\\') {
        it++;
    }
    if (it >= end) {
        return 0;
    }
    while (1) {
        unsigned char c = *it;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128) {
            for (++it; it < end; ++it) {
                c = *it;
                if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 128) {
                    continue;
                }
                if (c == '\\') {
                    break;
                }
                return 0;
            }
            if (c == '\\') {
                if (it + 1 == end) {
                    return 0;
                }
                ++it;
                continue;
            }
            return 1;
        }
        return 0;
    }
}
/* }}} */

/* {{{ php_voyager_parse_return_type_arg */
inline static parsed_return_type php_voyager_parse_return_type_arg(int argc, zval *args, int arg_pos)
{
    parsed_return_type retval;
    retval.return_type = NULL;
    retval.valid = 1;
    if (argc <= arg_pos) {
        return retval;
    }
    if (Z_TYPE(args[arg_pos]) == IS_STRING) {
        zend_string *return_type = Z_STR(args[arg_pos]);
        if (ZSTR_LEN(return_type) == 0) {
            return retval;
        }
        if (php_voyager_is_valid_return_type(return_type)) {
            retval.return_type = return_type;
            return retval;
        }
        php_error_docref(NULL, E_WARNING, "Invalid return type");
        retval.valid = 0;
        return retval;
    } else if (Z_TYPE(args[arg_pos]) != IS_NULL) {
        php_error_docref(NULL, E_WARNING, "Return type should be a string or NULL");
        retval.valid = 0;
    }
    return retval;
}
/* }}} */

/* {{{ php_voyager_parse_is_strict_arg */
inline static parsed_is_strict php_voyager_parse_is_strict_arg(int argc, zval *args, int arg_pos)
{
    parsed_is_strict retval;
    retval.is_strict = 0;
    retval.overridden = 0;
    retval.valid = 1;
    if (argc <= arg_pos) {
        return retval;
    }
    if (Z_TYPE(args[arg_pos]) == IS_TRUE || Z_TYPE(args[arg_pos]) == IS_FALSE) {
        retval.is_strict = Z_TYPE(args[arg_pos]) == IS_TRUE;
        retval.overridden = 1;
        return retval;
    } else if (Z_TYPE(args[arg_pos]) != IS_NULL) {
        php_error_docref(NULL, E_WARNING, "is_strict should be a boolean or NULL");
        retval.valid = 0;
    }
    return retval;
}
/* }}} */

/* {{{ php_voyager_parse_args_to_zvals */
inline static zend_bool php_voyager_parse_args_to_zvals(int argc, zval **pargs)
{
    *pargs = (zval *)emalloc(argc * sizeof(zval));
    if (*pargs == NULL) {
        PHP_VOYAGER_NOT_ENOUGH_MEMORY_ERROR;
        return 0;
    }
    if (zend_get_parameters_array_ex(argc, *pargs) == FAILURE) {
        php_error_docref(NULL, E_ERROR, "Internal error occurred while parsing arguments");
        efree(*pargs);
        return 0;
    }
    return 1;
}
/* }}} */

#define PHP_VOYAGER_BODY_ERROR_MSG "%s's body should be either a closure or two strings"

/* {{{ php_voyager_parse_function_arg */
inline static zend_bool php_voyager_parse_function_arg(int argc, zval *args, int arg_pos, zend_function **fe, zend_string **arguments, zend_string **phpcode, long *opt_arg_pos, char *type)
{
    if (Z_TYPE(args[arg_pos]) == IS_OBJECT && Z_OBJCE(args[arg_pos]) == zend_ce_closure) {
        *fe = (zend_function *)zend_get_closure_method_def(Z_OBJ(args[arg_pos]));
    } else if (Z_TYPE(args[arg_pos]) == IS_STRING) {
        (*opt_arg_pos)++;
        *arguments = Z_STR(args[arg_pos]);
        if (argc < arg_pos + 2 || Z_TYPE(args[arg_pos + 1]) != IS_STRING) {
            php_error_docref(NULL, E_ERROR, PHP_VOYAGER_BODY_ERROR_MSG, type);
            return 0;
        }
        *phpcode = Z_STR(args[arg_pos + 1]);
    } else {
        php_error_docref(NULL, E_ERROR, PHP_VOYAGER_BODY_ERROR_MSG, type);
        return 0;
    }
    return 1;
}
/* }}} */

void php_voyager_update_reflection_object_name(zend_object *object, int handle, const char *name);

#endif /* VOYAGER_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
