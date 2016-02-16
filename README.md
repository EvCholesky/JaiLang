# JaiLang
Work in progress JAI compiler

Slowly building a LLVM frontend for Jon Blow's JAI language.

It currently can compile and statically link very simple programs. It doesn't do runtime code execution yet: the plan is to use LLVM's jit compiler.

New:
  * Codegen for simple pointers
  * String literals
  * Variadic functions (*cough* printf)
  * Switched to the C interface for LLVM.
  * Pre/post increment/decrement
  * While loops

Coming soon:
  * Arrays
  * For Loops.
  * default initialization of global variables
  * Structures

Known Issues:
  * Void pointers are not supported (llvm wants to codegen them as *u8)
  * No pointer arithmatic yet.

Deviations from current JAI syntax:
  * currently using @ for a dereference operator, rather than <<
  * still support single quote character literals. (will add #char once the single char is used elsewhere)
