// Declare 'free' like glibc with a empty exception specifier.
extern "C" void free(void *ptr) throw();
