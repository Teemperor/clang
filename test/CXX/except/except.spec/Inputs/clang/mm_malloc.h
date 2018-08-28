// missing throw() is allowed in this case as we are in a system header.
// This is a redeclaration possibly from glibc.
extern "C" void free(void *ptr);
