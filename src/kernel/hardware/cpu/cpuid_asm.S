.global cpuid_check
cpuid_check:
// We need to check whether CPUID is available on this machine. To do that, we try to invert the ID flag
// in EFLAGS. If we succeed, CPUID is available, otherwise it's not.
        pushf                  // push EFLAGS to stack
        pop %ecx               // pop EFLAGS off to ECX
        mov %ecx, %eax         // copy EFLAGS to eax
        xor $0x200000, %eax    // invert ID flag in EFLAGS
        push %eax              // push altered EFLAGS to stack
        popf                   // pop altered EFLAGS to EFLAGS, we now need to check whether the change succeeded
        pushf                  // push the (possibly) altered EFLAGS to stack
        pop %eax               // pop EFLAGS off to EAX, we now have: unaltered EFLAGS in ECX, (possibly) altered EFLAGS in EAX
        xor %ecx, %eax         // compare both EFLAGS
        ret                    // if not equal, a nonzero value is returned in EAX and CPUID is available

.global cpuid_call
cpuid_call:
// We want to call CPUID. The function to call is given as the 1st argument, the 2nd argument is a pointer where to store the result.
// Because this function is called from C, we need to create and destroy a proper stack frame. We also need to preserve ESI because
// we change it inside the function.
        push %ebp              // create stack frame (store old frame pointer,
        mov %esp, %ebp         // let new frame pointer point to TOS)
        push %esi              // save ESI so we can use it in the meanwhile

        mov 8(%ebp), %eax      // Transfer 1st argument (4 byte) to EAX. (%ebp) is old %ebp, 4(%ebp) is return address, 8(%ebp) is 1st argument.
        cpuid                  // perform CPUID call with the function specified in EAX, results are in EAX, EBX, ECX, EDX
        mov 12(%ebp), %esi     // save pointer (2nd argument) to ESI
        mov %eax, (%esi)       // store EAX to pointer,
        mov %ebx, 4(%esi)      // EBX to pointer + 4, ...
        mov %ecx, 8(%esi)
        mov %edx, 12(%esi)
        mov %esi, %eax         // return the result pointer

        pop %esi               // restore old ESI
        mov %ebp, %esp         // destroy stack frame (let TOS point to current frame pointer,
        pop %ebp               // restore old frame pointer)
        ret