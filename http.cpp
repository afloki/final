#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sys/sendfile.h>
#include <arpa/inet.h>

#include <iostream>
#include <algorithm>
#include <set>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string>
// для многопоточности 
#include <omp.h>
using namespace std;

#define MAX_EVENTS 32

char * host;	// для хоста
char * port;	// для номера порта
char * dir;	// для имени директория 

// за основу возьмем эхо-сервер на epoll  с set_non_block

// функция переводит сокет в неблокирующий режим 
int set_nonblock(int fd);

void parce_from_http_get_request(std::string& parce, const char* buf, ssize_t len)
{
	std::string request(buf, len);
	std::string s1(" ");
    	std::string s2("?");

    	// "GET "
    	std::size_t pos1 = 4;

    	std::size_t pos2 = request.find(s2, 4);
    	if (pos2 == std::string::npos)
    	{
        	pos2 = request.find(s1, 4);
    	}

    	parce = request.substr(4, pos2 - 4);
}

// генерируем ответ cервера
void send_to_http(int des, char * buf, int siz, string dir)
{
    	// process http request, extract file path
    	std::string path;
    	parce_from_http_get_request(path, buf, siz);

    	// if path exists open and read file
    	std::string full_path = std::string(dir) + path;

    	char reply[2048];	
    	if (access(full_path.c_str(), F_OK) != -1)
    	{
        	// file exists, get its size
        	int fd = open(full_path.c_str(), O_RDONLY);
        	int sz = lseek(fd, 0, SEEK_END);;

        	sprintf(reply, "HTTP/1.0 200 OK\r\n"
        	               "Content-Type: text/html\r\n"
        	               "Content-length: %d\r\n"
        	               "Connection: close\r\n"
        	               "\r\n", sz);

        	ssize_t send_ret = send(des, reply, strlen(reply), MSG_NOSIGNAL);

        	off_t offset = 0;
        	while (offset < sz)
        	{
        	    // think not the best solution
        	    offset = sendfile(des, fd, &offset, sz - offset);
        	}

        	close(fd);
    	}
    	else
    	{
        	strcpy(reply, "HTTP/1.0 404 Not Found\r\n"
        	              "Content-Type: text/html\r\n"
        	              "Content-length: 107\r\n"
        	              "Connection: close\r\n"
        	              "\r\n");

        	ssize_t send_ret = send(des, reply, strlen(reply), MSG_NOSIGNAL);
        	strcpy(reply, "<html>\n<head>\n<title>Not Found</title>\n</head>\r\n");
       	 	send_ret = send(des, reply, strlen(reply), MSG_NOSIGNAL);
        	strcpy(reply, "<body>\n<p>404 Request file not found.</p>\n</body>\n</html>\r\n");
        	send_ret = send(des, reply, strlen(reply), MSG_NOSIGNAL);
    	}
}

int main(int argc, char ** argv)
{

	// создаем демона 	
	if (daemon(0, 0) == -1)
    	{
    		std::cout << "daemon error" << std::endl;
   		exit(1);
   	}
	

	// парсим командную строку вида
	//	 /home/box/final/final -h <ip> -p <port> -d <directory>
	int des;
	while((des = getopt(argc, argv, "h:p:d:")) != -1)
	{
		switch(des)
		{
			case 'h': host = optarg; break;
			case 'p': port = optarg; break;
			case 'd': dir = optarg; break;
			default:
				printf("Enter error -h <host> -p <port> -d <directory>.\n");
				exit(1);
		}
	}	

   	if (host == 0 || port == 0 || dir == 0)
    	{
        	printf("Usage: %s -h <host> -p <port> -d <folder>\n", argv[0]);
        	exit(1);
    	}
	

	// создаем сервер
	int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	struct sockaddr_in SockAddr;
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_port = htons(atoi(port));
	//SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	if (inet_pton(AF_INET, host, &(SockAddr.sin_addr.s_addr)) != 1)
    	{
        	printf("inet_aton error\n");
        	exit(2);
    	}	

	bind(MasterSocket, (struct sockaddr *)(&SockAddr), sizeof(SockAddr));
	// переводим сокет в неблокирующий режим
	set_nonblock(MasterSocket);
	listen(MasterSocket, SOMAXCONN);
	
	int EPoll = epoll_create1(0);
	struct epoll_event Event;
	Event.data.fd = MasterSocket;
	Event.events = EPOLLIN;
	
	epoll_ctl(EPoll, EPOLL_CTL_ADD, MasterSocket, &Event);	
	
	while(true)
	{
		struct epoll_event Events[MAX_EVENTS];
		int N = epoll_wait(EPoll, Events, MAX_EVENTS, -1);
		
		omp_set_dynamic(0);      // запретить библиотеке openmp менять число потоков во время исполнения
 		omp_set_num_threads(4); // установить число потоков в 4

#pragma omp parallel
{
		for(unsigned int i = 0; i < N; ++i)
		{
	#pragma omp single
	{
			if(Events[i].data.fd == MasterSocket)
			{
				int SlaveSocket = accept(MasterSocket, 0, 0);
				set_nonblock(SlaveSocket);
				struct epoll_event Event;
				Event.data.fd = SlaveSocket;
				Event.events = EPOLLIN;
				epoll_ctl(EPoll, EPOLL_CTL_ADD, SlaveSocket, &Event);
			}
			else{
				static char Buffer[1024];
				int RecvResult = recv(Events[i].data.fd, Buffer, 1024, MSG_NOSIGNAL);
				
				if((RecvResult == 0) && (errno != EAGAIN))
				{
					shutdown(Events[i].data.fd, SHUT_RDWR);
					close(Events[i].data.fd);
				}
				else if(RecvResult > 0)
				{

					send_to_http(Events[i].data.fd, Buffer, RecvResult, dir);
				}
			}
		}
	}
}
	}	

	return 0;
}


// функция переводит сокет в неблокирующий режим
int set_nonblock(int fd)
{
	int flags;
#if defined(O_NONBLOCK)
	if(-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	flags = 1;
	return ioctl(fd, FOIBOI, &flags);
#endif
}
