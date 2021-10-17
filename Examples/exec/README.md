# exec()

## Reference
- https://man7.org/linux/man-pages/man3/exec.3.html

## Note
- The exec() family of functions replaces the current process image
       with a new process image.  The functions described in this manual
       page are layered on top of execve(2).  (See the manual page for
       execve(2) for further details about the replacement of the
       current process image.)
    - This means the code after exec() will not be execute since the program has already been replaced by the new executable.   

* -l: use in-place arguments to pass in the argument your executable need
* -v: use array to pass in the arguement your executable need
* -p: https://www.unix.com/unix-for-dummies-questions-and-answers/22699-execv-vs-execvp.html
    , in short, With execvp(), the first argument is a filename, or argv[0]

1. execlp
```c
execlp("cat","cat","test.txt",NULL); // p meand argv[0] is executable file name
execl("cat","test.txt",NULL); // no -p means no need to specify executalbe file name
```

2. execvp
```c
char* const arg[3] = {"cat","test.txt",NULL};
execvp(arg[0],arg); // pass in the argument with argument array
```