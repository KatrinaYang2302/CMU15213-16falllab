
/* 
 * tsh - A tiny shell program with job control
 * by Yufan (Katrina) Yang (yufany) @ CMU
 * Nov. 2nd 2016
 * @ Squirrel Hill, Pittsburgh
 * 
 *  
 */

#include "tsh_helper.h"
#include "csapp.h"

#define ZhuoLiang token

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);
void sigtstp_handler2(int sig);
void sigint_handler2(int sig);
//volatile sig_atomic_t curPid;//the pid currently processed in background

/*
 * <Write main's function header documentation. What does main do?>
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE_TSH];  // Cmdline for fgets
    bool emit_prompt = true;    // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    Dup2(STDOUT_FILENO, STDERR_FILENO);

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h':                   // Prints help message
            usage();
            break;
        case 'v':                   // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p':                   // Disables prompt printing
            emit_prompt = false;  
            break;
        default:
            usage();
        }
    }

    // Install the signal handlers
    Signal(SIGINT,  sigint_handler);   // Handles ctrl-c
    Signal(SIGTSTP, sigtstp_handler);  // Handles ctrl-z
    Signal(SIGCHLD, sigchld_handler);  // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler); 

    // Initialize the job list
    initjobs(job_list);

    // Execute the shell's read/eval loop
    while (true)
    {
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin))
        {
            app_error("fgets error");
        }

        if (feof(stdin))
        { 
            // End of file (ctrl-d)
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }
        
        // Remove the trailing newline
        cmdline[strlen(cmdline)-1] = '\0';
        
        // Evaluate the command line
        eval(cmdline);
        
        fflush(stdout);
    } 
    
    return -1; // control never reaches here
}


/* Handy guide for eval:
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg),
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.
 * Note: each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */

//parse the command line and execute it

void eval(const char *cmdline) 
{
    parseline_return parse_result;     
    struct cmdline_tokens token;
    sigset_t ourMask, tempMask, emptyMask;
    Sigemptyset(&tempMask);
    Sigaddset(&tempMask, SIGCHLD);
    Sigaddset(&tempMask, SIGINT);
    Sigaddset(&tempMask, SIGTSTP);
    Sigemptyset(&emptyMask);

    // Parse command line
    parse_result = parseline(cmdline, &ZhuoLiang);

    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY)
    {
        return;
    }
    else if(ZhuoLiang.builtin == BUILTIN_QUIT) exit(0);//if quit, just guadiao
    else if(ZhuoLiang.builtin == BUILTIN_JOBS){//list all the jobs
	int fd;
	if(ZhuoLiang.outfile != NULL) fd = Open(ZhuoLiang.outfile, O_WRONLY | O_CREAT, 0);
	else fd = STDOUT_FILENO;
	Sigprocmask(SIG_BLOCK, &tempMask, &ourMask);
	listjobs(job_list, fd);
	Sigprocmask(SIG_SETMASK, &ourMask, NULL);
	return;
    }
    else if(ZhuoLiang.builtin == BUILTIN_BG){//load a bkgd job
    	if(ZhuoLiang.argc < 2) return;//illegal inputt
	pid_t pID;
	int jID;
	struct job_t* tmpJ;
	Sigprocmask(SIG_BLOCK, &tempMask, &ourMask);
	if(ZhuoLiang.argv[1][0] == '%'){//if it is a jid
		jID = atoi(ZhuoLiang.argv[1]+1);
		tmpJ = getjobjid(job_list, jID);
		if(tmpJ == NULL) pID = 0;
		else pID = tmpJ -> pid;
	}
	else{//if it is a pid
		pID = (pid_t)(atoi(ZhuoLiang.argv[1]));
		tmpJ = getjobpid(job_list, pID);
		if(tmpJ == NULL) pID = 0;
		else {
			pID = tmpJ -> pid;
			jID = tmpJ -> jid;
		}
	}
	//if(pID != 0){
	//	tmpJ -> state = BG;
	//}
	//Sigprocmask(SIG_SETMASK, &ourmask, NULL);
	//if(pID == 0) return;
	//Sigprocmask(SIG_BLOCK, &tempmask, &ourmask);
	//if(pID > 0) printf("[%d] (%d) %s\n", jID, pID, tmpJ->cmdline);
	//Sigprocmask(SIG_SETMASK, &ourMask, NULL);
	//if(pID <= 0) return;
	if(pID > 0){//if no such jobs exist then ignore.
		if(kill(-pID, SIGCONT) >= 0){
			printf("[%d] (%d) %s\n", jID, pID, tmpJ->cmdline);
			tmpJ -> state = BG;
		}
	}
	Sigprocmask(SIG_SETMASK, &ourMask, NULL);
    }
    else if(ZhuoLiang.builtin == BUILTIN_FG){//load a frgd job
    	if(ZhuoLiang.argc < 2) return;//illegal inputt
	pid_t pID;
	int jID;
	struct job_t* tmpJ;
	Sigprocmask(SIG_BLOCK, &tempMask, &ourMask);
	if(ZhuoLiang.argv[1][0] == '%'){//jid
		jID = atoi(ZhuoLiang.argv[1]+1);
		tmpJ = getjobjid(job_list, jID);
		if(tmpJ == NULL) pID = 0;
		else pID = tmpJ -> pid;
	}
	else{//pid
		pID = (pid_t)(atoi(ZhuoLiang.argv[1]));
		tmpJ = getjobpid(job_list, pID);
		if(tmpJ == NULL) pID = 0;
		else {
			pID = tmpJ -> pid;
			jID = tmpJ -> jid;
		}
	}
	if(pID != 0){
		tmpJ -> state = FG;
	}
	
	//if(pID == 0) return;
	if(pID > 0){
		if(kill(-pID, SIGCONT) >= 0){
			tmpJ -> state = FG;
			//curPid = 0;
			//wait for the frgd process
			while(1){
				if(fgpid(job_list) != pID){
					break;
				}
				sigsuspend(&emptyMask);
			}
		}
	}

	Sigprocmask(SIG_SETMASK, &ourMask, NULL);
	
	return;
    }
    else{//not builtin
	Sigprocmask(SIG_BLOCK, &tempMask, &ourMask);
	//printf("%s %s\n", token.infile, token.outfile);
	pid_t pid = Fork();
	int jID;
	if(pid == 0){//cihld process
		int inFD, outFD;
		//if there is a file redirection
		if(ZhuoLiang.infile != NULL){
			inFD = Open(ZhuoLiang.infile, O_RDONLY, 0);
			if(inFD > 0) Dup2(inFD, STDIN_FILENO);
		}
		if(ZhuoLiang.outfile != NULL){
			//printf("1");
			outFD = Open(ZhuoLiang.outfile, O_WRONLY | O_CREAT, 0);
			if(outFD > 0) Dup2(outFD, STDOUT_FILENO);
		}
		
		Sigprocmask(SIG_SETMASK, &ourMask, NULL);

		Setpgid(0,0);//new process GROUP
		//register new signal handers
		Signal(SIGCHLD, SIG_DFL);
		Signal(SIGINT, sigint_handler2);
		Signal(SIGTSTP, sigtstp_handler2);
		//execute
		if(execve(ZhuoLiang.argv[0], ZhuoLiang.argv, environ) < 0){
			printf("%s: Command not found\n", ZhuoLiang.argv[0]);
			exit(0);
		}
		//close files
		if(inFD > 0) close(inFD);
		if(outFD > 0) close(outFD);
	}
	if(parse_result == PARSELINE_FG) addjob(job_list, pid, FG, cmdline);
	else addjob(job_list, pid, BG, cmdline);
	jID = pid2jid(job_list, pid);
	//curPid = 0;
	
	//Sigprocmask(SIG_SETMASK, &ourMask, NULL);
	if(parse_result == PARSELINE_BG) {//if bkgd, print then return
		printf("[%d] (%d) %s\n", jID, pid, cmdline);
	}
	else{//if frgd, wait for terminations
		while(1){
			//Sigprocmask(SIG_BLOCK, &tempMask, &ourMask);
			if(fgpid(job_list) != pid) break;
			//Sigprocmask(SIG_SETMASK, &ourMask, NULL);
			sigsuspend(&emptyMask);
		}
	}
	Sigprocmask(SIG_SETMASK, &ourMask, NULL);
	return;
    }
    
    return;
}

/*****************
 * Signal handlers
 *****************/

//hander for sigchild
//print the signaled child's info and wait in a loop
//including sttopped and terminated childs
void sigchld_handler(int sig) 
{
    sigset_t oldMask, newMask;
    Sigemptyset(&newMask);
    Sigaddset(&newMask, SIGCHLD);
    Sigaddset(&newMask, SIGINT);
    Sigaddset(&newMask, SIGTSTP);
    int oldErrno = errno;
    int status = 0;
    pid_t thisPid;
    while((thisPid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0){
	Sigprocmask(SIG_BLOCK, &newMask, &oldMask);
	struct job_t *termJob = getjobpid(job_list, thisPid);
	int jID = pid2jid(job_list, thisPid);
	if(WIFSTOPPED(status)){
		//if(WIFSIGNALED(status)){
			//printf("Job [%d] (%d) stopped by signal %d\n", jID, (int)thisPid, WSTOPSIG(status));
			Sio_puts("Job [");
			Sio_putl((long)jID);
			Sio_puts("] (");
			Sio_putl((long)thisPid);
			Sio_puts(") stopped by signal ");
			Sio_putl((long)WSTOPSIG(status));
			Sio_puts("\n");
		//}
		termJob -> state = ST;
	}
	else{
		if(WIFSIGNALED(status)){
			//printf("Job [%d] (%d) terminated by signal %d\n", jID, (int)thisPid, WTERMSIG(status));
			Sio_puts("Job [");
			Sio_putl((long)jID);
			Sio_puts("] (");
			Sio_putl((long)thisPid);
			Sio_puts(") terminated by signal ");
			Sio_putl((long)WTERMSIG(status));
			Sio_puts("\n");
		}
		deletejob(job_list, thisPid);
	}
	//curPid = thisPid;
	Sigprocmask(SIG_SETMASK, &oldMask, NULL);
    }
    errno = oldErrno;
    return;
}

//hander for sigint
//re-send the signal SIGINT to all frgd childs
//and return, do not terminate itself
void sigint_handler(int sig) 
{
    sigset_t oldMask, newMask;
    Sigemptyset(&newMask);
    Sigaddset(&newMask, SIGCHLD);
    Sigaddset(&newMask, SIGINT);
    Sigaddset(&newMask, SIGTSTP);
    int oldErrno = errno;
    Sigprocmask(SIG_BLOCK, &newMask, &oldMask);
    int curFgJob = fgpid(job_list);
    if(curFgJob > 0){//if there is at least 1 frgd childs
	//printf("%d\n", curFgJob);
	kill(-curFgJob, SIGINT);
    }
    Sigprocmask(SIG_SETMASK, &oldMask, NULL);
    errno = oldErrno;
    return;
}

//hander for sigtstp
//re-send the signal SIGTSTP to all frgd childs
//if there is no frgd childs then just returned
void sigtstp_handler(int sig) 
{
    sigset_t oldMask, newMask;
    Sigemptyset(&newMask);
    Sigaddset(&newMask, SIGCHLD);
    Sigaddset(&newMask, SIGINT);
    Sigaddset(&newMask, SIGTSTP);
    int oldErrno = errno;
    Sigprocmask(SIG_BLOCK, &newMask, &oldMask);
    int curFgJob = fgpid(job_list);
    if(curFgJob > 0){//if there is at least 1 frgd childs
	kill(-curFgJob, SIGTSTP);
    }
    Sigprocmask(SIG_SETMASK, &oldMask, NULL);
    errno = oldErrno;
    return;
}

//following are (restored) signale handers for the childs
void sigint_handler2(int sig){
	sigset_t oldMask, newMask;
	Sigemptyset(&newMask);
    	Sigaddset(&newMask, SIGCHLD);
   	Sigaddset(&newMask, SIGINT);
    	Sigaddset(&newMask, SIGTSTP);
	int jID;
	pid_t pid = getpid();
	Sigprocmask(SIG_BLOCK, &newMask, &oldMask);
	jID = pid2jid(job_list, pid);
	Sigprocmask(SIG_SETMASK, &oldMask, NULL);
	Sio_puts("Job [");
	Sio_putl((long)jID);
	Sio_puts("] (");
	Sio_putl((long)pid);
	Sio_puts(") terminated by signal ");
	Sio_putl((long)SIGINT);
	Sio_puts("\n");
	exit(0);
}

void sigtstp_handler2(int sig){
	sigset_t oldMask, newMask;
	Sigemptyset(&newMask);
    	Sigaddset(&newMask, SIGCHLD);
   	Sigaddset(&newMask, SIGINT);
    	Sigaddset(&newMask, SIGTSTP);
	int jID;
	pid_t pid = getpid();
	Sigprocmask(SIG_BLOCK, &newMask, &oldMask);
	jID = pid2jid(job_list, pid);
	Sigprocmask(SIG_SETMASK, &oldMask, NULL);
	Sio_puts("Job [");
	Sio_putl((long)jID);
	Sio_puts("] (");
	Sio_putl((long)pid);
	Sio_puts(") stopped by signal ");
	Sio_putl((long)SIGTSTP);
	Sio_puts("\n");
	pause();
}
