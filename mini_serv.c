#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

void fatal_error() {
	write(2, "Fatal error\n", 12);
	exit(1);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

int max_fd, sockfd, client_ids[1024], next_id;
char *client_buf[1024];
fd_set read_set, current_set;

void broadcast(int except, char *msg)
{
	for(int fd = 0; fd <= max_fd; fd++){
		if(FD_ISSET(fd, &current_set) && fd != except && fd != sockfd){
			send(fd , msg, strlen(msg), 0);
		}
	}
}

void add_client()
{
	int peticion = accept(sockfd,0,0);
	char msg[100];
	if(peticion < 0)
		return;
	if(peticion > max_fd)
		max_fd = peticion;
	client_ids[peticion] = next_id++;
	client_buf[peticion] = 0;
	FD_SET(peticion, &current_set);
	sprintf(msg, "server: client %d just arrived\n", client_ids[peticion]);
	broadcast(peticion, msg);
}

void remove_client(int fd)
{
	char msg[100];
	sprintf(msg, "server: client %d just left\n", client_ids[fd]);
	broadcast(fd, msg);
	FD_CLR(fd, &current_set);
	close(fd);
	free(client_buf[fd]);
	client_buf[fd] = 0;
}

void send_message(int fd)
{
	char *msg, *tmp;
	while(extract_message(&client_buf[fd], &msg))
	{
		tmp = malloc(strlen(msg) + 50);
		if(!tmp)
			fatal_error();
		sprintf(tmp, "client %d: %s", client_ids[fd], msg);
		broadcast(fd, tmp);
		free(msg);
		free(tmp);
	}
}

int main(int argc, char **argv) {

	struct sockaddr_in servaddr;
	char buffer[1001];

	if(argc != 2){
		write(2,"Wrong number of arguments\n", 26);
		exit(1);
	} 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		fatal_error();
	}

	max_fd = sockfd;
	FD_ZERO(&current_set);
	FD_SET(sockfd, &current_set);

	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
  
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		fatal_error();
	} 
	if (listen(sockfd, 10) != 0) {
		fatal_error(); 
	}
	while (1) {
		read_set = current_set;
		if (select(max_fd + 1, &read_set, 0, 0, 0) < 0) continue;
		
		for (int fd = 0; fd <= max_fd; fd++) {
			if (!FD_ISSET(fd, &read_set)) continue;
			
			if (fd == sockfd) {
				add_client();
			} else {
				int ret = recv(fd, buffer, 1000, 0);
				if (ret <= 0)
					remove_client(fd);
				else {
					buffer[ret] = 0;
					client_buf[fd] = str_join(client_buf[fd], buffer);
					send_message(fd);
				}
			}
		}
	}
}