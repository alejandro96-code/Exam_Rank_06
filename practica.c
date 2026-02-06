#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

void fatal_error()
{
	write(1, "Fatal error\n" , 12);
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

int max_fd = 0;
int sockfd = 0;
fd_set read_set, write_set, current_set;

void envio_general(int envio_fd, char *msg)
{
for (int fd = 0; fd <= max_fd; fd++)
	{
		if (FD_ISSET(fd, &current_set) && fd != envio_fd && fd != sockfd)
			send(fd, msg, strlen(msg), 0);
	}
}

int clients_ids[1024];
char *clients_bufs[1024];
int next_id = 0;

void anadir_cliente(int sockfd)
{
	int peticion = accept(sockfd, NULL, NULL);
	char msg[100];

	if(peticion < 0)
		return;
	if(peticion > max_fd)
		max_fd = peticion;
	
	clients_ids[peticion] = next_id++;
	clients_bufs[peticion] = NULL;
	FD_SET(peticion, &current_set);
	sprintf(msg, "server: client %d just arrived\n", clients_ids[peticion]);
	envio_general(peticion, msg);
}

void eliminar_cliente(int fd)
{
	char msg[100];
	sprintf(msg, "server: client %d just left\n", clients_ids[fd]);
	envio_general(fd, msg);

	FD_CLR(fd, &current_set);
	close(fd);
	free(clients_bufs[fd]);
	clients_bufs[fd] = NULL;
}

void enviar_mensaje(int fd)
{
	char *msg;
	char buffer[200];
	while(extract_message(&clients_bufs[fd], &msg))
	{
		sprintf(buffer, "client %d: %s", clients_ids[fd], msg);
		envio_general(fd, buffer);
		free(msg);
	}
}

int main(int argc, char **argv)
{
	int port;
	struct sockaddr_in servaddr;
	char buffer[1001];

	if (argc != 2)
	{
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}

	port = atoi(argv[1]);

	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal_error();

	max_fd = sockfd;
	FD_ZERO(&current_set);
	FD_SET(sockfd, &current_set);

	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(port); 

	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) 
		fatal_error();

	if (listen(sockfd, 10) != 0)
		fatal_error();
	
		while (1)
	{
		read_set = write_set = current_set;
		if (select(max_fd + 1, &read_set, &write_set, NULL, NULL) < 0)
			continue;
		for (int fd = 0; fd <= max_fd; fd++)
		{
			if (!FD_ISSET(fd, &read_set))
				continue;
			if (fd == sockfd)
			{
				anadir_cliente(sockfd);
			}
			else
			{
				int ret = recv(fd, buffer, 1000, 0);
				if (ret <= 0)
				{
					eliminar_cliente(fd);
				}
				else
				{
					buffer[ret] = '\0';
					clients_bufs[fd] = str_join(clients_bufs[fd], buffer);
					enviar_mensaje(fd);
				}
			}
		}
	}
	return 0;
}