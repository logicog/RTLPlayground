#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <time.h>
#include "httpd_sim.h"
#include <json.h>

#define PORTS 6
time_t last_called;

uint64_t txG[PORTS], txB[PORTS], rxG[PORTS], rxB[PORTS];
char txG_buff[20], txB_buff[20], rxG_buff[20], rxB_buff[20];
char num_buff[20];

int hasSuffix(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix >  lenstr)
        return 0;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}


char *getMime(const char *name)
{
	if (hasSuffix(name, ".html"))
		return "text/html";
	else if (hasSuffix(name, ".svg"))
		return "image/svg+xml";
	else if (hasSuffix(name, ".ico"))
		return "image/svg+xml";
	else if (hasSuffix(name, ".png"))
		return "image/png";
	else if (hasSuffix(name, ".js"))
		return "text/javascript";
	else if (hasSuffix(name, ".css"))
		return "text/css";
	return "text/plain";
}


void send_vlan(int s, int vlan)
{
	struct json_object *v;
	const char *jstring;
        char *header = "HTTP/1.1 200 OK\r\n"
		       "Content-Type: application/json; charset=UTF-8\r\n\r\n";

	v = json_object_new_object();
	
	sprintf(num_buff, "0x%08x", 0x00060011);
	json_object_object_add(v, "members", json_object_new_string(num_buff));

        write(s, header, strlen(header));

	jstring = json_object_to_json_string_ext(v, JSON_C_TO_STRING_PLAIN);
        write(s, jstring, strlen(jstring));
	json_object_put(v);
}


void send_status(int s)
{
	struct json_object *jobj, *ports, *v;
	const char *jstring;
        char *header = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n\r\n";

	time_t now = time(NULL);
	now == last_called ? last_called + 1 : now; // Make sure we don't divide by 0 for rates

	ports = json_object_new_array_ext(PORTS);
	for (int i = 1; i <= PORTS; i++) {
		v = json_object_new_object();
		json_object_object_add(v, "portNum", json_object_new_int(i));
		json_object_object_add(v, "isSFP", json_object_new_int(i < 5 ? 0 : 1));
		json_object_object_add(v, "enabled", json_object_new_int((i % 4) ? 1 : 0));
		json_object_object_add(v, "link", json_object_new_int(i % 2 ? ((i == 1)? 5 : 2) : 0));
		if (i % 2) {
			uint64_t rate = (i == 1) ? 2400000000 : 950000000;
			txG[i-1] += rate * (now - last_called);
			rxG[i-1] += rate * (now - last_called);
			txB[i-1] += rate * (now - last_called) / 10000000;
			rxB[i-1] += rate * (now - last_called) / 10000000;
		}
		sprintf(txG_buff, "0x%016x", txG[i-1]);
		sprintf(txB_buff, "0x%016x", txB[i-1]);
		sprintf(rxG_buff, "0x%016x", rxG[i-1]);
		sprintf(rxB_buff, "0x%016x", rxB[i-1]);
		json_object_object_add(v, "txG", json_object_new_string(txG_buff));
		json_object_object_add(v, "txB", json_object_new_string(txB_buff));
		json_object_object_add(v, "rxG", json_object_new_string(rxG_buff));
		json_object_object_add(v, "rxB", json_object_new_string(rxB_buff));
		json_object_array_add(ports, v);
	}
	last_called = now;

        write(s, header, strlen(header));
	
	jstring = json_object_to_json_string_ext(ports, JSON_C_TO_STRING_PLAIN);
        write(s, jstring, strlen(jstring));
	json_object_put(v);
}


struct Server serverConstructor(int port, void (*launch)(struct Server *server)) {
    struct Server server;

    server.domain = AF_INET;
    server.service = SOCK_STREAM;
    server.port = port;
    server.protocol = 0;
    server.backlog = 10;

    server.address.sin_family = server.domain;
    server.address.sin_port = htons(port);
    server.address.sin_addr.s_addr = htonl(INADDR_ANY);

    server.socket = socket(server.domain, server.service, server.protocol);
    if (server.socket < 0) {
        perror("Failed to initialize/connect to socket...\n");
        exit(EXIT_FAILURE);
    }

    if (bind(server.socket, (struct sockaddr*)&server.address, sizeof(server.address)) < 0) {
        perror("Failed to bind socket...\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server.socket, server.backlog) < 0) {
        perror("Failed to start listening...\n");
        exit(EXIT_FAILURE);
    }

    server.launch = launch;
    return server;
}

void launch(struct Server *server) {
    char buffer[BUFFER_SIZE];
    FILE *inptr;

    last_called = time(NULL);
    for (int i=0; i < PORTS; i++)
	    txG[i] = txB[i] = rxG[i] = rxB[i] = 0;

    while (1) {
        printf("=== Waiting for connection on port %d === \n", server->port);
        int addrlen = sizeof(server->address);
        int new_socket = accept(server->socket, (struct sockaddr*)&server->address, (socklen_t*)&addrlen);
        ssize_t bytesRead = read(new_socket, buffer, BUFFER_SIZE - 1);
        if (bytesRead >= 0) {
            buffer[bytesRead] = '\0';  // Null terminate the string
            puts(buffer);
            
            if (buffer[0] == 'G' && buffer[1] == 'E' && buffer[2] == 'T' && buffer[3] == ' ') {
                printf("GET request\n");
		if (!strncmp(&buffer[4], "/status.json", 12)) {
                    printf("Status request\n");
                    send_status(new_socket);
		    goto done;
                }
		if (!strncmp(&buffer[4], "/vlan.json?vid=", 15)) {
		    int vlan = atoi(&buffer[19]);
                    printf("VLAN request for %d\n", vlan);
                    send_vlan(new_socket, vlan);
		    goto done;
                }
                int i = 0;
		while (!isspace(buffer[4 + i]))
			i++;
		buffer[4+i] = '\0';

		printf("Serving file: >%s<\n", &buffer[5]);
		inptr = fopen(&buffer[5], "rb");
		if (inptr == NULL) {
			printf("Cannot open input file %s\n", &buffer[5]);
			char *response = "HTTP/1.1 404 Not found\r\n"
					 "Content-Type: text/html\r\n\r\n"
					 "<!DOCTYPE html> <html><head><title>Not found</title></head>"
					 "<body><h1>Not found!</h1></html>";
			write(new_socket, response, strlen(response));
			goto done;
		}

		char *mime =getMime(&buffer[5]);
		printf("MIME type: %s\n", mime);

		fseek(inptr, 0L, SEEK_END);
		int filesize = ftell(inptr);
		printf("Filesize: %d\n", filesize);
		rewind(inptr);
		printf("Input file size: %ld\n", filesize);
		if (filesize > BUFFER_SIZE) {
			printf("File too large.\n");
			goto done;
		}
		size_t bytes_read = fread(buffer, 1, sizeof(buffer), inptr);

		printf("Bytes read: %ld\n", bytes_read);

		if (bytes_read != filesize) {
			printf("Error reading input file.\n");
			goto done;
		}
		fclose(inptr);
		char *response = "HTTP/1.1 200 OK\r\n"
				"Content-Type: ";
		write(new_socket, response, strlen(response));
		
		write(new_socket, mime, strlen(mime));
		response = "; charset=UTF-8\r\n\r\n";
		write(new_socket, response, strlen(response));

		write(new_socket, buffer, filesize);
            }
        } else {
            perror("Error reading buffer...\n");
        }
	
done:
        close(new_socket);
    }
}


int main() {
    struct Server server = serverConstructor(8080, launch);
    server.launch(&server);
    return 0;
}
