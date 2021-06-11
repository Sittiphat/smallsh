#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>

#define MAX_CHARACTERS 2048
#define MAX_ARGS 512
#define MAX_PIDS 100

// struct sigaction SIGINTACTION = {0};
// struct sigaction SIGSTPACTION = {0};
int pid_count = 0;
pid_t pid_arr[MAX_PIDS];
int childStatus = 0;

int foreground_process;
int foreground_mode = false;

void printBackPID() {
  pid_t pid = getpid();
  printf("background pid is %d\n", pid);
  fflush(stdout);
}

// void handle_SIGINT(int signo) {
//     if (!foreground_mode) {
//         char* msg = "\nEntering foreground-only mode (& is now ignored)\n";
//         write(STDOUT_FILENO, msg, 50);
//         fflush(stdout);
//         foreground_process = true;
//         foreground_mode = true;
//     } else {
//         char* msg = "\nExiting foreground-only mode\n";
//         write(STDOUT_FILENO, msg, 30);
//         fflush(stdout);
//         foreground_process = false;
//         foreground_mode = false;
//     }
// }

void handle_SIGINT(int signo) {
  // kill(getpid(), SIGTERM);
  char * msg = "hh\n";
  write(STDOUT_FILENO, msg, 1);
  fflush(stdout);

}

void handle_SIGSTP(int signo) {
  if (!foreground_mode) {
    char * msg = "\nEntering foreground-only mode (& is now ignored)\n: ";
    write(STDOUT_FILENO, msg, 52);
    fflush(stdout);
    foreground_process = true;
    foreground_mode = true;
  } else {
    char * msg = "\nExiting foreground-only mode\n: ";
    write(STDOUT_FILENO, msg, 32);
    fflush(stdout);
    foreground_process = false;
    foreground_mode = false;
  }
}

void printStatus(int childStatus) {
  if (WIFEXITED(childStatus)) {
    printf("exit value %i\n", WEXITSTATUS(childStatus));
  }
}

void printTerminatedSig(int childStatus) {
  if (!WIFEXITED(childStatus)) {
    printf("terminated by signal %i\n", childStatus);
  }
}

void execProcess(char * cmd_args[], char * input_file, char * output_file, int * foreground_process, int * childStatus, struct sigaction * SIGINTACTION) {
  pid_t spawnPid = fork();
  switch (spawnPid) {
  case -1:
    printf("The shell could not find the command to run\n");
    fflush(stdout);
    * childStatus = 1;
    break;
  case 0:
    // In the foreground, child processes run default SIGINT so can ctrl + c
    if ( * foreground_process) {
      SIGINTACTION -> sa_handler = SIG_DFL;
      sigaction(SIGINT, SIGINTACTION, NULL);
    }

    // Putting all children PIDs here
    pid_arr[pid_count] = spawnPid;
    // In the child process
    pid_count++;

    if (input_file != NULL || ! * foreground_process) {

      // If during a background proccess user does not
      // redirect stdout, we redirect it automatically to /dev/null
      if (! * foreground_process) {
        input_file = "/dev/null";
      }

      int targetFD = open(input_file, O_RDONLY);
      if (targetFD == -1) {
        printf("cannot open %s for input\n", input_file);
        fflush(stdout);
        exit(1);
      }

      if (dup2(targetFD, 0) == -1) {
        perror("dup2");
        exit(1);
      }

      close(targetFD);
    }
    if (output_file != NULL || ! * foreground_process) {
      // If during a background proccess user does not
      // redirect stdout, we redirect it automatically to /dev/null
      if (! * foreground_process) {
        output_file = "/dev/null";
      }
      int targetFD = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);
      if (targetFD == -1) {
        printf("cannot open %s for input\n", output_file);
        fflush(stdout);
        exit(1);
      }

      if (dup2(targetFD, 1) == -1) {
        perror("dup2");
        exit(1);
      }

      close(targetFD);
    }

    // Replace the current program with "/bin/ls"
    if (execvp(cmd_args[0], cmd_args)) {
      printf("%s: no such file or directory\n", cmd_args[0]);
      fflush(stdout);
      exit(1);
    }
    // exec only returns if there is an error
    break;

  default:
    // In the parent process
    // Wait for child's termination
    if ( * foreground_process) {
      waitpid(spawnPid, childStatus, 0);
    } else {
      printf("background pid is %d\n", spawnPid);
      fflush(stdout);
    }

    break;
  }
}

int main(int argc, char * argv[]) {
  int did_change_arr = false;

  struct sigaction SIGINTACTION = { 0 };
  struct sigaction SIGSTPACTION = { 0 };

  const size_t BUFFER_SIZE = MAX_CHARACTERS;
  const size_t CHAR_BUFFER_SIZE = BUFFER_SIZE * sizeof(char);
  size_t user_input_size;
  char * input_buffer;
  // int foreground_process = true;
  char * cmd_args[MAX_CHARACTERS] = { NULL };
  // int childStatus = 0;

  // Setting up SIGINT (ctr + c)
  SIGINTACTION.sa_handler = SIG_IGN;
  SIGINTACTION.sa_flags = 0;
  // This is commented out so we can do ^C^Z
  // sigfillset(&(SIGINTACTION.sa_mask));
  sigaction(SIGINT, & SIGINTACTION, NULL);

  // Setting up SIGSTP (ctr + z)
  SIGSTPACTION.sa_handler = handle_SIGSTP;
  SIGSTPACTION.sa_flags = SA_RESTART; // Need to restart so we can change fore and back modes
  sigfillset( & (SIGSTPACTION.sa_mask));
  sigaction(SIGTSTP, & SIGSTPACTION, NULL);

  printf("smallsh\n");
  fflush(stdout);

  input_buffer = (char * ) malloc(CHAR_BUFFER_SIZE + 1);

  while (true) {
    foreground_process = true;
    printf(": ");
    fflush(stdout);

    fgets(input_buffer, CHAR_BUFFER_SIZE, stdin);

    // If it is a comment or nothing inputted, we automatically rerun the prompt
    if (input_buffer[0] == '#' || input_buffer[0] == '\n' || input_buffer[0] == '\0') {
      // Index is -1 so we know not to delete array of argunments later
      continue;
    }

    // Expansion of Variable $$ by first checking if the needle $$ is in the haystack
    while (strstr(input_buffer, "$$")) {
      // Replaces the dollar sign with a format specifier
      int i = 0;

      // We do this to not affect original buffer
      char * buffer2 = strdup(input_buffer);
      while (input_buffer[i] != '\0') {
        if (input_buffer[i] == '$' && input_buffer[i + 1] == '$') {
          input_buffer[i] = '%';
          input_buffer[i + 1] = 'd';
        }
        i++;

        // During each find of %d sprintf will replace it with the PID
        // After we are done, we copy it back to the original input buffer
        sprintf(buffer2, input_buffer, getpid());
        strcpy(input_buffer, buffer2);
      }
      free(buffer2);
    }

    // First command argument
    char * token = strtok(input_buffer, " \n");
    char * output_file = NULL;
    char * input_file = NULL;

    // We parse the input argument to check for redirection, background process
    int index = 0;
    while (token != NULL) {
      if (!strcmp(token, ">")) {
        token = strtok(NULL, " \n");
        output_file = strdup(token);

      } else if (!strcmp(token, "<")) {
        token = strtok(NULL, " \n");
        input_file = strdup(token);
      } else if (!strcmp(token, "&")) {
        foreground_process = false;
        if (foreground_mode) {
          foreground_process = true;
        }

        break;
      } else {
        did_change_arr = true;
        cmd_args[index] = strdup(token);
        index++;
      }

      // Incrementing the pointer set by our deliminator
      token = strtok(NULL, " \n");
    }

    // Executing In-Built or Other Commands
    // If our first CLA is either exit, cd, status then we
    // use our built in commands, where else we use exec()
    if (!strcmp(cmd_args[0], "exit") || !strcmp(cmd_args[0], "cd") || !strcmp(cmd_args[0], "status")) {
      if (!strcmp(cmd_args[0], "exit")) {
        // We kill any child process that is left before exiting
        if (!strcmp(cmd_args[0], "exit")) {
          for (int i = 0; i < pid_count; i++) {
            kill(pid_arr[i], SIGTERM);
          }

          exit(EXIT_SUCCESS);
        }

      } else if (!strcmp(cmd_args[0], "cd")) {
        char dir_path[100];
        char * path;

        // If it is only one word, this will return NULL
        // and go to else clause
        if (!strcmp(cmd_args[0], "cd") && cmd_args[1] == NULL) {
          chdir(getenv("HOME"));
        } else {
          chdir(cmd_args[1]);
        }
      } else if (!strcmp(cmd_args[0], "status")) {

        printStatus(childStatus);
        printTerminatedSig(childStatus);
      }

    } else {
      execProcess(cmd_args, input_file, output_file, & foreground_process, & childStatus, & SIGINTACTION);

    }

    // If the index is negative then there are not CLA to free
    if (did_change_arr) {
      // Freeing the command arguments array
      for (size_t i = 0; i < index; i++) {
        free(cmd_args[i]);
        cmd_args[i] = NULL;
      }
    }

  }

  return 0;
}
