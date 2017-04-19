#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include "unistd.h"
#include "pthread.h"
#include <errno.h>
#include <signal.h>     //signal
#include <sys/file.h>   //flock
#include <arpa/inet.h>  //ntohl
#include <sys/prctl.h>
#include <semaphore.h>
#include <sys/time.h>

#define COUNT_PTHREADS    10   //
typedef struct _process_data_t {
        pthread_t thread_id[COUNT_PTHREADS];
} process_data_t;

static int g_running = 1;

static int g_index_queue = 0;
static pthread_mutex_t g_mutex;
static process_data_t process_data = {0};

static unsigned long ms_get_tick_count()
{
    struct timespec _t0;
    clock_gettime(CLOCK_MONOTONIC, &_t0);
    return _t0.tv_sec*1000+_t0.tv_nsec/1000/1000;
}

static void signal_handler(int signum)
{
    printf( "[%s] signal [%d] catched. pid=%d,ppid=%d\n", __func__, signum, getpid(), getppid());

    switch(signum){
    case SIGTERM:
        g_running = 0;
        printf( "[%s] running=0.\n", __func__);
        break;
    default:
        break;
    }
}

static void  _start_timer(){
	struct itimerval new_value;
	new_value.it_interval.tv_sec = 0;
	new_value.it_interval.tv_usec = 0;
	new_value.it_value.tv_sec = 2;
	new_value.it_value.tv_usec = 0;
    printf("\n\n[%s-%s(L:%d)](T:%ld): get pid=(%d),tid=[%lu], start timer (%d) seconds....\n\n", __FILE__,__func__, __LINE__,ms_get_tick_count(), (int)getpid(), pthread_self(), (int)new_value.it_value.tv_sec);

	setitimer(ITIMER_REAL, &new_value, NULL);
}

static void _callback_timeout()
{
    unsigned long clock = 0;
    int i = 0;
    float fv = 1.1;

    clock = ms_get_tick_count();
    
	_start_timer();

}
static void signal_alarm(int signum)
{
    int errno_saved = errno;
	fprintf(stderr, "[%s-%s(L:%d)](T:%ld): pid=(%d),get signum=[%d],current_errno=[%d]\n", __FILE__,__func__, __LINE__,ms_get_tick_count(), (int)getpid(), signum,errno_saved);

    if(SIGALRM==signum){
        _callback_timeout();
    }

}


static void* thread_process_data(void* param)
{
    
    int fd_sock = 0;
 	
    time_t t0 = 0;

    int i = 0;
    fd_set readset;
    int saved_errno = 0;
    int ret = 0;
    char thread_name[16]= "";
    int index = 0;
    struct itimerval new_value;
    sigset_t nset,oset;

#ifdef USE_PSELECT
	struct timespec ts;
#endif
	index = g_index_queue;
    snprintf(thread_name, sizeof(thread_name), "process_command_%d", index);
    prctl(PR_SET_NAME, (unsigned long)thread_name);
    printf(">>>> %s(%d): Enter thread-%d [%d->%d/%lu]. \n", __func__, __LINE__, index, getppid(), getpid(), pthread_self());
 
    pthread_mutex_unlock(&g_mutex);



    do{
		if(sigemptyset(&nset) == -1)
			perror("sigemptyet");
		if(sigaddset(&nset, SIGALRM) == -1)
			perror("sigaddset");
#ifdef BLOCK_IN_MAIN
		if (pthread_sigmask(SIG_UNBLOCK, &nset, &oset) != 0)
			printf("!! Set pthread mask failed\n");
#endif
        signal(SIGALRM, signal_alarm);
        
        //start timer
        _start_timer();

		t0 = ms_get_tick_count();
		printf("Test %d[T:%ld],tid=[%lu]-----\n", i++, t0 , pthread_self());

		while(g_running)
		{
			printf("\n\n[%s-%s(L:%d)](T:%ld): get pid=(%d),tid=[%lu], before select....\n\n", __FILE__,__func__, __LINE__,ms_get_tick_count(), (int)getpid() , pthread_self());

			FD_ZERO(&readset);
			FD_SET(fd_sock, &readset);
#ifdef USE_PSELECT
			ts.tv_sec = 10;
			ts.tv_nsec= 0;
			ret = pselect(fd_sock+1, &readset, NULL, NULL, &ts, &nset);
#else
			ret = select(fd_sock, &readset, NULL, NULL, 0);
#endif
			saved_errno = errno;
#ifdef USE_PSELECT
			printf("\n\n[%s-%s(L:%d)](T:%ld): pid=[%d],tid=[%lu], pselect() return=[%d], current errno=[%d]\n\n", __FILE__,__func__, __LINE__,ms_get_tick_count(), (int)getpid(), pthread_self(), ret, saved_errno );
		  
#else
			printf("\n\n[%s-%s(L:%d)](T:%ld): pid=[%d],tid=[%lu], select() return=[%d], current errno=[%d]\n\n", __FILE__,__func__, __LINE__,ms_get_tick_count(), (int)getpid(), pthread_self(), ret, saved_errno );

#endif
			
			if(ret<0){
				if(EINTR==saved_errno){ 
					continue;
				}
				if(0==saved_errno){
					continue;
				}

				perror("select");
				ret = -__LINE__;
				break;
			}

		}

    }while(0);


    printf("%s(): Leave thread [%lu]. \n", __func__, pthread_self());
    return NULL;
}


static int process_global_init(process_data_t *data)
{
    int ret = 0;
    int index = 0;

    sigset_t nset, oset;

    do {
        /* establish a handler to handle signal. */
        signal(SIGTERM, signal_handler);

		if(sigemptyset(&nset) == -1)
			perror("sigemptyet");
		if(sigaddset(&nset, SIGALRM) == -1)
			perror("sigaddset");

#ifdef BLOCK_IN_MAIN
		if (pthread_sigmask(SIG_BLOCK, &nset, &oset) != 0)
			printf("!! Set pthread mask failed\n");
#endif

        for(index=0; index<2; index++){
            g_index_queue = index;
            pthread_mutex_lock(&g_mutex);
            ret = pthread_create(&data->thread_id[index], NULL, thread_process_data, data);
            pthread_mutex_lock(&g_mutex);
            pthread_mutex_unlock(&g_mutex);
            if(ret) {
                perror("pthread_create");
                ret = -__LINE__;
                break;
            }
        }
        if(ret) break;
        
        //pthread_join(data->thread_id[0], NULL);

    } while(0);

    return ret;
}

static int process_global_run(process_data_t *data)
{
    int ret = 0;
    const unsigned int loop_interval = 5;
    struct timeval tv = {loop_interval, 0};
    fd_set readfds;
    int saved_errno = 0;

    while(g_running) {
		saved_errno = 0;
		fprintf(stderr, "[%s-%s(L:%d)](T:%ld): pid=(%d),restart\n", __FILE__,__func__, __LINE__,ms_get_tick_count(), (int)getpid());
 
        FD_ZERO(&readfds);
        tv.tv_sec = loop_interval;
        ret = select(0, &readfds, NULL, NULL, &tv);
        saved_errno = errno;
        fprintf(stderr, "[%s-%s(L:%d)](T:%ld): pid=(%d),select() return=[%d], current errno=[%d]\n", __FILE__,__func__, __LINE__,ms_get_tick_count(), (int)getpid(), ret, saved_errno);

        if (ret < 0){
            if(EINTR!=saved_errno) {
                perror("select");
                break;
            }
            continue;
        }
    }

    return ret;
}

static void process_global_deinit(process_data_t *data)
{


    int index = 0;
    for(index=0; index<COUNT_PTHREADS; index++){
        if(0==data->thread_id[index]) continue;

        int ret = pthread_kill(data->thread_id[index], 0);
        switch(ret){
        case ESRCH:
                printf("The thread [%lu] did not exists or already quit.\n", data->thread_id[index]);
                break;
        case EINVAL:
                printf("The signal is invalid.\n");
                break;
        default:
                printf("The thread [%lu] is alive.\n", data->thread_id[index]);
                ret = pthread_cancel(data->thread_id[index]);
                break;
        }
    }

}
////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
    int ret = 0;
       
    do {
		//start timer and pthread in process_global_init()
        ret = process_global_init((process_data_t *)&process_data);
        if(0!=ret) {
            break;
        }
        printf( "[%s] init done  pid=[%d], do process_global_run().\n", argv[0], (int)getpid());
        process_global_run((process_data_t *)&process_data);
    } while(0);

    process_global_deinit((process_data_t *)&process_data);

    printf("Daemon [%s] Exit Normally with pid [%d]\n", argv[0], getpid());
    return ret;
}
