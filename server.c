#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "/usr/include/mysql/mysql.h"

#define BUFSIZE 1024
#define SERVPORT 5090
#define GATEPORT 5089
#define GW_IN  1
#define GW_OUT 2
#define VALUE 0
#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1
#define R1_POUT 4
#define G1_POUT 17
#define BUFFER_MAX 3
#define DIRECTION_MAX 35
#define VALUE_MAX 30 

void* calc_routine(void *arg);
void* pass_routine(void *arg);
static int GPIOExport(int pin);
static int GPIOUnexport(int pin);
static int GPIODirection(int pin, int dir);
static int GPIOWrite(int pin, int value);


char addresslist[100];
int query(char *);
int ratecal();
int current_in = 0;
int current_out= 0;
int gate_out = 2;
int gate_in = 3;

int cal_thread_id;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int main()
{
	pthread_t cal_t,pass_t;

	if(-1 == GPIOExport(R1_POUT) || -1 == GPIOExport(G1_POUT))
		return(1);

	if(-1 == GPIODirection(R1_POUT,OUT) || -1 == GPIODirection(G1_POUT,OUT))
		return(2);

	if(-1 == GPIOWrite(R1_POUT,1) || -1 == GPIOWrite(G1_POUT,1))
		return(3);

	pthread_create(&cal_t, NULL, calc_routine, NULL);
	cal_thread_id = cal_t;
	
	//------------communication routine ------------------

	int serv_sock, clnt_sock, str_len;
	char message[BUFSIZE];
	struct sockaddr_in serv_addr, clnt_addr;
	int clnt_addr_size;

	serv_sock = socket(AF_INET,SOCK_STREAM,0);
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("192.168.0.7");
	serv_addr.sin_port = htons(SERVPORT);
	
	if( bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))== -1)
	{
		printf("error in binding");
		return -1;
	}

	if(listen(serv_sock, 5) == -1)
	{
		printf("error in listening");
		return -1;
	}
	clnt_addr_size = sizeof(clnt_addr);

	while(1)
	{
		printf("waiting for User...\n");
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
		pthread_create(&pass_t, NULL, pass_routine, (void *)&clnt_sock);
	}
	
	return 0;
}

int query(char *id)
{
	MYSQL *conn;
	MYSQL_RES *res;
	MYSQL_ROW row;
	char *server = "localhost";
	char *user ="root";
	char *password = "raspberry";
	char *database = "SYSTEM";
	char myquery[100];

	if( !(conn = mysql_init((MYSQL*)NULL)))
	{
		printf("conn init fail\n");
		return -1;
	}
	if(!mysql_real_connect(conn,server,user,password,NULL,3306,NULL,0))
	{
		printf("connection error\n");
		return -1;
	}
	if(mysql_select_db(conn,database) != 0)
	{
		mysql_close(conn);
		printf("db select fail\n");
		return -1;
	}
	sprintf(myquery,"UPDATE USERS SET fine = fine + 2500 where id = %s", id);
	mysql_query(conn,myquery);
	res = mysql_store_result(conn);
	return 0;
}

void* pass_routine(void *arg)
{
	int sock = *(int *)arg;
	char message[BUFSIZE];
	int retval, socklen, client_len;
	struct sockaddr_in clientaddr;
	int in_out, query_result;
	char user_id[30];
	struct hostent *host;

	client_len = sizeof(clientaddr);
	getpeername(sock,(struct sockaddr *)&clientaddr, &client_len);
	
	strcpy(addresslist, inet_ntoa (clientaddr.sin_addr) );


	printf("Connected from IP : %s\n", addresslist);
	while(1)
	{
		
		retval = read(sock,message,sizeof(message));
		if(retval < 0)
		{
			close(sock);
			break;
		}

		printf("##########GET message From GATE##########\n");
		
		sscanf(message,"%d,%s" , &in_out,user_id);
		if( ( query_result =  query(user_id)) == -1 )
			printf("query error\n");

		 if(in_out == GW_IN) // people in using gate
		 {
			pthread_mutex_lock(&mutex);
			
			
			if(current_in != 3 && current_out ==0) 
			{
				current_in++;
			}
			else if(current_in != 3 && current_out != 0)
			{
				current_in++;
				current_out = 0;
			}
			else if(current_in ==3 && current_out ==0) // continuous in of 4 people
			{
				if(gate_in < 4) // update gate
				{
					gate_in++;
					gate_out--;
				}

				pthread_kill(cal_thread_id,SIGILL); // give signal to calthread
				current_in = 0;
			}		
			pthread_mutex_unlock(&mutex);
		 }
		 else // OUT
		 {
			pthread_mutex_lock(&mutex);
		 	
			if(current_out != 3 && current_in == 0)
			{
				current_out++;
			}
			else if(current_out != 3 && current_in != 0)
			{
				current_out++;
				current_in = 0;
			}
			else if(current_out ==3 && current_in ==0) //continuous out of 4 people
			{

				if(gate_out < 4) //update gate
				{
					gate_out++;
					gate_in--;
				}
				pthread_kill(cal_thread_id,SIGILL); // give signal to calthread
				current_out = 0 ;
			}

			pthread_mutex_unlock(&mutex);
		 }

		 memset(message,0x00,sizeof(message));
		 memset(user_id, 0x00,sizeof(user_id));
	}
	pthread_exit(NULL);
}

void* calc_routine(void *arg)
{
	int sock;
	struct sockaddr_in serv_addr;
	char message[BUFSIZE];
	int str_len, signo, in,out;
 	int prev_in, prev_out; 

	//set signal mask for cal routine
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask,SIGILL);
	pthread_sigmask(SIG_BLOCK,&mask,0);
	
	sock = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("192.168.0.10");
	serv_addr.sin_port = htons(GATEPORT);

	while(1)
	{
		printf("sigwait\n");
		sigwait(&mask,&signo); // block until SIGILL CAME
		printf("signal received\n");
		pthread_mutex_lock(&mutex); // lock for global variable 
		in = gate_in;
		out = gate_out;
		pthread_mutex_unlock(&mutex);
		
		 
		sprintf(message,"%d,%d", gate_in, gate_out); // make gate info (in,out_)
		printf("########## MODIFY MESSAGE SEND ##########\n");
		sendto(sock, message, sizeof(message),0,(struct sockaddr *)&serv_addr, sizeof(serv_addr)); // send gate info to gate
		memset(message,0x00,sizeof(message)); // cleanup message
		

	}
	pthread_exit(NULL);
}

static int GPIOExport(int pin)
{
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/export", O_WRONLY);
   if (-1 == fd) 
   {
      fprintf(stderr, "Failed to open export for writing!\n");
      return(-1);
   }

   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return 0;
}

static int GPIOUnexport(int pin)
{
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/unexport", O_WRONLY);;
   if ( -1 == fd) 
   {
      fprintf(stderr, "Failed to open unexport for writing!\n");
      return(-1);
   }

   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);

   return(0);
}

static int GPIODirection(int pin, int dir)
{
   static const char s_directions_str[] ="in\0out";
   char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
   int fd;

   snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction",pin);

   fd = open(path, O_WRONLY);
   if( -1 == fd )
   {
      fprintf(stderr, "Failed to open gpio direction for writing!\n");
      return(-1);
   }

   if(-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2:3 ))
   {
      fprintf(stderr, "Failed to set direction!\n");
      return(-1);
   }

   close(fd);
   return(0);
}


static int GPIOWrite(int pin, int value)
{
   static const char s_values_str[] = "01";

   char path[VALUE_MAX];
   int fd;

   snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_WRONLY);
   if(-1 == fd)
   {
      fprintf(stderr, "Failed to open gpio value for writing!\n");
   }

   if(1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1))
   {
      fprintf(stderr, "Failed to write value!\n");
      close(fd);
      return(0);
   }
}
