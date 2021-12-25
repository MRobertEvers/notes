# Exploring Zig's Async Implementation

I was interested to see how Zig implemented async functions. By reading the docs, I had a pretty good idea, but I wanted to be sure.

## Basic Idea

The basic idea is that the codegen for async function generates some book keeping checks at the start of the function, and additionally stores all arguments and local variables in a 'frame' that is kept around for later calls.

## The Frame

The frame is stored in memory wherever the frame variable is stored. It has a structure like this; Note: this is on a 64 bit system so pointers are 8 bytes.

```
struct AsyncFrame { // unique per function
  // pointer to the async function
  void* async_fn;                   // frame + 0   // Pointer to the async function itself.
  uint64_t resume_index;            // frame + 8   // used to in the jump table at the async func 
  void*    awaiter_frame;           // frame + 16  // frame of the awaiting function. Usually zero; populated at callsite `var r = async_fn()` if called without `async` keyword (i.e. not like `var r = async async_fn()) if it was called in an async function; OR, when calling `var r = await async_fn_frame`. Note that in the first cast, if it was called normally in an async function, then the call semantics appear as though it were a normal function call.
  // A pointer to frame + 40; There is space in the frame for the return value. However, there are some cases where
  // the result_pointer points somewhere else, e.g. when provided explicitly by @asyncCall (search frame_ret_start + 2)
  ReturnType* result_pointer;       // frame + 24 // a pointer to the result
  void* awaiter_result_ptr;         // frame + 32 // a pointer to where the awaiter's result is; this is not populated in the initial call (as there is nothing awaiting) the value at [frame + 24] is copied to [frame + 32] // (search frame_ret_start + 1 or gen_await_early_return)
  ReturnType return;                // frame + 40 // A small buffer that *usually* stores the awaiters return type.
  Arg1Type arg1;                    // frame + 48
  Arg2Type arg2;                    // frame + 48 + sizeof(Arg1Type) 
  Arg3Type arg3;                    // ...
  LocalVar1Type local1;             // frame + 48 + sizeof(...args)
  LocalVar1Type local2;             // frame + 48 + sizeof(...args) + sizeof(LocalVar1Type)
  ...
}
```

Which, in the Zig compiler code, looks like this,

```c++
// all_types.hpp
// label (grep this): [fn_frame_struct_layout]
static const size_t frame_fn_ptr_index = 0;
static const size_t frame_resume_index = 1;
static const size_t frame_awaiter_index = 2;
static const size_t frame_ret_start = 3;
```

The compiler effective uses a _generator_ type pattern to implement suspend/resume. `resume` produces a `call` argument, and `suspend` produces a return. `await` does not produce a call. The call argument simply calls into the function normally. The generated code for the async function, however, begins the function with a jump table, which figures out where it should go in the function.

From the Zig docs:

```
There is a common misconception that await resumes the target function. 
It is the other way around: it suspends until the target function completes. 
In the event that the target function has already completed, await does not suspend; 
instead it copies the return value directly from the target function's frame.
```

`await` is a suspend point inside the awaiting function.

`resume` is the only way to re-enter a suspended function.

The async function codegen starts with copying the frame's local variables and function arguments onto the stack (these are called `async_spills` in the zig source), then a jump table lookup; i.e. it looks for the position in the function that it should return. It uses the `resume_index` to do this. The geneted code for the entry point of an async function looks like below.

```
async_func:
        # Render the async spills. I.e. Put all the local vars and the args from the frame onto the stack via pointers.
        push    rbp
        mov     rbp, rsp
        sub     rsp, 48
        mov     rax, rdi
        add     rax, 16
        mov     qword ptr [rbp - 24], rax
        mov     rax, rdi
        add     rax, 8
        mov     qword ptr [rbp - 16], rax

        # Jump table for returning to the correct position in the function.
        mov     rax, qword ptr [rdi + 8]
        mov     qword ptr [rbp - 8], rax
        sub     rax, 3
        ja      .LBB3_2
        mov     rax, qword ptr [rbp - 8]
        mov     rax, qword ptr [8*rax + .LJTI3_0]
        jmp     rax
```

The async spills are copied from their corresponding position onto the stack, in the zig source. An interesting feature of zig is that types are first class citizens, `type_has_bits` returns 0 for compile time types and arguments.

```cpp
static void render_async_spills(CodeGen *g) {
    ZigType *fn_type = g->cur_fn->type_entry;
    ZigType *import = get_scope_import(&g->cur_fn->fndef_scope->base);

    CalcLLVMFieldIndex arg_calc = {0};
    frame_index_arg_calc(g, &arg_calc, fn_type->data.fn.fn_type_id.return_type);
    for (size_t var_i = 0; var_i < g->cur_fn->variable_list.length; var_i += 1) {
        ZigVar *var = g->cur_fn->variable_list.at(var_i);

        if (!type_has_bits(g, var->var_type)) {
            continue;
        }
        if (ir_get_var_is_comptime(var))
            continue;
        switch (type_requires_comptime(g, var->var_type)) {
            case ReqCompTimeInvalid:
                zig_unreachable();
            case ReqCompTimeYes:
                continue;
            case ReqCompTimeNo:
                break;
        }
        if (var->src_arg_index == SIZE_MAX) {
            continue;
        }

        calc_llvm_field_index_add(g, &arg_calc, var->var_type);
        var->value_ref = LLVMBuildStructGEP(g->builder, g->cur_frame_ptr, arg_calc.field_index - 1, var->name);
        if (var->decl_node) {
            var->di_loc_var = ZigLLVMCreateAutoVariable(g->dbuilder, get_di_scope(g, var->parent_scope),
                var->name, import->data.structure.root_struct->di_file,
                node_line_onebased(var->decl_node),
                get_llvm_di_type(g, var->var_type), !g->strip_debug_symbols, 0);
            gen_var_debug_decl(g, var);
        }
    }

    ZigType *frame_type = g->cur_fn->frame_type->data.frame.locals_struct;

    for (size_t alloca_i = 0; alloca_i < g->cur_fn->alloca_gen_list.length; alloca_i += 1) {
        Stage1AirInstAlloca *instruction = g->cur_fn->alloca_gen_list.at(alloca_i);
        if (instruction->field_index == SIZE_MAX)
            continue;

        size_t gen_index = frame_type->data.structure.fields[instruction->field_index]->gen_index;
        instruction->base.llvm_value = LLVMBuildStructGEP(g->builder, g->cur_frame_ptr, gen_index,
                instruction->name_hint);
    }
}
```

Additionally, notice that the variables are not copied via value, but rather via pointer `LLVMBuildStructGEP` onto the stack. This is not normally the case for for other function calls. This is why the helper function `ir_llvm_value` is used to get the value of a variable on the stack. It checks if `instruction->spill` is populated, which it will be for async functions, and generates the correct instruction for loading the value. For true pointers, i.e. arguments or local vars that are pointers, this is a no-op, because the pointer itself is already on the stack. For non-pointer arguments or local variables, it has to perform a load, because the value itself is not on the stack, but rather in the frame.

```cpp
// ir_llvm_value
if (instruction->spill != nullptr) {
    ZigType *ptr_type = instruction->spill->value->type;
    ir_assert(ptr_type->id == ZigTypeIdPointer, instruction);
    return get_handle_value(g, ir_llvm_value(g, instruction->spill),
        ptr_type->data.pointer.child_type, instruction->spill->value->type);
}
```

The position in the frame where arguments and local vars are stored is calculated in the following function. Notice that it assumes the size of each of the fields in the frame are `entry_usize`.

```cpp
static void frame_index_trace_arg_calc(CodeGen *g, CalcLLVMFieldIndex *calc, ZigType *return_type) {
    calc_llvm_field_index_add(g, calc, g->builtin_types.entry_usize); // function pointer
    calc_llvm_field_index_add(g, calc, g->builtin_types.entry_usize); // resume index
    calc_llvm_field_index_add(g, calc, g->builtin_types.entry_usize); // awaiter index

    if (type_has_bits(g, return_type)) {
        calc_llvm_field_index_add(g, calc, g->builtin_types.entry_usize); // *ReturnType (callee's)
        calc_llvm_field_index_add(g, calc, g->builtin_types.entry_usize); // *ReturnType (awaiter's)
        calc_llvm_field_index_add(g, calc, return_type); // ReturnType
    }
}
```

After the spills are copied to the stack, the function performs a jump to the correct location.

The generated code for a suspend function generates code that stores the position in the function into the frame object before calling `ret`.

e.g.

```rust
var frame: @Frame(func) = undefined;

fn amain() void {
    frame = async func();
    resume &frame;
    await &frame;
}

fn func() void {
    suspend {}
    suspend {}
    return;
}

export fn main() void {
    _ = async amain();
}
```

The entry code of `func` always stores a `1` in the resume index. This happens before the first suspend.

```
        mov     qword ptr [rax], 1
```

The first suspend puts a `2` in the resume index and returns.

```
        mov     qword ptr [rax], 2
        add     rsp, 48
        pop     rbp
        ret
```

The second suspend puts a `3` in the resume index and returns.

```
        mov     qword ptr [rax], 3
        add     rsp, 48
        pop     rbp
        ret
```

We can see this in the `gen_suspend_begin` code
```
static LLVMBasicBlockRef gen_suspend_begin(CodeGen *g, const char *name_hint) {
    LLVMTypeRef usize_type_ref = g->builtin_types.entry_usize->llvm_type;
    LLVMBasicBlockRef resume_bb = LLVMAppendBasicBlock(g->cur_fn_val, name_hint);
    size_t new_block_index = g->cur_resume_block_count;
    g->cur_resume_block_count += 1;
    LLVMValueRef new_block_index_val = LLVMConstInt(usize_type_ref, new_block_index, false);
    LLVMAddCase(g->cur_async_switch_instr, new_block_index_val, resume_bb);
    LLVMBuildStore(g->builder, new_block_index_val, g->cur_async_resume_index_ptr);
    return resume_bb;
}
```

I omitted a few things above, here is the actual assembly generated,

```
        mov     qword ptr [rax], 1
        mov     qword ptr [rax], 3
        add     rsp, 48
        pop     rbp
        ret
```

The first store (redundant) stores a 1 in the current_resume index which is the `bad_resume` block. Recall that the first two entries in the jump table for an async function are 0: the function start, and 1: bad resume. 1 indicates that the function is not suspended and will result in a runtime errors. See e.g. below. `g->cur_bad_not_suspended_index` is always 1, due to the way async calls are generated.

```
static LLVMValueRef ir_render_suspend_finish(CodeGen *g, Stage1Air *executable,
        Stage1AirInstSuspendFinish *instruction)
{
    LLVMBuildRetVoid(g->builder);

    LLVMPositionBuilderAtEnd(g->builder, instruction->begin->resume_bb);
    if (ir_want_runtime_safety(g, &instruction->base)) {
        LLVMBuildStore(g->builder, g->cur_bad_not_suspended_index, g->cur_async_resume_index_ptr);
    }
    render_async_var_decls(g, instruction->base.scope);
    return nullptr;
}
```

## Notes

We can compare the LLVM output to assembly; I've added annotations as notes.

```cpp
// for (auto fn : fns) {
// ...
// if (is_async) { 
// ...
LLVMValueRef resume_index = LLVMBuildLoad(g->builder, resume_index_ptr, "");
LLVMValueRef switch_instr = LLVMBuildSwitch(g->builder, resume_index, bad_resume_block, 4);

// This is used when a suspend/await block is encountered the function
g->cur_async_switch_instr = switch_instr;

LLVMValueRef zero = LLVMConstNull(usize_type_ref);
Stage1AirBasicBlock *entry_block = executable->basic_block_list.at(0);
LLVMAddCase(switch_instr, zero, entry_block->llvm_block);
g->cur_resume_block_count += 1;
// }
// }

//...

// Appends a case to the current llvm switch.
static LLVMBasicBlockRef gen_suspend_begin(CodeGen *g, const char *name_hint) {
    LLVMTypeRef usize_type_ref = g->builtin_types.entry_usize->llvm_type;
    LLVMBasicBlockRef resume_bb = LLVMAppendBasicBlock(g->cur_fn_val, name_hint);
    size_t new_block_index = g->cur_resume_block_count;
    g->cur_resume_block_count += 1;
    LLVMValueRef new_block_index_val = LLVMConstInt(usize_type_ref, new_block_index, false);
    LLVMAddCase(g->cur_async_switch_instr, new_block_index_val, resume_bb);
    LLVMBuildStore(g->builder, new_block_index_val, g->cur_async_resume_index_ptr);
    return resume_bb;
}


```

```assembly
# When calling the function, or calling resume on the frame, this is where execution goes.
# The jump table at the bottom jumps to the point in the function where it last left off.
func:
        push    rbp
        mov     rbp, rsp
        sub     rsp, 160
        mov     qword ptr [rbp - 72], rdi
        mov     rax, rdi
        add     rax, 48
        mov     qword ptr [rbp - 64], rax
        mov     rax, rdi
        add     rax, 56
        mov     qword ptr [rbp - 56], rax
        mov     rax, rdi
        add     rax, 57
        mov     qword ptr [rbp - 48], rax
        mov     rax, rdi
        add     rax, 58
        mov     qword ptr [rbp - 40], rax
        mov     rax, rdi
        add     rax, 16
        mov     qword ptr [rbp - 32], rax
        mov     rax, rdi
        add     rax, 8
        mov     qword ptr [rbp - 24], rax
        mov     rax, qword ptr [rdi + 8]
        mov     qword ptr [rbp - 16], rax
        mov     rcx, qword ptr [rdi + 24]
        mov     qword ptr [rbp - 8], rcx
        # <---- This is 1 + the number of suspends, where is cur_resume_block_count used? How does it know the number of suspends and why is the switchCase always declared with 4 cases? Otherwise, this is a basic switch case implementation, make sure the value we are switching on is smaller than the number of cases in the jump table so we can just index into the jump table with the value. A: See answers below.
        # <---- This is one of the basic steps in creating a jump table. Ensure that the jump param is not going
        # to jump you over the end of the table.
        sub     rax, 4 
        # <---- this is a jump to a panic saying "resumed an async function which already returned"
        ja      .LBB3_2 
        mov     rax, qword ptr [rbp - 16]
        # <---- Jump table using the resume index
        mov     rax, qword ptr [8*rax + .LJTI3_0] 
        jmp     rax
```

All functions are generated in `do_code_gen`. When an async function is being codegenned, it populates the `g->cur_async_switch_instr`, which is the jump table switch instruction above. Functions are codegenned 1 by 1 so the `g` var holds the current function information.

The first two cases in the jump table resume index are always 0: the function entry block, and 1: the not suspended panic block.
Cases. See above in function

LLVM Has to be keeping track of the number of cases in the LLVM switch instruction, but why does the constructor take a `num cases` argument? LLVM IR doesn't have that? https://llvm.org/docs/LangRef.html#switch-instruction

From the llvm source code, `NumCases` is a **hint** to the number of cases https://llvm.org/doxygen/Instructions_8h_source.html line 3237 (as of dec 2021)
```cpp
   /// Create a new switch instruction, specifying a value to switch on and a
   /// default destination. The number of additional cases can be specified here
   /// to make memory allocation more efficient. This constructor can also
   /// auto-insert before another instruction.
   SwitchInst(Value *Value, BasicBlock *Default, unsigned NumCases,
              Instruction *InsertBefore);
```

The `awaiter_frame` is populated when an async function is called normally in an async function, e.g.

```rust
async inner() {
  suspend {}
  return 0;
}

async outer() {
  var res = inner();
  return;
}
```

or when the frame is awaited.

```rust
async inner() {
  suspend {}
  return 0;
}

async outer() {
  var frame = async inner();

  await frame;

  return;
}
```

The zig codegen does an atomic check on the `awaiter_frame` pointer inside the frame struct when generating and `await` expression. It performs an atomic exhange between the `g->cur_frame_ptr` and `frame->awaiter_frame`. More specifically, it produces the llvm `LLVMAtomicRMWBinOpXchg`, docs here https://llvm.org/doxygen/group__LLVMCCoreTypes.html#gad8bf14715a5cff0222ad416f17132e51. This returns the value that was previously in `frame->awaiter_frame`.

If the result is 0, i.e. nothing was awaiting the frame, then it continues with the await instruction, and the `frame->awaiter_frame` was set to `g->cur_frame_ptr`. If the result is all 1s, then the await instruction returns immediately, as the function is already complete.

The return of an async function is special. If there is an `awaiter_frame`, it returns control back to that frame immediately identical to a `resume caller_frame`, otherwise, it suspends. In the return codegen of an async function, it again performs an atomic exchange with the `g->cur_async_awaiter_ptr` and `all_ones`. This puts all ones in the `cur_async_awaiter_ptr` which is why in the `await` statement codegen, `all_ones` indicates return immediately. If `g->cur_async_awaiter_ptr` was zero, then nobody is awaiting this function and it gens a void return. This is different from the other return blocks in that when there is no awaiter, then that means we entered the function via `resume`, which calls into the async function. `await` does not call into the function, it simply checks if the function is already done, and if so, copies the result and continues execution, otherwise, it marks the `frame->awaiter_frame` with the awaiting frame, and suspends the awaiter.

