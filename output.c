#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct machine {
    double* memory;
    bool*   allocated;
    int     capacity;
    int     stack_ptr;
    int     base_ptr;
} machine;


// Fatal error handler. Always exits program.
void panic(int code);

////////////////////////////////////////////////////////////////////////
////////////////////// Constructor and destructor //////////////////////
////////////////////////////////////////////////////////////////////////
// Create new virtual machine
machine *machine_new(int vars, int capacity);
// Free the virtual machine's memory. This is called at the end of the program.
void machine_drop(machine *vm);

void machine_load_base_ptr(machine *vm);
void machine_load_stack_ptr(machine *vm);
void machine_store_base_ptr(machine *vm);


/////////////////////////////////////////////////////////////////////////
///////////////////// Stack manipulation operations /////////////////////
/////////////////////////////////////////////////////////////////////////
// Push a number onto the stack
void machine_push(machine *vm, double n);
// Pop a number from the stack
double machine_pop(machine *vm);
// Add the topmost numbers on the stack
void machine_add(machine *vm);
// Subtract the topmost number on the stack from the second topmost number on the stack
void machine_subtract(machine *vm);
// Multiply the topmost numbers on the stack
void machine_multiply(machine *vm);
// Divide the second topmost number on the stack by the topmost number on the stack
void machine_divide(machine *vm);


/////////////////////////////////////////////////////////////////////////
///////////////////// Pointer and memory operations /////////////////////
/////////////////////////////////////////////////////////////////////////
// Pop the `size` parameter off of the stack, and return a pointer to `size` number of free cells.
int machine_allocate(machine *vm);
// Pop the `address` and `size` parameters off of the stack, and free the memory at `address` with size `size`.
void machine_free(machine *vm);
// Pop an `address` parameter off of the stack, and a `value` parameter with size `size`.
// Then store the `value` parameter at the memory address `address`.
void machine_store(machine *vm, int size);
// Pop an `address` parameter off of the stack, and push the value at `address` with size `size` onto the stack.
void machine_load(machine *vm, int size);

void prn(machine *vm);
void prs(machine *vm);
void prc(machine *vm);
void prend(machine *vm);
void getch(machine *vm);


///////////////////////////////////////////////////////////////////////
///////////////////////////// Error codes /////////////////////////////
///////////////////////////////////////////////////////////////////////
const int STACK_HEAP_COLLISION = 1;
const int NO_FREE_MEMORY       = 2;
const int STACK_UNDERFLOW      = 3;

void panic(int code) {
    printf("panic: ");
    switch (code) {
        case 1: printf("stack and heap collision during push"); break;
        case 2: printf("no free memory left"); break;
        case 3: printf("stack underflow"); break;
        default: printf("unknown error code");
    }
    printf("\n");
    exit(code);
}

machine *machine_new(int vars, int capacity) {
    machine *result = malloc(sizeof(machine));
    result->capacity  = capacity;
    result->memory    = malloc(sizeof(double) * capacity);
    result->allocated = malloc(sizeof(bool)   * capacity);
    printf("ALLOC %p\n", (void*)result->allocated);
    result->stack_ptr = 0;
    int i;
    for (i=0; i<capacity; i++) {
        result->memory[i] = 0;
        result->allocated[i] = false;
    }

    for (i=0; i<vars; i++)
        machine_push(result, 0);

    result->base_ptr = 0;

    return result;
}

void machine_dump(machine *vm) {
    int i;
    printf("stack: [ ");
    for (i=0; i<vm->stack_ptr; i++)
        printf("%g ", vm->memory[i]);
    for (i=vm->stack_ptr; i<vm->capacity; i++)
        printf("  ");
    printf("]\nheap:  [ ");
    for (i=0; i<vm->stack_ptr; i++)
        printf("  ");
    for (i=vm->stack_ptr; i<vm->capacity; i++)
        printf("%g ", vm->memory[i]);
    printf("]\nalloc: [ ");
    for (i=0; i<vm->capacity; i++)
        printf("%d ", vm->allocated[i]);
    printf("]\n");
    int total = 0;
    for (i=0; i<vm->capacity; i++)
        total += vm->allocated[i];
    printf("STACK SIZE    %d\n", vm->stack_ptr);
    printf("TOTAL ALLOC'D %d\n", total);
}

void machine_drop(machine *vm) {
    machine_dump(vm);
    printf("TRY FREED MEM\n");
    free(vm->memory);
    printf("FREED MEM\n");
    printf("TRY FREED ALLOC %p\n", (void*)vm->allocated);
    free(vm->allocated);
    printf("FREED ALLOC\n");
}

void machine_load_base_ptr(machine *vm) {
    machine_push(vm, vm->base_ptr);
}

void machine_establish_stack_frame(machine *vm, int arg_size, int local_scope_size) {
    double *args = malloc(arg_size * sizeof(double));
    for (int i=0; i<arg_size; i++)
        args[i] = machine_pop(vm);

    machine_push(vm, vm->base_ptr);
    vm->base_ptr = vm->stack_ptr;

    for (int i=0; i<local_scope_size; i++)
        machine_push(vm, 0);

    // for (int i=0; i<arg_size; i++)
    //     machine_push(vm, args[i]);
    for (int i=0; i<arg_size; i++)
        machine_push(vm, args[i]);
    free(args);
    // printf("BEGIN BASE PTR %d\n", vm->base_ptr);
}

void machine_end_stack_frame(machine *vm, int local_scope_size, int return_size) {
    double *return_val = malloc(return_size * sizeof(double));
    for (int i=return_size-1; i>=0; i--)
        return_val[i] = machine_pop(vm);

    for (int i=0; i<local_scope_size; i++)
        machine_pop(vm);
    
    vm->base_ptr = machine_pop(vm);
    for (int i=0; i<return_size; i++)
        machine_push(vm, return_val[i]);
    // for (int i=return_size-1; i>=0; i--)
    //     machine_push(vm, return_val[i]);

    free(return_val);
    // printf("END BASE PTR %d\n", vm->base_ptr);
}

void machine_push(machine *vm, double n) {
    if (vm->allocated[vm->stack_ptr])
        panic(STACK_HEAP_COLLISION);
    vm->memory[vm->stack_ptr++] = n;
}

double machine_pop(machine *vm) {
    if (vm->stack_ptr == 0) {
        panic(STACK_UNDERFLOW);
    }
    double result = vm->memory[vm->stack_ptr-1];
    vm->memory[--vm->stack_ptr] = 0;
    return result;
}

int machine_allocate(machine *vm) {    
    int i, size=machine_pop(vm), addr=0, consecutive_free_cells=0;
    for (i=vm->capacity-1; i>vm->stack_ptr; i--) {
        if (!vm->allocated[i]) consecutive_free_cells++;
        else consecutive_free_cells = 0;

        if (consecutive_free_cells == size) {
            addr = i;
            break;
        }
    }

    if (addr <= vm->stack_ptr)
        panic(NO_FREE_MEMORY);
    
    for (i=0; i<size; i++)
        vm->allocated[addr+i] = true;

    machine_push(vm, addr);
    return addr;
}

void machine_free(machine *vm) {
    int i, addr=machine_pop(vm), size=machine_pop(vm);

    for (i=0; i<size; i++) {
        vm->allocated[addr+i] = false;
        vm->memory[addr+i] = 0;
    }
}

void machine_store(machine *vm, int size) {
    int i, addr=machine_pop(vm);

    for (i=size-1; i>=0; i--) vm->memory[addr+i] = machine_pop(vm);
}

void machine_load(machine *vm, int size) {
    int i, addr=machine_pop(vm);

    for (i=0; i<size; i++) machine_push(vm, vm->memory[addr+i]);
}

void machine_add(machine *vm) {
    machine_push(vm, machine_pop(vm) + machine_pop(vm));
}

void machine_subtract(machine *vm) {
    double b = machine_pop(vm);
    double a = machine_pop(vm);
    machine_push(vm, a-b);
}

void machine_multiply(machine *vm) {
    machine_push(vm, machine_pop(vm) * machine_pop(vm));
}

void machine_divide(machine *vm) {
    double b = machine_pop(vm);
    double a = machine_pop(vm);
    machine_push(vm, a/b);
}

void machine_sign(machine *vm) {
    double x = machine_pop(vm);
    if (x >= 0) {
        machine_push(vm, 1);
    } else {
        machine_push(vm, -1);
    }
}
void prn(machine *vm) {
    double n = machine_pop(vm);
    printf("%g", n);
}

void prs(machine *vm) {
    double addr = machine_pop(vm);
    int i;
    for (i=addr; vm->memory[i]; i++) {
        printf("%c", (char)vm->memory[i]);
    }
}

void prc(machine *vm) {
    double n = machine_pop(vm);
    printf("%c", (char)n);
}

void prend(machine *vm) {
    printf("\n");
}

void getch(machine *vm) {
    char ch = getchar();
    if (ch == '\r') {
        ch = getchar();
    }
    machine_push(vm, ch);
}

void fn0(machine* vm);
void fn1(machine* vm);
void fn2(machine* vm);
void fn3(machine* vm);
void fn4(machine* vm);
void fn5(machine* vm);
void fn6(machine* vm);
void fn7(machine* vm);
void fn8(machine* vm);
void fn9(machine* vm);
void fn10(machine* vm);
void fn0(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
machine_load(vm, 1);
machine_push(vm, 1);
machine_add(vm);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
machine_store(vm, 1);
machine_end_stack_frame(vm, 2, 0);
}
void fn2(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
prs(vm);
machine_end_stack_frame(vm, 2, 0);
}
void fn3(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
fn2(vm);
prend(vm);
machine_end_stack_frame(vm, 2, 0);
}
void fn4(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
prn(vm);
machine_end_stack_frame(vm, 2, 0);
}
void fn5(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
fn4(vm);
prend(vm);
machine_end_stack_frame(vm, 2, 0);
}
void fn6(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
prc(vm);
machine_end_stack_frame(vm, 2, 0);
}
void fn7(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
fn6(vm);
prend(vm);
machine_end_stack_frame(vm, 2, 0);
}
void fn8(machine* vm) { machine_establish_stack_frame(vm, 1, 4);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
machine_push(vm, 2);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_push(vm, 3);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 2);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
while (machine_pop(vm)) {
machine_push(vm, 116);
fn6(vm);
machine_push(vm, 114);
fn6(vm);
machine_push(vm, 117);
fn6(vm);
machine_push(vm, 101);
fn6(vm);
machine_push(vm, 0);
machine_push(vm, 2);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 0);
machine_push(vm, 3);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 2);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
}
machine_push(vm, 3);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
while (machine_pop(vm)) {
machine_push(vm, 102);
fn6(vm);
machine_push(vm, 97);
fn6(vm);
machine_push(vm, 108);
fn6(vm);
machine_push(vm, 115);
fn6(vm);
machine_push(vm, 101);
fn6(vm);
machine_push(vm, 0);
machine_push(vm, 2);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 0);
machine_push(vm, 3);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 3);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
}
machine_end_stack_frame(vm, 4, 0);
}
void fn9(machine* vm) { machine_establish_stack_frame(vm, 1, 2);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
fn8(vm);
prend(vm);
machine_end_stack_frame(vm, 2, 0);
}
void fn10(machine* vm) { machine_establish_stack_frame(vm, 0, 1);
getch(vm);
machine_end_stack_frame(vm, 1, 1);
}
void fn1(machine* vm) { machine_establish_stack_frame(vm, 0, 2);
machine_push(vm, 0);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_store(vm, 1);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
machine_push(vm, 10);
machine_subtract(vm);
machine_sign(vm);
machine_push(vm, 1);
machine_subtract(vm);
machine_push(vm, -2);
machine_divide(vm);
while (machine_pop(vm)) {
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
fn5(vm);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
fn0(vm);
machine_push(vm, 1);
machine_load_base_ptr(vm);
machine_add(vm);
machine_load(vm, 1);
machine_push(vm, 10);
machine_subtract(vm);
machine_sign(vm);
machine_push(vm, 1);
machine_subtract(vm);
machine_push(vm, -2);
machine_divide(vm);
}
machine_end_stack_frame(vm, 2, 0);
}
int main() {
machine *vm = machine_new(0, 128);
fn1(vm);

machine_drop(vm);
return 0;
}