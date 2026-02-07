#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

void fatal_error() {
	write(2, "Fatal error\n", 12);
	exit(1);
}

// ========== COPIAR DEL SUBJECT ==========
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
// ========================================

// GLOBALS (esto es lo que trackeas)
int max_fd, sockfd, ids[1024], next_id;
char *bufs[1024];
fd_set current_set, read_set;

// BROADCAST: envia a todos excepto "except" y "sockfd"
void broadcast(int except, char *msg) {
	for (int fd = 0; fd <= max_fd; fd++)
		if (FD_ISSET(fd, &current_set) && fd != except && fd != sockfd)
			send(fd, msg, strlen(msg), 0);
}

// ADD: accept + asignar id + anunciar llegada
void add_client() {
	int peticion = accept(sockfd, 0, 0);
	char msg[100];
	
	if (peticion < 0) return;
	if (peticion > max_fd) max_fd = peticion;
	ids[peticion] = next_id++;
	bufs[peticion] = 0;
	FD_SET(peticion, &current_set);
	sprintf(msg, "server: client %d just arrived\n", ids[peticion]);
	broadcast(peticion, msg);
}

// REMOVE: anunciar salida + limpiar + cerrar
void remove_client(int fd) {
	char msg[100];
	sprintf(msg, "server: client %d just left\n", ids[fd]);
	broadcast(fd, msg);
	FD_CLR(fd, &current_set);
	close(fd);
	free(bufs[fd]);
	bufs[fd] = 0;
}

// RELAY: extraer y reenviar cada mensaje
void send_mensage(int fd) {
	char *msg, tmp[200];
	while (extract_message(&bufs[fd], &msg))
	{
		sprintf(tmp, "client %d: %s", ids[fd], msg);
		broadcast(fd, tmp);
		free(msg);
	}
}

int main(int argc, char **argv) {
	struct sockaddr_in servaddr;
	char buf[1001];
	
	if (argc != 2) {
		write(2, "Wrong number of arguments\n", 26);
		exit(1);
	}
	
	// SETUP: socket + bind + listen
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1)
		fatal_error();
	max_fd = sockfd;
	FD_ZERO(&current_set);
	FD_SET(sockfd, &current_set);
	
	bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	servaddr.sin_port = htons(atoi(argv[1])); 
	
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
		fatal_error();
	if (listen(sockfd, 10) != 0)
		fatal_error();
	
	// LOOP: select + handle cada fd
	while (1) {
		read_set = current_set;
		if (select(max_fd + 1, &read_set, 0, 0, 0) < 0) continue;
		
		for (int fd = 0; fd <= max_fd; fd++) {
			if (!FD_ISSET(fd, &read_set)) continue;
			
			if (fd == sockfd) {
				add_client();
			} else {
				int ret = recv(fd, buf, 1000, 0);
				if (ret <= 0)
					remove_client(fd);
				else {
					buf[ret] = 0;
					bufs[fd] = str_join(bufs[fd], buf);
					send_mensage(fd);
				}
			}
		}
	}
}
