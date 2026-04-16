You are editing a C source or header file for the Solaris embedded firmware project.
Apply ALL of the following transformations directly to the file. Do not ask for confirmation — apply changes and save.

## File to edit

The file path is provided as the argument to this command: $ARGUMENTS

Read the file first, then apply every rule below, then write the result back.

---

## Rules to apply

### 1. File header Doxygen block

Every file must start with exactly this block (adapt content to the file):

```c
/**
 * @file <filename>
 * @brief <one-line description of the file's purpose>.
 *
 * <Optional extended description. For headers: describe the module's
 * responsibility. For .c files: "See <header>.h for the public API.">
 *
 * Naming conventions used in this file:
 *  - Constants / macros : K_SPP_MODULE_*
 *  - Types              : SPP_ModuleName_t
 *  - Public functions   : SPP_Module_functionName()
 *  - Pointer params     : p_paramName
 *  - Static module vars : s_varName
 */
```

For `.c` files the naming conventions block is optional if the file has no public symbols.

### 2. Section dividers

Group declarations into sections using this exact divider style (no more, no less):

```c
/* ----------------------------------------------------------------
 * Section Name
 * ---------------------------------------------------------------- */
```

Standard sections for headers (in order): Constants → Types → Public Functions
Standard sections for sources (in order): Static Variables → Private Functions → Public Functions

Only add sections that actually have content. Do not add empty sections.

### 3. Function Doxygen blocks

Every public function declaration (in `.h`) and definition (in `.c` if not already in `.h`) must have:

```c
/**
 * @brief <short imperative description, e.g. "Initialize the databank.">.
 *
 * <Optional extended description.>
 *
 * @param[in]     p_name  <Description. State if NULL is allowed or not.>
 * @param[in,out] p_name  <Description.>
 * @param[out]    p_name  <Description.>
 *
 * @return SPP_OK on success.
 * @return SPP_ERROR_NULL_POINTER if <condition>.
 * @return SPP_ERROR_INVALID_PARAMETER if <condition>.
 */
```

- Use `@param[in]`, `@param[out]`, or `@param[in,out]` — never plain `@param`.
- List every distinct return code the function can return, one `@return` per code.
- If the function returns `void`, omit the `@return` section.
- Align the parameter descriptions by padding with spaces after the parameter name.

### 4. Struct / typedef Doxygen blocks

```c
/**
 * @brief <Short description of what the struct represents.>
 */
typedef struct {
    spp_uint32_t  field;  /**< @brief <Description of this field.> */
} SPP_ModuleName_t;
```

Every struct member must have an inline `/**< @brief ... */` comment.

### 5. Enum Doxygen blocks

```c
/**
 * @brief <Short description of the enum.>
 */
typedef enum {
    K_SPP_MODULE_VALUE_ONE = 0, /**< @brief <Description.> */
    K_SPP_MODULE_VALUE_TWO = 1, /**< @brief <Description.> */
} SPP_ModuleEnum_t;
```

### 6. Macro / constant Doxygen blocks

Single-line:
```c
/** @brief <Description of the constant.> */
#define K_SPP_MODULE_NAME  (value)
```

Multi-line (function-like macros):
```c
/**
 * @brief <Description.>
 * @param name  <Description of parameter.>
 */
#define SPP_MACRO_NAME(name)  (expression)
```

### 7. Naming conventions — check and flag

Do NOT rename symbols automatically, as that would break compilation.
Instead, after applying all Doxygen changes, append a comment block at the very end of the file listing any naming violations found:

```
/* NAMING REVIEW
 * The following symbols do not follow the Solaris naming conventions
 * and should be renamed manually to avoid breaking other files:
 *
 *   line 42: `someVariable` → should be `s_someVariable` (static module var)
 *   line 78: `MY_CONSTANT`  → should be `K_SPP_MODULE_MY_CONSTANT`
 */
```

Violations to check:
- Constants/macros not starting with `K_` (except `SPP_OK` / `SPP_ERROR*` return codes and include guards)
- Types not ending in `_t`
- Public functions not following `MODULE_camelCase()` pattern
- Pointer parameters not using `p_` prefix
- Static module-level variables not using `s_` prefix
- Global variables not using `g_` prefix

If there are no violations, do not add the comment block.

### 8. Include guards

Headers must use include guards in this form:
```c
#ifndef SPP_MODULE_NAME_H
#define SPP_MODULE_NAME_H

/* ... content ... */

#endif /* SPP_MODULE_NAME_H */
```

The guard name must match the file path: `spp/core/packet.h` → `SPP_CORE_PACKET_H`.

---

## What NOT to change

- Do not reformat code logic, expressions, or control flow.
- Do not rename any symbols (only flag them in the NAMING REVIEW block).
- Do not change `#include` order.
- Do not add or remove functionality.
- Preserve all existing comments that are not being replaced by Doxygen blocks.
