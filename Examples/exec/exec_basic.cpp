#include <iostream>
#include <unistd.h>

/* exec is for calling executable */
int main(){
	// the first "cat" is the actual executable
	// the secomd "cat" is the argv[0], it's not fine if you want to put whatever in it
	// see here: https://unix.stackexchange.com/questions/187666/why-do-we-have-to-pass-the-file-name-twice-in-exec-functions
	std::cout << "using exec to execute executable: " << std::endl;
	
	execlp("cat","cat","test.txt",NULL); //works
	// execlp("cat","whatever","test.txt",NULL); //also works

	/*
	The exec() family of functions replaces the current process image
    with a new process image.  The functions described in this manual
    page are layered on top of execve(2).  (See the manual page for
    execve(2) for further details about the replacement of the
    current process image.)
	*/

	// this won't work since the process is being replace by the exec function
	// in other word, code exit with the executable exec() calls
	std::cout << "Code does not go here" << std::endl;
}