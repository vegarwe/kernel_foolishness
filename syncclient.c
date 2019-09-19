//syncclient.c
//#gcc -o sycclient syncclient.c -pthread

#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include <pthread.h>
#include <string.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>

#define DEVICE_NAME "/dev/syncdevice"
#define REALTIMECLOCK "/dev/rtc"
#define THREAD_COUNT 3
#define TOKEN_MAX 10
#define TIMEFRAME 1//1 second

static int fd = 0, rtc_device = 0;
static int token = 1;
pthread_mutex_t _mutex;

//echo 42 > /dev/char_device
void* write_thread(void* data)
{
    struct rtc_time rtc_tm;

    int ret=0;
    char buff[100], timestamp[100];

    int p = (int)data;

    pthread_mutex_lock(&_mutex);

    while(token <= TOKEN_MAX)
    {
        memset(buff,0, 100);
        memset(timestamp,0, 100);

        sprintf(buff,"%d",token);
        ret = write(fd,buff, 5);

        /* Read the RTC time/date */
        if( rtc_device != -1)
        {

            memset ( &rtc_tm, 0, sizeof( struct rtc_time));
            ioctl(rtc_device, RTC_RD_TIME, &rtc_tm);

            /* print out the time from the rtc_tm variable */
            sprintf(timestamp, "%02d:%02d:%02d", rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);
        }
        printf("write_thread()::thread ID: %d Token = %s Timestamp = %s\n",p, buff, timestamp);

        //pthread_mutex_lock(&_mutex);

        token++;

        pthread_mutex_unlock(&_mutex);

        usleep(TIMEFRAME);

        pthread_mutex_lock(&_mutex);
    }

    pthread_mutex_unlock(&_mutex);

    pthread_exit(NULL);
}

//cat /dev/syncdevice
void* read_thread(void* data)
{
    struct rtc_time rtc_tm;
    int ret=0;
    char buff[100], timestamp[100];
    memset(buff,0, 100);

    //Returns 0 when there is nothing in the queue
    while (ret=read(fd,buff,10) != 0 )
    {
        buff[strlen(buff)]='\0';
        memset(timestamp,0, 100);

        /* Read the RTC time/date */
        if( rtc_device  != -1 )
        {
            memset ( &rtc_tm, 0, sizeof( struct rtc_time));
            ioctl(rtc_device, RTC_RD_TIME, &rtc_tm);

            /* print out the time from the rtc_tm variable */
            sprintf(timestamp, "%02d:%02d:%02d", rtc_tm.tm_hour, rtc_tm.tm_min,
                    rtc_tm.tm_sec);
        }


        printf("read_thread()::Token : %s Timestamp = %s\n",buff, timestamp);
        usleep(TIMEFRAME);
    }

    pthread_exit(NULL);
}


int main()
{
    int i;
    char timestamp[100];
    int retval;

    struct rtc_time rtc_tm;

    pthread_mutex_init(&_mutex,NULL);

    fd=open(DEVICE_NAME,O_RDWR);
    if( fd == -1)
    {
        printf("Unable to open %s.\n", DEVICE_NAME );
        return 1;
    }

    //RTC
    rtc_device = open (REALTIMECLOCK, O_RDONLY);
    if( rtc_device == -1)
    {
        printf("Unable to open rtc_device. Try using 'sudo'.\n");
    }

    if( fd )
    {
        //Write threads
        pthread_t write_threads[THREAD_COUNT];

        for(i=0;i<THREAD_COUNT;i++)
        {
            pthread_create(&write_threads[i], NULL, write_thread, (void*)(i+1));

        }

        //Read threads
        //Wait for 3 seconds
        usleep(TIMEFRAME * 3);

        pthread_t read_threads[THREAD_COUNT];

        for(i=0;i<THREAD_COUNT;i++)
        {
            pthread_create(&read_threads[i], NULL, read_thread, NULL);
        }

        //Wait all the threads
        for(i=0;i<THREAD_COUNT;i++)
        {
            pthread_join(write_threads[i], NULL);
            pthread_join(read_threads[i], NULL);

        }
        close(fd);
    }

    pthread_mutex_destroy(&_mutex);
    pthread_exit(NULL);

    close(rtc_device);
}

