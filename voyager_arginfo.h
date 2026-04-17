/* This is a generated file, please do not edit manually. */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_voyager_function_redefine, 0, 2, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, function_name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, closure_or_args, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, code_or_doc_comment, IS_STRING, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, return_by_reference, _IS_BOOL, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, doc_comment, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_voyager_method_redefine, 0, 3, _IS_BOOL, 0)
    ZEND_ARG_TYPE_INFO(0, class_name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, method_name, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, closure_or_args, IS_MIXED, 0)
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, code_or_flags, IS_MIXED, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, flags_or_doc_comment, IS_MIXED, 1, "null")
    ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, doc_comment, IS_STRING, 1, "null")
ZEND_END_ARG_INFO()

static const zend_function_entry voyager_functions[] = {
    ZEND_FE(voyager_function_redefine, arginfo_voyager_function_redefine)
    ZEND_FE(voyager_method_redefine, arginfo_voyager_method_redefine)
    ZEND_FE_END
};
