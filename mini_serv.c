#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

int client_ids[1024];
char *client_bufs[1024];
int max_fd = 0;
int next_id = 0;
fd_set read_set, write_set, current_set;

void fatal_error(void)
{
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
	char *newbuf;
	int len;

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

void send_to_all(int sender_fd, char *msg)
{
	for (int fd = 0; fd <= max_fd; fd++)
	{
		if (FD_ISSET(fd, &write_set) && fd != sender_fd)
			send(fd, msg, strlen(msg), 0);
	}
}

void add_client(int sockfd)
{
	int connfd;
	struct sockaddr_in cli;
	socklen_t len = sizeof(cli);
	char msg[100];

	connfd = accept(sockfd, (struct sockaddr *)&cli, &len);
	if (connfd < 0)
		return;

	if (connfd > max_fd)
		max_fd = connfd;

	client_ids[connfd] = next_id++;
	client_bufs[connfd] = NULL;

	FD_SET(connfd, &current_set);

	sprintf(msg, "server: client %d just arrived\n", client_ids[connfd]);
	send_to_all(connfd, msg);
}

void remove_client(int fd)
{
	char msg[100];

	sprintf(msg, "server: client %d just left\n", client_ids[fd]);
	send_to_all(fd, msg);

	FD_CLR(fd, &current_set);
	close(fd);
	free(client_bufs[fd]);
	client_bufs[fd] = NULL;
}

void send_message(int fd)
{
	char *msg;
	char buffer[200];

	while (extract_message(&client_bufs[fd], &msg))
	{
		sprintf(buffer, "client %d: %s", client_ids[fd], msg);
		send_to_all(fd, buffer);
		free(msg);
	}
}

int main(int argc, char **argv)
{
	int sockfd, port;
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
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
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
				add_client(sockfd);
				break;
			}
			else
			{
				int ret = recv(fd, buffer, 1000, 0);
				if (ret <= 0)
				{
					remove_client(fd);
					break;
				}
				else
				{
					buffer[ret] = '\0';
					client_bufs[fd] = str_join(client_bufs[fd], buffer);
					send_message(fd);
				}
			}
		}
	}

	return 0;
}