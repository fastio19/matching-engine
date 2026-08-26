---
description: "Use when fixing Coverity static analysis defects in Teamcenter C/C++ code. Covers RESOURCE_LEAK, FORWARD_NULL, REVERSE_INULL, UNINIT, USE_AFTER_FREE, memory handling with SM_alloc/SM_free/MEM_alloc/MEM_free, RAII with scoped_ptr/scoped_smptr, ITK error handling, and Teamcenter coding standards."
applyTo: "**/*.{c,cpp,cxx,h,hxx}"
---

# Coverity Defect Fix Instructions for Teamcenter C/C++ Code

You are an expert at fixing Coverity static analysis defects in Teamcenter server-side C/C++ code.
Follow the Teamcenter coding standards, C++ Core Guidelines (with Teamcenter exceptions), and
Sutter & Alexandrescu best practices.

---

## General Principles

- Follow the **C++ Core Guidelines** as the external coding standard for Teamcenter, with documented Teamcenter exceptions.
- Reference: *Coding Standards: 101 Rules, Guidelines, and Best Practices* by Sutter & Alexandrescu.
- Comply with **SEI CERT Secure Coding Guidelines** and **OWASP Secure Coding Principles** (referenced by Teamcenter coding standards).
- Present clear, descriptive feedback. Comply with ISO audit requirements.

---

## Memory Handling

### Allocation & Deallocation Rules

- **Internal Teamcenter code** must use `SM_alloc` / `SM_free` for shared memory allocation.
- **User exit / sample / customization code** must use `MEM_alloc` / `MEM_free` (C-callable wrappers around SM functions).
- All dynamic memory returned from ITK functions must be freeable with a single call to `SM_free` / `MEM_free`.
- **SOA framework** uses standard memory allocator internally (SM is not suitable for multithreaded code).
- **Third-party libraries** (e.g., POCO) continue to use standard memory allocation.
- Never mix allocators: memory allocated with `SM_alloc` must be freed with `SM_free`, not `free()` or `delete`.

### RAII & Smart Pointers (Mandatory)

Smart pointers can help avoid **more than 90%** of memory leak issues. Always prefer them over raw pointers.

**Teamcenter-specific smart pointer classes** (not STL smart pointers):

| Class | Header | Use When |
|-------|--------|----------|
| `scoped_smptr<T>` | `<base_utils/ScopedSmPtr.hxx>` | Memory allocated via `SM_alloc` or `SM_alloc_persistent` |
| `scoped_ptr<T>` | `<base_utils/ScopedPtr.hxx>` | Memory allocated via `new` or standard allocator |
| `scoped_array<T>` | `<base_utils/ScopedArray.hxx>` | Array memory allocated via `new[]` |

**Do NOT use STL smart pointers** (`std::shared_ptr`, `std::unique_ptr`, `std::auto_ptr`) per NX/Teamcenter standards.

Also use:
- `StdSmVector`, `StdSmMap` and friends for SM-aware containers
- `TagVector` and friends for handy tag typedefs
- `POMRef` (see `tcref.hxx`) for POM references

### Which scoped_ptr for which ITK?

```cpp
// 1) POM ITKs - output memory is packed, use scoped_smptr
int nRows = 0;
int nCols = 0;
scoped_smptr<void **> objs = 0;
pStat = POM_enquiry_execute( query.c_str(), &nRows, &nCols, &objs );

// 2) Other ITKs with packed memory - use scoped_smptr
int count = 0;
scoped_smptr<char*> prefValues;
rStat = PREF_ask_char_values( preferenceName.c_str(), &count, &prefValues );

// 3) Pass scoped_ptr directly to ITK instead of raw pointer
scoped_smptr<tag_t> inputTags;
scoped_smptr<tag_t> outputTags;
stat = setParamDefs( &inputCount, &inputTags, &outputCount, &outputTags );
// Memory auto-cleaned when scoped_smptr goes out of scope
```

### Example: Correct RAII Pattern

```cpp
if ( doSomething() == OK )
{
    int inputCount = 0;
    int outputCount = 0;

    // Smart pointers for managing SM storage
    scoped_smptr<tag_t> inputTags;
    scoped_smptr<tag_t> outputTags;

    if ( doSomethingElseLater() == OK )
    {
        stat = setParamDefs( &inputCount, &inputTags,
                             &outputCount, &outputTags );
    }
    // ...
}
// scoped_smptr is now out of scope - SM memory is auto-cleaned
```

---

## Coverity Defect Categories & Fix Patterns

### RESOURCE_LEAK (Memory Leak)

A memory leak is where heap or SM memory has been allocated but not correctly freed/deallocated in all possible execution paths.

**DOs:**
- Use `scoped_ptr` or `scoped_smptr` whenever you define a pointer. This avoids the vast majority of resource leak defects.
- Assign pointer to `scoped_smptr` immediately after memory allocation in legacy code.
- Pass `scoped_smptr` directly to ITK instead of raw pointer, then assign.

**DONTs:**
```cpp
// BAD: Memory leak in loop - only frees last iteration
for (int i = 0; i < count; i++)
{
    arry = SM_alloc(...);
    // ... use arry ...
}
SM_free(arry); // Only frees the LAST iteration's allocation!

// BAD: Overwriting pointer without freeing previous value
value = SM_alloc(...);
value = SM_alloc(...); // Previous allocation leaked!

// BAD: Conditional path leaks
char* str = (char*)MEM_alloc(100);
if (error_condition)
    return ifail;  // str leaked on this path!
MEM_free(str);
```

**FIX with RAII:**
```cpp
// GOOD: Use scoped_smptr - automatic cleanup on all paths
scoped_smptr<char> str( (char*)SM_alloc(100) );
if (error_condition)
    return ifail;  // str automatically freed
```

### FORWARD_NULL / REVERSE_INULL (Null Pointer Dereference)

**FORWARD_NULL:** A pointer checked against NULL is then dereferenced without a guard.
**REVERSE_INULL:** A pointer is dereferenced, then later checked against NULL (implying it could be NULL).

**DOs:**
- Check all required input arguments for NULL at function entry.
- Always check ITK return values before using output pointers.
- When you check a pointer against NULL, make sure this pointer has not been dereferenced before.

```cpp
// BAD: REVERSE_INULL - dereference before null check
int len = strlen(name);   // dereferenced here
if (name == NULL)          // checked here - too late!
    return ERROR;

// GOOD: Check first, then dereference
if (name == NULL)
    return ERROR;
int len = strlen(name);
```

### UNINIT (Uninitialized Variable)

Uninitialized variables can cause undefined behavior.

**DOs:**
- Initialize every variable at the time you define it.
- Initialize all output parameters in ITK functions.

```cpp
// BAD
tag_t objTag;
int count;

// GOOD
tag_t objTag = NULLTAG;
int count = 0;
char* name = NULL;
```

### USE_AFTER_FREE (Use After Free)

**DOs:**
- Set pointer to NULL immediately after freeing.
- Don't release resources twice.

```cpp
// BAD: Double free in ERROR_RECOVER
SM_free(ptr);
// ... more code ...
ERROR_RECOVER
SM_free(ptr);  // ptr already freed above!

// GOOD: Set to NULL after free
SM_free(ptr);
ptr = NULL;
// ...
ERROR_RECOVER
SM_free(ptr);  // Safe - SM_free(NULL) is a no-op
```

---

## Error Handling Patterns

### C (ITK) Error Handling

ITK functions return `status_t` (int). Always check return values.

```cpp
// BAD: Not adding ifail to error stack
ifail = POM_set_attr_tag(1, &myTag, attrId, parentObj_tag);
if (ifail != ITK_ok)
    return ifail;  // Error lost from stack

// GOOD: Use ResultStatus for automatic error stack management
ResultStatus status = AOM_ask_num_elements(objectTag, propName, &num);
```

### C++ Error Handling

For exceptional errors in C++ layer, throw `IFail`:

```cpp
// Throw IFail for exceptional conditions
throw IFail( ITEM_bomline_does_not_have_sos );

// Catch IFail exceptions
try
{
    // Some code that may throw
}
catch ( const IFail& e )
{
    Teamcenter::CAE::logger()->note( ERROR_line, "\n**ERROR:%d.", e.ifail() );
    break;
}
```

### ERROR_PROTECT / ERROR_RECOVER Pattern

Use `ERROR_FINALLY_PROTECT` for guaranteed cleanup:

```cpp
// BEST: Use ERROR_FINALLY for guaranteed cleanup
void* ptr = NULL;

ERROR_FINALLY_PROTECT
    ptr = SM_alloc( ... );
    // ... use ptr ...
ERROR_FINALLY
    SM_free( ptr );
ERROR_FINALLY_END

ERROR_RECOVER
    // handle error
ERROR_END
```

Alternative pattern (set pointer to NULL after free):

```cpp
ERROR_PROTECT
    ptr = SM_alloc( ... );
    // ... use ptr ...
    SM_free( ptr );
    ptr = NULL;        // <-- Critical: set to NULL after free
ERROR_RECOVER
    SM_free( ptr );    // Safe because ptr is NULL if already freed
ERROR_END
```

---

## Code Review Checklist (from Teamcenter standards)

When fixing Coverity defects, verify:

1. **Memory:** No leaks on any execution path. Use smart pointers.
2. **Null checks:** All input args validated. No dereference-before-check.
3. **Initialization:** All variables initialized at definition.
4. **Resource cleanup:** No double-free. Set pointers to NULL after free.
5. **Error propagation:** ITK errors added to error stack, not silently dropped.
6. **Smart pointers:** Correct variant used (`scoped_smptr` for SM, `scoped_ptr` for `new`).
7. **No STL smart pointers:** Use Teamcenter variants only.
8. **Cannot pass non-POD through varargs:** Never pass `scoped_smptr` to printf-style functions. Use `.getString()` instead.

---

## Common Coverity Fix Quick Reference

| Defect | Root Cause | Fix |
|--------|-----------|-----|
| RESOURCE_LEAK | Raw pointer not freed on all paths | Wrap in `scoped_smptr` / `scoped_ptr` |
| FORWARD_NULL | Dereference after possible NULL return | Add NULL check before use |
| REVERSE_INULL | Dereference before NULL check | Move NULL check before first use |
| UNINIT | Variable used before initialization | Initialize at declaration |
| USE_AFTER_FREE | Pointer used after `SM_free`/`MEM_free` | Set to NULL after free; use RAII |
| OVERRUN | Buffer access beyond bounds | Validate array index/size |
| CHECKED_RETURN | ITK return value ignored | Check `ifail != ITK_ok` |
| DEADCODE | Unreachable code after return/throw | Remove or restructure control flow |

---

## Reference Links

- [Teamcenter Server Development Guide](http://cipgweb/tc_devdoc/dokuwiki/knowledge_base:procedures:modify_source:teamcenter_development_guide)
- [Teamcenter Coding Standard](http://cipgweb/tc_devdoc/dokuwiki/knowledge_base:procedures:modify_source:teamcenter_coding_standard)
- [C++ Core Guidelines (Teamcenter)](http://cipgweb/tc_devdoc/dokuwiki/knowledge_base:procedures:modify_source:cpp_core_guidelines)
- [Memory Handling](http://cipgweb/tc_devdoc/dokuwiki/knowledge_base:procedures:modify_source:teamcenter_coding_standards:memory_handling)
- [Error Handling Best Practices](http://cipgweb/tc_devdoc/dokuwiki/knowledge_base:procedures:modify_source:teamcenter_server_development:error_handling)
- [DOs and DONTs for Coverity](http://cipgweb/tc_devdoc/dokuwiki/sandbox:chenji:main)
- [Coverity Desktop Analysis](http://cipgweb/tc_devdoc/dokuwiki/sandbox:coverity:coverity_desktop_analysis)
- [Code Review Checklist](http://cipgweb/tc_devdoc/dokuwiki/knowledge_base:teamcenter_university:meta:codereviewchecklist)
- [Scoped Pointer Usage](http://cipgweb/tc_devdoc/dokuwiki/sandbox:now_i_am_getting_another_error_while_building_testcases:main)
- [Great Code First Time Talk Series](http://cipgweb/tc_devdoc/dokuwiki/greatcode)


