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
#include "Zend/zend.h"

#ifdef ZEND_MAP_PTR_GET
#define VOYAGER_RUN_TIME_CACHE(op_array) \
       ZEND_MAP_PTR_GET((op_array)->run_time_cache)
#else
#define VOYAGER_RUN_TIME_CACHE(op_array) \
       ((op_array)->run_time_cache)
#endif

#define VOYAGER_TEMP_FUNCNAME  "__voyager_temporary_function__"
#define VOYAGER_TEMP_CLASSNAME "__voyager_temporary_class__"
#define VOYAGER_TEMP_METHODNAME "__voyager_temporary_method__"

#define PHP_VOYAGER_FETCH_FUNCTION_INSPECT  0
#define PHP_VOYAGER_FETCH_FUNCTION_REMOVE   1
#define PHP_VOYAGER_FETCH_FUNCTION_RENAME   2

/* {{{ php_voyager_check_call_stack
 */
int php_voyager_check_call_stack(zend_op_array *op_array)
{
    zend_execute_data *ptr;

    ptr = EG(current_execute_data);

    while (ptr) {
        if (ptr->func && ptr->func->op_array.opcodes == op_array->opcodes) {
            return FAILURE;
        }
        ptr = ptr->prev_execute_data;
    }

    return SUCCESS;
}
/* }}} */

/* {{{ php_voyager_fetch_function
 */
static zend_function *php_voyager_fetch_function(zend_string *fname, int flag)
{
    zend_function *fe;
    zend_string *fname_lower;

    fname_lower = zend_string_tolower(fname);

    if ((fe = zend_hash_find_ptr(EG(function_table), fname_lower)) == NULL) {
        zend_string_release(fname_lower);
        php_error_docref(NULL, E_WARNING, "%s() not found", ZSTR_VAL(fname));
        return NULL;
    }

    if (fe->type == ZEND_INTERNAL_FUNCTION &&
        !VOYAGER_G(internal_override)) {
        zend_string_release(fname_lower);
        php_error_docref(NULL, E_WARNING, "%s() is an internal function and voyager.internal_override is disabled", ZSTR_VAL(fname));
        return NULL;
    }

    if (fe->type != ZEND_USER_FUNCTION &&
        fe->type != ZEND_INTERNAL_FUNCTION) {
        zend_string_release(fname_lower);
        php_error_docref(NULL, E_WARNING, "%s() is not a user or normal internal function", ZSTR_VAL(fname));
        return NULL;
    }

    if (fe->type == ZEND_INTERNAL_FUNCTION &&
        flag >= PHP_VOYAGER_FETCH_FUNCTION_REMOVE) {

        if (!VOYAGER_G(replaced_internal_functions)) {
            ALLOC_HASHTABLE(VOYAGER_G(replaced_internal_functions));
            zend_hash_init(VOYAGER_G(replaced_internal_functions), 4, NULL, NULL, 0);
        }

        if (!zend_hash_exists(VOYAGER_G(replaced_internal_functions), fname_lower)) {
            Bucket *b;
            zend_function *fe_copy;
            zend_string_addref(fe->common.function_name);
            fe_copy = php_voyager_function_clone(fe, fe->common.function_name, ZEND_INTERNAL_FUNCTION);
            b = voyager_zend_hash_find_bucket(EG(function_table), fname_lower);
            if (b != NULL && b->key != NULL) {
                zend_string_addref(b->key);
                zend_string_release(fname_lower);
                fname_lower = b->key;
            } else {
                zend_string_addref(fname_lower);
            }
            zend_hash_add_ptr(VOYAGER_G(replaced_internal_functions), fname_lower, fe_copy);
        }
    }
    zend_string_release(fname_lower);

    return fe;
}
/* }}} */

/* {{{ php_voyager_arginfo_type_addref
 */
static void php_voyager_arginfo_type_addref(zend_arg_info *arginfo)
{
    zend_type type = arginfo->type;
    if (ZEND_TYPE_HAS_LIST(type)) {
        zend_type *atomic_type;
        zend_type_list *type_list = ZEND_TYPE_LIST(type);
        size_t size_in_bytes = ZEND_TYPE_LIST_SIZE(type_list->num_types);
        zend_type_list *new_type_list = emalloc(size_in_bytes);
        memcpy(new_type_list, type_list, size_in_bytes);
        arginfo->type.ptr = new_type_list;

        ZEND_TYPE_LIST_FOREACH(type_list, atomic_type) {
            if (ZEND_TYPE_HAS_NAME(*atomic_type)) {
                zend_string_addref(ZEND_TYPE_NAME(*atomic_type));
            }
        } ZEND_TYPE_LIST_FOREACH_END();
    } else if (ZEND_TYPE_HAS_NAME(type)) {
        zend_string_addref(ZEND_TYPE_NAME(type));
    }
}
/* }}} */

/* {{{ runkit_allocate_opcode_copy
 */
static zend_op *voyager_allocate_opcode_copy(const zend_op_array *const op_array)
{
    if (!(ZEND_USE_ABS_CONST_ADDR) &&
            (op_array->fn_flags & ZEND_ACC_DONE_PASS_TWO) &&
            op_array->literals) {
        size_t bytes_to_allocate = ZEND_MM_ALIGNED_SIZE_EX(sizeof(zend_op) * op_array->last, 16) +
            sizeof(zval) * op_array->last_literal;
        zend_op *opcode = (zend_op *)emalloc(bytes_to_allocate);
        memset(opcode, 0, bytes_to_allocate);
        return opcode;
    }
    return safe_emalloc(sizeof(zend_op), op_array->last, 0);
}
/* }}} */

/* {{{ runkit_allocate_literals */
static zval *voyager_allocate_literals(const zend_op_array *const op_array, zend_op *opcode_copy)
{
    if (!(ZEND_USE_ABS_CONST_ADDR) &&
            (op_array->fn_flags & ZEND_ACC_DONE_PASS_TWO)) {
        return (zval *)(((char *)opcode_copy) + ZEND_MM_ALIGNED_SIZE_EX(sizeof(zend_op) * op_array->last, 16));
    }
    (void)opcode_copy;
    return safe_emalloc(op_array->last_literal, sizeof(zval), 0);
}
/* }}} */

/* {{{ php_voyager_set_opcode_constant_relative
 */
static void php_voyager_set_opcode_constant_relative(const zend_op_array *op_array, const zend_op *opline, znode_op *op, zval *literalI)
{
#if ZEND_USE_ABS_CONST_ADDR
    RT_CONSTANT(opline, *op) = literalI;
#else
    op->constant = ((char *)literalI) - ((char *)opline);
#endif
}
/* }}} */

/* {{{ php_voyager_function_copy_ctor_same_type
 */
static void php_voyager_function_copy_ctor_same_type(zend_function *fe, zend_string *newname)
{
    zval *literals;
    void *run_time_cache;
    zend_string **dupvars;
    zend_op *last_op;
    zend_op *opcode_copy;
    zend_op_array *const op_array = &(fe->op_array);
    uint32_t i;

    if (newname) {
        zend_string_addref(newname);
        fe->common.function_name = newname;
    } else {
        zend_string_addref(fe->common.function_name);
    }

    if (fe->common.type == ZEND_USER_FUNCTION) {
        if (op_array->vars) {
            i = op_array->last_var;
            dupvars = safe_emalloc(op_array->last_var, sizeof(zend_string *), 0);
            while (i > 0) {
                i--;
                dupvars[i] = op_array->vars[i];
                zend_string_addref(dupvars[i]);
            }
            op_array->vars = dupvars;
        }

        if (op_array->static_variables) {
            op_array->static_variables = zend_array_dup(op_array->static_variables);
#if PHP_VERSION_ID >= 80200
            HashTable *ht_dup = zend_array_dup(op_array->static_variables);
            ZEND_MAP_PTR_INIT(op_array->static_variables_ptr, ht_dup);
#endif
        }

        if (VOYAGER_RUN_TIME_CACHE(op_array)) {
            run_time_cache = pemalloc(op_array->cache_size, 1);
            memset(run_time_cache, 0, op_array->cache_size);
#ifdef ZEND_MAP_PTR_SET
            ZEND_MAP_PTR_SET(op_array->run_time_cache, run_time_cache);
#else
            op_array->run_time_cache = run_time_cache;
#endif
        }

        opcode_copy = voyager_allocate_opcode_copy(op_array);
        last_op = op_array->opcodes + op_array->last;
        for (i = 0; i < op_array->last; i++) {
            opcode_copy[i] = op_array->opcodes[i];
            if (opcode_copy[i].op1_type != IS_CONST) {
                zend_op *opline;
                zend_op *jmp_addr_op1;
                switch (opcode_copy[i].opcode) {
#ifdef ZEND_GOTO
                    case ZEND_GOTO:
#endif
#ifdef ZEND_FAST_CALL
                    case ZEND_FAST_CALL:
#endif
                    case ZEND_JMP:
                        opline = &opcode_copy[i];
                        jmp_addr_op1 = OP_JMP_ADDR(opline, opline->op1);
                        if (jmp_addr_op1 >= op_array->opcodes &&
                            jmp_addr_op1 < last_op) {
#if ZEND_USE_ABS_JMP_ADDR
                            opline->op1.jmp_addr = opcode_copy + (op_array->opcodes[i].op1.jmp_addr - op_array->opcodes);
#else
                            opline->op1.jmp_offset += ((char *)op_array->opcodes) - ((char *)opcode_copy);
#endif
                        }
                }
            }
            if (opcode_copy[i].op2_type != IS_CONST) {
                zend_op *opline;
                zend_op *jmp_addr_op2;
                switch (opcode_copy[i].opcode) {
                    case ZEND_JMPZ:
                    case ZEND_JMPNZ:
                    case ZEND_JMPZ_EX:
                    case ZEND_JMPNZ_EX:
#ifdef ZEND_JMP_SET
                    case ZEND_JMP_SET:
#endif
#ifdef ZEND_JMP_SET_VAR
                    case ZEND_JMP_SET_VAR:
#endif
                        opline = &opcode_copy[i];
                        jmp_addr_op2 = OP_JMP_ADDR(opline, opline->op2);
                        if (jmp_addr_op2 >= op_array->opcodes &&
                            jmp_addr_op2 < last_op) {
#if ZEND_USE_ABS_JMP_ADDR
                            opline->op2.jmp_addr = opcode_copy + (op_array->opcodes[i].op2.jmp_addr - op_array->opcodes);
#else
                            opline->op2.jmp_offset += ((char *)op_array->opcodes) - ((char *)opcode_copy);
#endif
                        }
                }
            }
        }

        if (op_array->literals) {
            uint32_t k;
            literals = voyager_allocate_literals(op_array, opcode_copy);
            for (i = op_array->last_literal; i > 0; ) {
                i--;
                literals[i] = op_array->literals[i];
                Z_TRY_ADDREF(literals[i]);
            }
            for (k = 0; k < op_array->last; k++) {
                zend_op *const new_op = &opcode_copy[k];
                zend_bool found_op1 = 0;
                zend_bool found_op2 = 0;
                for (i = op_array->last_literal; i > 0; ) {
                    i--;
                    if (!found_op1 && new_op->op1_type == IS_CONST && RT_CONSTANT(&op_array->opcodes[k], new_op->op1) == &op_array->literals[i]) {
                        php_voyager_set_opcode_constant_relative(op_array, new_op, &(new_op->op1), &literals[i]);
                        found_op1 = 1;
                    }
                    if (!found_op2 && new_op->op2_type == IS_CONST && RT_CONSTANT(&op_array->opcodes[k], new_op->op2) == &op_array->literals[i]) {
                        php_voyager_set_opcode_constant_relative(op_array, new_op, &(new_op->op2), &literals[i]);
                        found_op2 = 1;
                    }
                }
            }
            op_array->literals = literals;
        }
        op_array->opcodes = opcode_copy;
        op_array->refcount = (uint32_t *)emalloc(sizeof(uint32_t));
        *op_array->refcount = 1;

        if (op_array->doc_comment) {
            zend_string_addref(op_array->doc_comment);
        }
        if (op_array->filename) {
            zend_string_addref(op_array->filename);
        }
        op_array->try_catch_array = (zend_try_catch_element *)estrndup((char *)op_array->try_catch_array, sizeof(zend_try_catch_element) * op_array->last_try_catch);
        if (op_array->live_range) {
            op_array->live_range = (zend_live_range *)estrndup((char *)op_array->live_range, sizeof(zend_live_range) * op_array->last_live_range);
        }
#if PHP_VERSION_ID >= 80100
        if (op_array->num_dynamic_func_defs) {
            const size_t size = sizeof(zend_op_array *) * op_array->num_dynamic_func_defs;
            zend_op_array **new_dynamic_func_defs = emalloc(size);
            memcpy(new_dynamic_func_defs, op_array->dynamic_func_defs, size);
            op_array->dynamic_func_defs = new_dynamic_func_defs;
        }
#endif

        if (op_array->arg_info) {
            zend_arg_info *tmpArginfo;
            zend_arg_info *originalArginfo;
            uint32_t num_args = op_array->num_args;
            int32_t offset = 0;

            if (op_array->fn_flags & ZEND_ACC_HAS_RETURN_TYPE) {
                offset++;
                num_args++;
            }
            if (op_array->fn_flags & ZEND_ACC_VARIADIC) {
                num_args++;
            }

            tmpArginfo = (zend_arg_info *)safe_emalloc(sizeof(zend_arg_info), num_args, 0);

            originalArginfo = &((op_array->arg_info)[-offset]);
            for (i = 0; i < num_args; i++) {
                tmpArginfo[i] = originalArginfo[i];
                if (tmpArginfo[i].name) {
                    zend_string_addref(tmpArginfo[i].name);
                }
                php_voyager_arginfo_type_addref(&tmpArginfo[i]);
            }
            op_array->arg_info = &tmpArginfo[offset];
        }
    }

    fe->common.prototype = fe;
}
/* }}} */

/* {{{ php_voyager_function_clone
 */
zend_function *php_voyager_function_clone(zend_function *fe, zend_string *newname, char orig_fe_type)
{
    zend_function *new_function = pemalloc(sizeof(zend_function), 1);
    if (fe->type == ZEND_INTERNAL_FUNCTION) {
        memset(new_function, 0, sizeof(zend_function));
        memcpy(new_function, fe, sizeof(zend_internal_function));
    } else {
        memcpy(new_function, fe, sizeof(zend_function));
    }
    php_voyager_function_copy_ctor_same_type(new_function, newname);
    return new_function;
}
/* }}} */

/* {{{ php_voyager_function_dtor_impl */
void php_voyager_function_dtor_impl(zend_function *fe, zend_bool is_clone)
{
    zend_bool is_user_function;
    is_user_function = fe->type == ZEND_USER_FUNCTION;
    zval zv;
    ZVAL_FUNC(&zv, fe);
    zend_function_dtor(&zv);
    if (is_clone && is_user_function) {
        pefree(fe, 1);
    }
}
/* }}} */

/* {{{ php_voyager_function_dtor */
void php_voyager_function_dtor(zend_function *fe)
{
    php_voyager_function_dtor_impl(fe, 1);
}
/* }}} */

/* {{{ php_voyager_request_function_restore_dtor */
void php_voyager_request_function_restore_dtor(zval *zv)
{
    voyager_function_restore *restore = (voyager_function_restore *)Z_PTR_P(zv);
    if (!restore) {
        return;
    }
    if (restore->funcname_lower) {
        zend_string_release(restore->funcname_lower);
    }
    if (restore->orig_fe) {
        php_voyager_function_dtor(restore->orig_fe);
    }
    efree(restore);
}
/* }}} */

/* {{{ php_voyager_remove_function_from_reflection_objects */
void php_voyager_remove_function_from_reflection_objects(zend_function *fe)
{
    uint32_t i;
    extern PHPAPI zend_class_entry *reflection_function_ptr;
    extern PHPAPI zend_class_entry *reflection_method_ptr;
    extern PHPAPI zend_class_entry *reflection_parameter_ptr;

    PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
        if (object->ce == reflection_function_ptr) {
            reflection_object *refl_obj = (reflection_object *)((char *)(object) - XtOffsetOf(reflection_object, zo));
            if (refl_obj->ptr == fe) {
                /* Free the reflection pointer */
                if (refl_obj->ptr) {
                    if (refl_obj->ref_type == REF_TYPE_FUNCTION) {
                        /* Don't free the function itself, just clear the reference */
                    } else if (refl_obj->ref_type == REF_TYPE_PARAMETER) {
                        parameter_reference *reference = (parameter_reference *)refl_obj->ptr;
                        (void)reference;
                        efree(refl_obj->ptr);
                    } else if (refl_obj->ref_type == REF_TYPE_PROPERTY) {
                        property_reference *prop_reference = (property_reference *)refl_obj->ptr;
                        zend_string_release_ex(prop_reference->unmangled_name, 0);
                        efree(refl_obj->ptr);
                    }
                }
                refl_obj->ptr = VOYAGER_G(removed_function);
            }
        } else if (object->ce == reflection_method_ptr) {
            reflection_object *refl_obj = (reflection_object *)((char *)(object) - XtOffsetOf(reflection_object, zo));
            if (refl_obj->ptr == fe) {
                zend_function *f = emalloc(sizeof(zend_function));
                memcpy(f, VOYAGER_G(removed_method), sizeof(zend_function));
                f->common.scope = fe->common.scope;
                f->internal_function.fn_flags |= ZEND_ACC_CALL_VIA_TRAMPOLINE;
                zend_string_addref(f->internal_function.function_name);
                if (refl_obj->ptr) {
                    if (refl_obj->ref_type == REF_TYPE_FUNCTION) {
                        /* Don't free */
                    } else if (refl_obj->ref_type == REF_TYPE_PARAMETER) {
                        parameter_reference *reference = (parameter_reference *)refl_obj->ptr;
                        (void)reference;
                        efree(refl_obj->ptr);
                    } else if (refl_obj->ref_type == REF_TYPE_PROPERTY) {
                        property_reference *prop_reference = (property_reference *)refl_obj->ptr;
                        zend_string_release_ex(prop_reference->unmangled_name, 0);
                        efree(refl_obj->ptr);
                    }
                }
                refl_obj->ptr = f;
            }
        } else if (object->ce == reflection_parameter_ptr) {
            reflection_object *refl_obj = (reflection_object *)((char *)(object) - XtOffsetOf(reflection_object, zo));
            parameter_reference *reference = (parameter_reference *)refl_obj->ptr;
            if (reference && reference->fptr == fe) {
                efree(refl_obj->ptr);
                refl_obj->ptr = NULL;
            }
        }
    PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_END
}
/* }}} */

/* {{{ php_voyager_clear_function_runtime_cache */
static void php_voyager_clear_function_runtime_cache(zend_function *f)
{
    zend_op_array *op_array;
    if (f->type != ZEND_USER_FUNCTION) {
        return;
    }
    op_array = &(f->op_array);
    if (op_array->cache_size == 0 || VOYAGER_RUN_TIME_CACHE(op_array) == NULL) {
        return;
    }
    memset(VOYAGER_RUN_TIME_CACHE(op_array), 0, op_array->cache_size);
}
/* }}} */

/* {{{ php_voyager_clear_all_functions_runtime_cache */
void php_voyager_clear_all_functions_runtime_cache(void)
{
    uint32_t i;
    zend_execute_data *ptr;
    zend_class_entry *ce;
    zend_function *f;

    ZEND_HASH_FOREACH_PTR(EG(function_table), f) {
        php_voyager_clear_function_runtime_cache(f);
    } ZEND_HASH_FOREACH_END();

    ZEND_HASH_FOREACH_PTR(EG(class_table), ce) {
        ZEND_HASH_FOREACH_PTR(&(ce->function_table), f) {
            php_voyager_clear_function_runtime_cache(f);
        } ZEND_HASH_FOREACH_END();
    } ZEND_HASH_FOREACH_END();

    for (ptr = EG(current_execute_data); ptr != NULL; ptr = ptr->prev_execute_data) {
        if (ptr->func == NULL || ptr->func->type == ZEND_INTERNAL_FUNCTION ||
            ptr->func->op_array.cache_size == 0 || VOYAGER_RUN_TIME_CACHE(&(ptr->func->op_array)) == NULL) {
            continue;
        }
        memset(VOYAGER_RUN_TIME_CACHE(&(ptr->func->op_array)), 0, ptr->func->op_array.cache_size);
    }

    PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
        if (object->ce == zend_ce_closure) {
            zend_closure *cl = (zend_closure *)object;
            php_voyager_clear_function_runtime_cache(&cl->func);
        }
    PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_END
}
/* }}} */

/* {{{ php_voyager_fix_hardcoded_stack_sizes */
static inline void php_voyager_fix_hardcoded_stack_sizes(zend_function *f, zend_string *called_name_lower, zend_function *called_f)
{
    zend_op_array *op_array;
    zend_op *opline_it;
    zend_op *end;
    if (f == NULL || f->type != ZEND_USER_FUNCTION) {
        return;
    }

    op_array = &(f->op_array);
    opline_it = op_array->opcodes;
    end = opline_it + op_array->last;

    for (; opline_it < end; opline_it++) {
        if (opline_it->opcode == ZEND_INIT_FCALL) {
            zval *function_name = (zval *)(RT_CONSTANT(opline_it, opline_it->op2));
            if (zend_string_equals(Z_STR_P(function_name), called_name_lower)) {
                uint32_t new_size = zend_vm_calc_used_stack(opline_it->extended_value, called_f);
                if (new_size > opline_it->op1.num) {
                    opline_it->op1.num = new_size;
                }
            }
        }
    }
}
/* }}} */

/* {{{ php_voyager_fix_hardcoded_stack_sizes_for_function_table */
static void php_voyager_fix_hardcoded_stack_sizes_for_function_table(HashTable *function_table, zend_string *called_name_lower, zend_function *called_f)
{
    zend_function *f;
    ZEND_HASH_FOREACH_PTR(function_table, f) {
        php_voyager_fix_hardcoded_stack_sizes(f, called_name_lower, called_f);
    } ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ php_voyager_fix_all_hardcoded_stack_sizes */
void php_voyager_fix_all_hardcoded_stack_sizes(zend_string *called_name_lower, zend_function *called_f)
{
    uint32_t i;
    zend_class_entry *ce;
    zend_execute_data *ptr;

    php_voyager_fix_hardcoded_stack_sizes_for_function_table(EG(function_table), called_name_lower, called_f);

    ZEND_HASH_FOREACH_PTR(EG(class_table), ce) {
        php_voyager_fix_hardcoded_stack_sizes_for_function_table(&(ce->function_table), called_name_lower, called_f);
    } ZEND_HASH_FOREACH_END();

    for (ptr = EG(current_execute_data); ptr != NULL; ptr = ptr->prev_execute_data) {
        if (ptr->func == NULL || ptr->func->type != ZEND_USER_FUNCTION) {
            continue;
        }
        php_voyager_fix_hardcoded_stack_sizes(ptr->func, called_name_lower, called_f);
    }

    PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_BEGIN(i)
        if (object->ce == zend_ce_closure) {
            zend_closure *cl = (zend_closure *)object;
            php_voyager_fix_hardcoded_stack_sizes(&cl->func, called_name_lower, called_f);
        }
    PHP_VOYAGER_ITERATE_THROUGH_OBJECTS_STORE_END
}
/* }}} */

/* {{{ php_voyager_generate_lambda_function
 */
int php_voyager_generate_lambda_function(const zend_string *arguments, const zend_string *return_type, const zend_bool is_strict, const zend_string *phpcode,
                                         zend_function **pfe, zend_bool return_ref)
{
    char *eval_code;
    char *eval_name;
    char *return_type_code;
    int eval_code_length;

    eval_code_length =
        (is_strict ? (sizeof("declare(strict_types=1);") - 1) : 0) +
        sizeof("function " VOYAGER_TEMP_FUNCNAME) +
        ZSTR_LEN(arguments) + 4 +
        ZSTR_LEN(phpcode) +
        (return_ref ? 1 : 0);
    if (return_type != NULL) {
        int return_type_code_length = ZSTR_LEN(return_type) + 4;
        return_type_code = (char *)emalloc(return_type_code_length + 1);
        snprintf(return_type_code, return_type_code_length + 4, " : %s ", ZSTR_VAL(return_type));
        eval_code_length += return_type_code_length;
    } else {
        return_type_code = (char *)emalloc(1);
        return_type_code[0] = '\0';
    }

    eval_code = (char *)emalloc(eval_code_length);
    snprintf(eval_code, eval_code_length, "%sfunction %s" VOYAGER_TEMP_FUNCNAME "(%s)%s{%s}",
        is_strict ? "declare(strict_types=1);" : "",
        (return_ref ? "&" : ""),
        ZSTR_VAL(arguments), return_type_code, ZSTR_VAL(phpcode));
    eval_name = zend_make_compiled_string_description("voyager runtime-created function");
    if (zend_eval_string(eval_code, NULL, eval_name) == FAILURE) {
        php_error_docref(NULL, E_ERROR, "Cannot create temporary function");
        efree(eval_code);
        efree(eval_name);
        efree(return_type_code);
        zend_hash_str_del(EG(function_table), VOYAGER_TEMP_FUNCNAME, sizeof(VOYAGER_TEMP_FUNCNAME) - 1);
        return FAILURE;
    }
    efree(eval_code);
    efree(eval_name);
    efree(return_type_code);

    if ((*pfe = zend_hash_str_find_ptr(EG(function_table), VOYAGER_TEMP_FUNCNAME, sizeof(VOYAGER_TEMP_FUNCNAME) - 1)) == NULL) {
        php_error_docref(NULL, E_ERROR, "Unexpected inconsistency creating temporary voyager function");
        return FAILURE;
    }

    return SUCCESS;
}
/* }}} */

/* {{{ php_voyager_generate_lambda_method
 */
int php_voyager_generate_lambda_method(const zend_string *arguments, const zend_string *return_type, const zend_bool is_strict, const zend_string *phpcode,
                                       zend_function **pfe, zend_bool return_ref, zend_bool is_static)
{
    char *eval_code;
    char *eval_name;
    char *return_type_code;
    int eval_code_length;
    zend_class_entry *ce;

    eval_code_length =
        (is_strict ? (sizeof("declare(strict_types=1);") - 1) : 0) +
        sizeof("class " VOYAGER_TEMP_CLASSNAME " { %sfunction " VOYAGER_TEMP_METHODNAME) +
        ZSTR_LEN(arguments) + 4 +
        ZSTR_LEN(phpcode) +
        (is_static ? (sizeof("static ") - 1) : 0) +
        (return_ref ? 1 : 0) +
        (sizeof("}") - 1);
    if (return_type != NULL) {
        int return_type_code_length = ZSTR_LEN(return_type) + 4;
        return_type_code = (char *)emalloc(return_type_code_length + 1);
        snprintf(return_type_code, return_type_code_length + 4, " : %s ", ZSTR_VAL(return_type));
        eval_code_length += return_type_code_length;
    } else {
        return_type_code = (char *)emalloc(1);
        return_type_code[0] = '\0';
    }

    eval_code = (char *)emalloc(eval_code_length);
    snprintf(eval_code, eval_code_length,
            "%sclass " VOYAGER_TEMP_CLASSNAME " { %sfunction %s" VOYAGER_TEMP_METHODNAME "(%s)%s{%s}}",
            (is_strict ? "declare(strict_types=1);" : ""),
            (is_static ? "static " : ""),
            (return_ref ? "&" : ""),
            ZSTR_VAL(arguments),
            return_type_code,
            ZSTR_VAL(phpcode));
    eval_name = zend_make_compiled_string_description("voyager runtime-created method");
    if (zend_eval_string(eval_code, NULL, eval_name) == FAILURE) {
        efree(eval_code);
        efree(eval_name);
        efree(return_type_code);
        php_error_docref(NULL, E_ERROR, "Cannot create temporary method");
        zend_hash_str_del(EG(class_table), VOYAGER_TEMP_CLASSNAME, sizeof(VOYAGER_TEMP_CLASSNAME) - 1);
        return FAILURE;
    }
    efree(eval_code);
    efree(eval_name);
    efree(return_type_code);

    ce = zend_hash_str_find_ptr(EG(class_table), VOYAGER_TEMP_CLASSNAME, sizeof(VOYAGER_TEMP_CLASSNAME) - 1);
    if (ce == NULL) {
        php_error_docref(NULL, E_ERROR, "Unexpected inconsistency creating a temporary class");
        return FAILURE;
    }

    if ((*pfe = zend_hash_str_find_ptr(&(ce->function_table), VOYAGER_TEMP_METHODNAME, sizeof(VOYAGER_TEMP_METHODNAME) - 1)) == NULL) {
        php_error_docref(NULL, E_ERROR, "Unexpected inconsistency creating a temporary method");
        return FAILURE;
    }

    return SUCCESS;
}
/* }}} */

/* {{{ php_voyager_cleanup_lambda_function */
int php_voyager_cleanup_lambda_function(void)
{
    if (zend_hash_str_del(EG(function_table), VOYAGER_TEMP_FUNCNAME, sizeof(VOYAGER_TEMP_FUNCNAME) - 1) == FAILURE) {
        php_error_docref(NULL, E_WARNING, "Unable to remove temporary function entry");
        return FAILURE;
    }
    return SUCCESS;
}
/* }}} */

/* {{{ php_voyager_cleanup_lambda_method */
int php_voyager_cleanup_lambda_method(void)
{
    if (zend_hash_str_del(EG(class_table), VOYAGER_TEMP_CLASSNAME, sizeof(VOYAGER_TEMP_CLASSNAME) - 1) == FAILURE) {
        php_error_docref(NULL, E_WARNING, "Unable to remove temporary method entry");
        return FAILURE;
    }
    return SUCCESS;
}
/* }}} */

/* {{{ php_voyager_update_reflection_object_name */
void php_voyager_update_reflection_object_name(zend_object *object, int handle, const char *name)
{
    zval prop_value;
    (void)handle;
    ZVAL_STRING(&prop_value, name);
#if PHP_VERSION_ID >= 80000
    zend_string *name_string = zend_string_init(name, strlen(name), 0);
    zend_std_write_property(object, name_string, &prop_value, NULL);
    zend_string_release(name_string);
#endif
    if (Z_REFCOUNTED_P(&prop_value)) {
        Z_DELREF_P(&prop_value);
    }
}
/* }}} */

/* The original destructor of the affected function_table. */
static dtor_func_t __function_table_orig_pDestructor = NULL;

static void php_voyager_function_table_dtor(zval *pDest)
{
    zend_function *fe = (zend_function *)Z_PTR_P(pDest);
    if (fe->type != ZEND_INTERNAL_FUNCTION && __function_table_orig_pDestructor != NULL) {
        __function_table_orig_pDestructor(pDest);
    }
}

/* {{{ voyager_zend_hash_add_or_update_function_table_ptr */
static inline void *voyager_zend_hash_add_or_update_function_table_ptr(HashTable *function_table, zend_string *key, void *pData, uint32_t flag)
{
    void *result;
    __function_table_orig_pDestructor = function_table->pDestructor;
    function_table->pDestructor = php_voyager_function_table_dtor;
    result = voyager_zend_hash_add_or_update_ptr(function_table, key, pData, flag);
    function_table->pDestructor = __function_table_orig_pDestructor;
    __function_table_orig_pDestructor = NULL;
    return result;
}
/* }}} */

/* {{{ php_voyager_restore_functions
       Restores all functions redefined during the current request.
 */
void php_voyager_restore_functions(void)
{
    HashTable *restores = VOYAGER_G(request_function_restores);
    voyager_function_restore *restore;

    if (!restores || zend_hash_num_elements(restores) == 0) {
        return;
    }

    ZEND_HASH_FOREACH_PTR(restores, restore) {
        zend_function *orig_fe;
        zend_function *redefined_fe;

        if (!restore || !restore->orig_fe) {
            continue;
        }

        orig_fe = restore->orig_fe;
        redefined_fe = zend_hash_find_ptr(EG(function_table), restore->funcname_lower);
        if (redefined_fe) {
            php_voyager_remove_function_from_reflection_objects(redefined_fe);
        }

        php_voyager_clear_all_functions_runtime_cache();
        php_voyager_fix_all_hardcoded_stack_sizes(restore->funcname_lower, orig_fe);

        if (voyager_zend_hash_add_or_update_function_table_ptr(EG(function_table),
                restore->funcname_lower, orig_fe, HASH_UPDATE) != NULL) {
            restore->orig_fe = NULL;
        }
    } ZEND_HASH_FOREACH_END();

    php_voyager_clear_all_functions_runtime_cache();
}
/* }}} */

/* {{{ php_voyager_function_add_or_update */
static void php_voyager_function_add_or_update(INTERNAL_FUNCTION_PARAMETERS, int add_or_update)
{
    zend_string *funcname;
    zend_string *funcname_lower;
    zend_string *arguments = NULL;
    zend_string *phpcode = NULL;
    zend_string *doc_comment = NULL;
    parsed_return_type return_type;
    parsed_is_strict is_strict;
    zend_bool return_ref = 0;
    zend_function *orig_fe = NULL, *source_fe = NULL, *func;
    char target_function_type;
    zval *args;
    int remove_temp = 0;
    long argc = ZEND_NUM_ARGS();
    long opt_arg_pos = 2;

    if (argc < 1 || zend_parse_parameters_ex(ZEND_PARSE_PARAMS_QUIET, 1, "S", &funcname) == FAILURE || !ZSTR_LEN(funcname)) {
        php_error_docref(NULL, E_ERROR, "Function name should not be empty");
        RETURN_FALSE;
    }

    if (argc < 2) {
        php_error_docref(NULL, E_ERROR, "Function body should be provided");
        RETURN_FALSE;
    }

    if (!php_voyager_parse_args_to_zvals(argc, &args)) {
        RETURN_FALSE;
    }

    if (!php_voyager_parse_function_arg(argc, args, 1, &source_fe, &arguments, &phpcode, &opt_arg_pos, "Function")) {
        efree(args);
        RETURN_FALSE;
    }

    if (argc > opt_arg_pos && !source_fe) {
        switch (Z_TYPE(args[opt_arg_pos])) {
            case IS_NULL:
            case IS_TRUE:
            case IS_FALSE:
                convert_to_boolean_ex(&args[opt_arg_pos]);
                return_ref = Z_TYPE(args[opt_arg_pos]) == IS_TRUE;
                break;
            default:
                php_error_docref(NULL, E_WARNING, "return_ref should be boolean");
        }
        opt_arg_pos++;
    }

    doc_comment = php_voyager_parse_doc_comment_arg(argc, args, opt_arg_pos);

    return_type = php_voyager_parse_return_type_arg(argc, args, opt_arg_pos + 1);

    is_strict = php_voyager_parse_is_strict_arg(argc, args, opt_arg_pos + 2);

    efree(args);
    if (!return_type.valid) {
        RETURN_FALSE;
    }
    if (!is_strict.valid) {
        RETURN_FALSE;
    }

    if (source_fe && return_type.return_type) {
        php_error_docref(NULL, E_WARNING, "Overriding return_type is not currently supported for closures");
        RETURN_FALSE;
    }
    if (source_fe && is_strict.overridden) {
        php_error_docref(NULL, E_WARNING, "Overriding is_strict is not currently supported for closures");
        RETURN_FALSE;
    }

    if (add_or_update == HASH_UPDATE &&
        (orig_fe = php_voyager_fetch_function(funcname, PHP_VOYAGER_FETCH_FUNCTION_REMOVE)) == NULL) {
        RETURN_FALSE;
    }

    funcname_lower = zend_string_tolower(funcname);

    if (add_or_update == HASH_ADD && zend_hash_exists(EG(function_table), funcname_lower)) {
        zend_string_release(funcname_lower);
        php_error_docref(NULL, E_WARNING, "Function %s() already exists", ZSTR_VAL(funcname));
        RETURN_FALSE;
    }

    if (!source_fe) {
        if (php_voyager_generate_lambda_function(arguments, return_type.return_type, is_strict.is_strict, phpcode, &source_fe, return_ref) == FAILURE) {
            zend_string_release(funcname_lower);
            RETURN_FALSE;
        }
        remove_temp = 1;
    }

    if (orig_fe) {
        target_function_type = orig_fe->type;
    } else {
        target_function_type = VOYAGER_G(replaced_internal_functions)
            && zend_hash_exists(VOYAGER_G(replaced_internal_functions), funcname_lower) ? ZEND_INTERNAL_FUNCTION : ZEND_USER_FUNCTION;
    }
    func = php_voyager_function_clone(source_fe, funcname, target_function_type);
    func->common.scope = NULL;
    func->common.fn_flags &= ~ZEND_ACC_CLOSURE;

    if (doc_comment == NULL && source_fe->op_array.doc_comment == NULL &&
        orig_fe && orig_fe->type == ZEND_USER_FUNCTION && orig_fe->op_array.doc_comment) {
        doc_comment = orig_fe->op_array.doc_comment;
    }
    php_voyager_modify_function_doc_comment(func, doc_comment);

    if (add_or_update == HASH_UPDATE) {
        if (VOYAGER_G(request_function_restores) &&
                !zend_hash_exists(VOYAGER_G(request_function_restores), funcname_lower)) {
            voyager_function_restore *restore = emalloc(sizeof(voyager_function_restore));
            restore->funcname_lower = zend_string_copy(funcname_lower);
            restore->orig_fe = php_voyager_function_clone(orig_fe, orig_fe->common.function_name, 0);
            restore->orig_fe->common.prototype = orig_fe->common.prototype;
            zend_hash_add_ptr(VOYAGER_G(request_function_restores), funcname_lower, restore);
        }
        php_voyager_remove_function_from_reflection_objects(orig_fe);
        php_voyager_clear_all_functions_runtime_cache();
        php_voyager_fix_all_hardcoded_stack_sizes(funcname_lower, func);
    }

    if (voyager_zend_hash_add_or_update_function_table_ptr(EG(function_table), funcname_lower, func, add_or_update) == NULL) {
        php_error_docref(NULL, E_WARNING, "Unable to add new function");
        zend_string_release(funcname_lower);
        if (remove_temp) {
            php_voyager_cleanup_lambda_function();
        }
        php_voyager_function_dtor(func);
        RETURN_FALSE;
    }

    if (remove_temp) {
        php_voyager_cleanup_lambda_function();
    }

    zend_string_release(funcname_lower);

    RETURN_TRUE;
}
/* }}} */

/* *****************
   * Functions API *
   ***************** */

/* {{{ proto bool voyager_function_redefine(string funcname, string arglist, string code[, bool return_by_reference[, string doc_comment]])
       proto bool voyager_function_redefine(string funcname, closure code[, string doc_comment])
 */
PHP_FUNCTION(voyager_function_redefine)
{
    php_voyager_function_add_or_update(INTERNAL_FUNCTION_PARAM_PASSTHRU, HASH_UPDATE);
}
/* }}} */
