// C program to implement
// the above approach
# include <sys/types.h>
# include <unistd.h>
# include <stdio.h>
# include <stdlib.h>
// Driver code
int main()
{
  int pid, shared_data = 69420;
  pid = fork();
  
  if (pid == 0) // fork() returns 0 when called by child process
  {
    printf ("Child : I am the child process\n");
    printf ("Child : Child’s PID: %d\n", getpid());
    printf ("Child : Parent’s PID: %d\n", getppid());
    printf ("Shared data address in child %p\n", &shared_data);
    shared_data += 1;
    printf ("Shared data %d\n", shared_data);
    printf ("Shared data address in child %p after\n", &shared_data);

    // for (int i = 0; i <10; i++) printf("Child printing %d\n", i);
  }
  else
  {
    printf ("Parent : I am the parent process\n");
    printf ("Parent : Parent’s PID: %d\n", getpid());
    printf ("Parent : Child’s PID: %d\n", pid);
    printf ("Shared data %d\n", shared_data);
    printf ("Shared data address in parent %p\n", &shared_data);
    // for (int i = 0; i <10; i++) printf("Parent printing %d\n", i);
    
  }
  printf ("Shared data %d @ PID: %d\n", shared_data, getpid());
}