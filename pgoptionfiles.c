/*
  pgoptionfiles.c --  To get a list of the option files that MySQL or MariaDB Connector C actually uses

   Version: 0.3.0
   Last modified: August 1 2025

  Copyright (c) 2025 by Peter Gulutzan. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
  Package: https://github.com/pgulutzan/pgoptionfiles, maybe https://github.com/ocelot-inc/ocelotgui later
*/

/*
  WHY IT IS GOOD
    MySQL and MariaDB Connector C use different option files and the choices might be affected by SYSCONFDIR
    (whose value they don't show at runtime) and environment variables (which aren't known until runtime).
    For details see https://ocelot.ca/blog/blog/2025/07/16/options-for-mysql-or-mariadb-connector-c-libraries/
    "Options for MySQL or MariaDB Connector C libraries".
    So, tell this program what the Connector C library is, and it will tell you what option files the connector uses.
  HOW IT WORKS
    With ptrace().
    It opens the Connector C library and calls mysql_options() + mysql_real_connect() so the library reads the option files.
    (The mysql_real_connect() call is harmless, it is designed to fail.)
    The libraries are opening files with syscalls. It's possible to catch the arguments of the syscalls and filter the
    ones that contain "*my.cnf".
    Then pgoptionfiles outputs a list:
      (pgoptionfiles)
      etc/my.cnf
      /etc/mysql/my.cnf
      /etc/mysql/conf.d//my.cnf
      /etc/mysql/mariadb.conf.d//my.cnf
      /home/pgulutzan/.my.cnf
    The list includes default option files which the connector opened, i.e. fopen(filename) succeeded.
    The list includes files which the connector would have opened but couldn't, i.e. access(filename) failed.
    The list includes files which were !included in other if and only if a non-default build option is used.
    The list does not include duplicates.
    If errors occur, a message beginning with "Error: " will appear after "(pgoptionfiles)" on the same line.
  HOW TO BUILD IT
    You need post-2019 Linux, gcc, pgoptionfiles.c (this file), and pgoptionfiles.h.
    Ensure that the libdl library is accessible with an "-ldl" clause because there will be a dlopen() call.
    gcc -o pgoptionfiles pgoptionfiles.c -ldl
  HOW TO USE IT
    You just need to know library name = path of the Connector C library, whose name usually ends with ".so".
    You may find that pgfindlib https://github.com/pgulutzan/pgfindlib is useful for finding the library name.
    pgoptionfiles library-name
    ... Result will be either an error message or a list of the option files that the connector read or tried to read
  WHAT IT CALLS
    The library functions are mysql_init(), mysql_options(...MYSQL_READ_DEFAULT_GROUP ...),
    mysql_real_connect(... NULL ...).
  HOW IT CAN FAIL
    It can fail if for security ptrace() is disabled or limited for your system or for your privilege set.
    https://becomingahacker.org/a-comparative-overview-of-selinux-apparmor-yama-tomoyo-linux-and-smack-bf7f0a1789cf
    ... You can see by looking at this pgoptionfile.c code that nothing crooked goes on, it just sees syscalls
    ... of an open-source library.
    It can fail if the connector code changes, making some necessary assumptions invalid ...
      We assume that mysql_options() processing will not stop when an error e.g. bad syntax happens,
      If MySQL or MariaDB changes this behaviour then you won't get a full list but you'll get to see
      where the error happened, it will be the last file in the file list.
      We assume that mysql_real_connect() won't call any plugin because the arguments are for a server
      that won't exist and therefore there won't be a handshake (Quote from MySQL manual: "Plugins are called
      at some point during handshake ..."). If MySQL or MariaDB changes this behaviour or if a custom plugin
      activates early, pgoptionfiles could fail.
  PGOPTIONFILES_DELIMITER
    In the output list the default delimiter is \n, which is why file names are printed on separate lines.
    To change it to comma or colon or semicolon etc., compile with
    -DPGOPTIONFILES_DELIMITER="','" or -DPGOPTIONFILES_DELIMITER="':'" or -DPGOPTIONFILES_DELIMITER="';'" etc.
  PGOPTIONFILES_READ
    Default behaviour is that pgoptionfiles does not allow the connector to read the files.
    To change it, compile with -DPGOPTIONFILES_READ=1. The benefit is that the connector will see "!include ..."
    instructions and open those !included files too, therefore file_name_list will have not only the default
    option files but every file. The possible non-benefits are: MySQL Connector C will display error messages
    if e.g. !include file does not exist (before the first line that pgoptionfiles displays starting with "(pgoptionfiles)"),
    and a custom plugin might activate early.
  PGOPTIONFILES_INCLUDE_MYSQL
    By default the building does not require "#include <mysql.h>" because the only two items that pgoptionfiles needs,
    i.e. MYSQL struct and MYSQL_READ_DEFAULT_GROUP enum, can easily be specified in pgoptionfiles.h.
    However, anyone worried that Oracle will do an incompatible-ABI change may want building to require "#include <mysql.h>".
    If so: Ensure that either MySQL's or MariaDB's mysql.h is on the include path
    (it doesn't matter which, since MySQL and MariaDB define the two items similarly), e.g. on the developer's macine:
      for MySQL 8.3: export C_INCLUDE_PATH=/home/pgulutzan/mysql-8.3.0-linux-glibc2.28-x86_64/include
      for MariaDB 3.4.3: export C_INCLUDE_PATH=/home/pgulutzan/connector-c-3.4.3/usr/local/include/mariadb
    then say
    gcc -DPGOPTIONFILES_INCLUDE_MYSQL=1 -o pgoptionfiles pgoptionfiles.c -ldl
  PGOPTIONFILES_TRACEE_ONLY
    This is a debugging option, off (0) by default. If it is on (1), there is no ptrace() and no tracer.
    An example test case: build with -DPGOPTIONFILES_TRACEE_ONLY=1 -DPGOPTIONFILES_READ=1,
    strace 2>&1 ./pgoptionfiles library-name | grep my.cnf
    to check whether it displays the same file names.
  USE IN OCELOTGUI
    The intent is to use something like this in ocelotgui:
      FILE *fp= popen(./pgoptionfiles library_found_with_pgfindlib.so", "r");
      {
        while (fgets(file_name, sizeof(file_name), fp) != NULL)
        {
          skip-till-line-beginning with-"(pgoptionfiles)"-seen
          add-to-list-of-option-files
        }
        pclose(fp);
      }
    timed on developer's machine: 420 ms first time, 43 ms if loaded earlier, i.e. expensive for initialization
  EXAMPLES (DONE ON DEVELOPER'S MACHINE AFTER BUILDING WITH PGOPTIONFILES_READ=1)
    $ ./pgoptionfiles /home/pgulutzan/connector-c-3.4.3/usr/local/lib/mariadb/libmariadb.so
      (pgoptionfiles)
      /etc/my.cnf;/etc/mysql/my.cnf
      /etc/mysql/conf.d//my.cnf
      /etc/mysql/mariadb.conf.d//my.cnf
      /home/pgulutzan/.my.cnf
    $ ./pgoptionfiles /home/pgulutzan/mysql-8.3.0-linux-glibc2.28-x86_64/lib/libmysqlclient.so
      (pgoptionfiles)
      /etc/my.cnf
      /etc/mysql/my.cnf
      /usr/local/mysql/etc/my.cnf
      /home/pgulutzan/.my.cnf
*/

#include "pgoptionfiles.h" /* all #defines and function declarations */

int main(int argc, char **argv)
{
  char file_names_list[PGOPTIONFILES_MAX_FILE_NAMES_LIST_SIZE]= ""; 
  char error_list[4096]= "(pgoptionfiles)";
  int result_code= 0;
  if (argc < 2)
  {
    printf("(pgoptionfiles)Error: too few args. Say pgoptionfiles library-file\n");
    exit(1);
  }
#if (PGOPTIONFILES_TRACEE_ONLY == 1)
  pgoptionfiles_tracee(argv[1]);
#else
  pid_t pid;
  pid= fork();
  if (pid < 0) { printf("(pgoptionfiles)Error: fork() failed\n"); return -1; }
  if (pid == 0)
  {
    pgoptionfiles_tracee(argv[1]);
  }
  {
    result_code= pgoptionfiles_tracer(pid, file_names_list, error_list);
  }
  printf("%s\n", error_list);
  printf("%s\n", file_names_list);
#endif
  return result_code; /* program end */
}

/*
  ******************* TRACEE ***************
  Turn optimizing off because tracer might look for specific signals that an optimizer might decide are unnecessary.
  Turn warning off for -Wno-unused-result by letting fopen return a value.
*/
#pragma GCC push_options
#pragma GCC optimize ("O0")
void pgoptionfiles_tracee(const char *argv1)
{
#if (PGOPTIONFILES_TRACEE_ONLY == 0)
  ptrace((enum __ptrace_request)PTRACE_TRACEME, 0, NULL, NULL);
  if (raise(SIGSTOP))
    pgoptionfiles_tracee_error("Error: raise sigstop failed.");
#endif
  void *dlopen_handle= dlopen(argv1, RTLD_LAZY); /* argv[1] should be library file name */
  if (dlopen_handle == NULL)
    pgoptionfiles_tracee_error("Error: dlopen() failed --does library exist and is it Connector C?");
  typedef MYSQL*          (*tmysql_init)         (MYSQL *);
  tmysql_init t__mysql_init;
  t__mysql_init= (tmysql_init) dlsym(dlopen_handle, "mysql_init");
  if (dlerror() != 0)
    pgoptionfiles_tracee_error("Error: dlsym() failed for mysql_init() -- is this a Connector C library?");
  typedef int             (*tmysql_options)      (MYSQL *, enum mysql_option, const void *);
  tmysql_options t__mysql_options;
  t__mysql_options= (tmysql_options) dlsym(dlopen_handle, "mysql_options");
  if (dlerror() != 0)
    pgoptionfiles_tracee_error("Error: dlsym() failed for mysql_options() -- is this a Connector C library?");
  typedef MYSQL*          (*tmysql_real_connect) (MYSQL *, const char *,
                                              const char *, const char *, const char *,
                                              unsigned int, const char *, unsigned long);
  tmysql_real_connect t__mysql_real_connect;
  t__mysql_real_connect= (tmysql_real_connect) dlsym(dlopen_handle, "mysql_real_connect");
  if (dlerror() != 0)
    pgoptionfiles_tracee_error("Error: dlsym() failed for mysql_real_connect() -- is this a Connector C library?");
  MYSQL *mysql= NULL;
  mysql= t__mysql_init(mysql);
  if (!mysql)
    pgoptionfiles_tracee_error("Error: mysql_init() failed -- out of memory?");
  /* This tells the connector to try to open all option files, group name doesn't matter */
  if (t__mysql_options(mysql, MYSQL_READ_DEFAULT_GROUP, "client") == 1)
    pgoptionfiles_tracee_error("Error: mysql_options() failed -- bad syntax in an option file?");
  /* The actual reading takes place during mysql_real_connect, failure doesn't matter */
  if (t__mysql_real_connect(mysql, "localhost", "","", "", 3309, NULL, 0) != 0)
    pgoptionfiles_tracee_error("Error: mysql_real_connect() succeeded -- this is probably harmless.");
  /* skip mysql_close() */
  exit(EXIT_SUCCESS);
}

/*
  Pass: the message for an error in tracee()
  Do: a fake fopen() -- all tracee messages start with "Error: ", the tracer looks for openat that starts with "Error: " 
      + exit.
  Since real files don't have names with this format, EXIT_FAILURE is certain. But EXIT_SUCCESS is harmless.
*/
void pgoptionfiles_tracee_error(const char * message)
{
#if (PGOPTIONFILES_TRACEE_ONLY == 1)
  printf("%s\n", message);
  exit(EXIT_FAILURE);
#else
  if ((fopen(message, "r") == NULL) || (access(message, F_OK) == 0)) exit(EXIT_FAILURE);
  exit(EXIT_SUCCESS);
#endif
}

#pragma GCC pop_options

/*
  ******************* TRACER ***************
*/

/*
  These are the five syscalls that are monitored.
  Connector C only uses fopen() so 257 openat, but in case that changes we're ready for 2 open.
  Connector C also uses 21 access or 4 stat for default files, but not necessarily for !include files.
  Nobody uses 2 open or 6 lstat at this time but they're monitored in case that changes.
  There's no checking for fstat (which occurs but seems redundant) or for statat.
*/
#define SYSCALL_OPEN 2
#define SYSCALL_STAT 4
#define SYSCALL_LSTAT 6
#define SYSCALL_ACCESS 21
#define SYSCALL_OPENAT 257


#if (PGOPTIONFILES_TRACEE_ONLY == 0)
int pgoptionfiles_tracer(pid_t pid, char *file_names_list, char *error_list)
{
  int status= 0;
  int retcode= 0;

  /* In this loop, odd trace_number is entry and even trace_number (other than 0) is exit), we worry only about entry */
  /* (They alternate because there are no other choices because the seccomp flag is off.) */
  for (unsigned int trace_number= 0; ; ++trace_number)
  {
    if (trace_number > 0)
    {
      if (ptrace((enum __ptrace_request)PTRACE_SYSCALL, pid, NULL, NULL) < 0)
      {
        /* errno could be EPERM or ESRCH or EIO but those are impossible so don't bother to look, just end */
        retcode= -1;
        break;
      }
    }
    /* Wait for pid (tracee). First time will be wait for SIGSTOP, later times will be wait for SYSCALL result. */
    int waitpid_result= 0; /* can be -1 (error), 0 (not yet changed state), or > 0 (child process id) */
#if (PGOPTIONFILES_USE_TIMEOUT == 0) /* The usual setting since the timeout loop is a bit expensive */
      waitpid_result= waitpid(pid, &status, 0);
#else
    {
      for (useconds_t usleep_mikes= 125; usleep_mikes < 4096000; usleep_mikes= usleep_mikes * 2)
      {
        waitpid_result= waitpid(pid, &status, WNOHANG);
        if (waitpid_result != 0) break;
        usleep(usleep_mikes);
      }
      if (waitpid_result == 0) { /* i.e. if tracee still unresponsive after last usleep which was probably usleep(4096000) */
        kill(pid, SIGTERM);
        waitpid(pid, &status, 0); // Wait for the child to actually terminate
        strcat(error_list, "Error: waitpid timeout.");
        retcode= -1;
        break;
      }
    }
#endif
    if (waitpid_result < 0)
    {
      if (WIFEXITED(status)) retcode= 0;
      else { strcat(error_list, "Error: waitpid failed."); retcode= -2; }
      break;
    }
    if (WIFEXITED(status)) { retcode= 0; break; }
    if (trace_number == 0)
    {
      if (!WIFSTOPPED(status) || WSTOPSIG(status) != SIGSTOP) {
        kill(pid, SIGKILL);
        sprintf(error_list + strlen(error_list), "Error: waitpid status: %x.", status);
        retcode= -3;
        break;
      }
      continue; /* so next thing that happens with be PTRACE_SYSCALL */
    }
    if ((trace_number %2) == 1) /* like if ( ptrace_syscall_info op == PTRACE_SYSCALL_INFO_ENTRY) */
    {
      /* orig_rax or orig_eax should have the number of the system call, like  ptrace_syscall_info entry.nr */
      struct user_regs_struct registers;
      ptrace((enum __ptrace_request)PTRACE_GETREGS, pid, 0, &registers);
#ifdef __x86_64__
      size_t psi_entry_nr= registers.orig_rax;
#else
      size_t psi_entry_nr= registers.orig_eax;
#endif
      if ((psi_entry_nr == SYSCALL_OPENAT)
       || (psi_entry_nr == SYSCALL_ACCESS)
       || (psi_entry_nr == SYSCALL_STAT)
       || (psi_entry_nr == SYSCALL_OPEN)
       || (psi_entry_nr == SYSCALL_LSTAT))
      {
        char file_name[PATH_MAX];
        file_name[0]= PGOPTIONFILES_DELIMITER;
        int copy_result;
        if (psi_entry_nr == SYSCALL_OPENAT)
          copy_result= pgoptionfiles_copy_from_tracee(pid, file_name + 1, (const char *) registers.rsi); /* like entry.args[1]) */
        else /* SYSCALL_OPEN) | SYSCALL_ACCESS) ||  SYSCALL_STAT | SYSCALL_LSTAT */
          copy_result= pgoptionfiles_copy_from_tracee(pid, file_name + 1, (const char *) registers.rdi); /* like entry.args[0] */
        if (copy_result > 0)
        {
          /* if tracee has an error it calls fopen("Error: ...", "r"); */
          if (strncmp(file_name + 1, "Error: ", sizeof("Error: ") - 1) == 0)
          {
            strcat(error_list, file_name + 1);
            retcode= -6;
            break;
          }
          if (strstr(file_name, "my.cnf") == NULL) continue; /* option files always are named my.cnf or .my.cnf */
          int file_name_length= strlen(file_name);
#if (PGOPTIONFILES_READ == 0)
          /* Change filename's register to point to the trailing '\0' so the pass is empty string causing ENOENT. */
#ifdef __x86_64__
          if (psi_entry_nr == SYSCALL_OPENAT) registers.rsi+= file_name_length;
          else /* SYSCALL_OPEN | SYSCALL_ACCESS | SYSCALL_STAT | SYSCALL_LSTAT */ registers.rdi+= file_name_length;
#else
          if (psi_entry_nr == SYSCALL_OPENAT) registers.esi+= file_name_length;
          else /* SYSCALL_OPEN or SYSCALL_ACCESS of SYSCALL_STAT */ registers.edi+= file_name_length;
#endif
          ptrace((enum __ptrace_request)PTRACE_SETREGS, pid, 0, &registers);
#endif
          file_name[file_name_length]= PGOPTIONFILES_DELIMITER;
          file_name[file_name_length + 1]= '\0';
          const char *strstrx= strstr(file_names_list, file_name);
          if (strstrx != NULL) continue; /* ignore duplicate file name */
          if (strlen(file_names_list) + strlen(file_name) >= PGOPTIONFILES_MAX_FILE_NAMES_LIST_SIZE) break; /* overflow check */
          strcat(file_names_list, file_name);
        }
      }
    }
  }
  /* Eliminate initial or duplicate or trail delimiters */
  {
    int i_in= 0;
    int i_out= 0;
    for (;;)
    {
      char c= file_names_list[i_in++];
      if (c == '\0') break;
      if (c == '\0') { file_names_list[i_out]= '\0'; break; }
      if (file_names_list[i_in] == '\0') { file_names_list[i_out]= '\0'; break; }
      if (c != PGOPTIONFILES_DELIMITER) { file_names_list[i_out++]= c; continue; }
      if (i_out == 0) continue;
      if (file_names_list[i_in] == PGOPTIONFILES_DELIMITER) continue;
      file_names_list[i_out++]= c;
    }
  }
  return retcode;
}
#endif

#if (PGOPTIONFILES_TRACEE_ONLY == 0)
/*
  Pass: a tracee address, taken from an arg of the syscall, e.g. (char*) psi.entry.args[1]
  Do: make a copy as far as '\0' in a tracer string, e.g. file_name
  Return: -1 if unexpected error, or # of bytes 
  This is not the most efficient way to copy but it is the shortest code.
  PTRACE_PEEKDATA needs a "word", I'm assuming that's size_t so 4 bytes in 32-bit or 8-bits in 64-bit
  (elsewhere the test is #ifdef __x86_64__)
*/
int pgoptionfiles_copy_from_tracee(pid_t tracee_pid, char *dest, const char *src)
{
  if (src == NULL) return 0;
  int dest_offset= 0;
  for (int word_number= 0;; ++word_number)
  {
    size_t word= 0;
    uint8_t c= '\0';
    errno= 0;
    word= ptrace((enum __ptrace_request)PTRACE_PEEKDATA, tracee_pid, src + (sizeof(word) * word_number), NULL);
    if (errno != 0) break; /* though I don't know what would cause an error here */
    uint8_t *src_word_pointer;
    unsigned int src_word_offset= 0;
    src_word_pointer= (uint8_t *)&word;
    for (src_word_offset= 0; src_word_offset < sizeof(word); ++src_word_offset)
    {
      c= *(src_word_pointer + src_word_offset);
      if (dest_offset >= (PATH_MAX - 1)) c= '\0'; /* overflow check to force breaks, such path name invalid anyway */ 
      if (c == '\0') break;
      *(dest + dest_offset)= c;
      ++dest_offset;
    }
    if (c == '\0') break;
  }
  *(dest + dest_offset)= '\0';
  return dest_offset;
}
#endif
