#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "httpd_sim.h"
#include <json.h>
#include <signal.h>
#include "../version.h"

#define SESSION_ID "1234567890ab"
#define PASSWORD "1234"
#define SESSION_TIMEOUT 2000

#define PORTS 6
time_t last_called;
time_t last_session_use;

uint64_t txG[PORTS], txB[PORTS], rxG[PORTS], rxB[PORTS];
char txG_buff[20], txB_buff[20], rxG_buff[20], rxB_buff[20];
char sfp_temp[8], sfp_vcc[8], sfp_txbias[8], sfp_txpower[8], sfp_rxpower[8], sfp_laser[8], sfp_options[6];
char num_buff[20];
char upload_buffer[4194304]; // 4MB

char *content_type = NULL;
char boundary[72];
char cmd_history[1024][256];
uint16_t cmd_ptr = 0;
char *uploaded_config = NULL;
int uploaded_config_len;
const char *session = NULL;
bool authenticated = false;

char is_word(char *c, char *d)
{
	uint8_t i = 0;

	while (d[i] && (d[i] == c[i]))
		i++;

	if (d[i])
		return 0;
	if (c[i] != ' ' && c[i] != '\t' && c[i] != ':' && c[i] != '?' && c[i] != '=' && c[i] != '\n' && c[i] != '\r' && c[i])
		return 0;
	return 1;
}


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

void send_basic_info(int socket)
{
	char *response = "HTTP/1.1 200 OK\r\n"
		    "Content-Type: application/json; charset=UTF-8\r\n\r\n"
			"{\"ip_address\":\"192.168.10.247\",\"ip_gateway\":\"192.168.2.22\",\"ip_netmask\":\"255.255.255.0\",\"mac_address\":\"1c:2a:a3:23:00:02\",\"sw_ver\":\"" VERSION_SW "\",\"hw_ver\":\"SWGT024-V2.0\"}";
	write(socket, response, strlen(response));
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
	struct json_object *ports, *v;
	const char *jstring;
        char *header = "HTTP/1.1 200 OK\r\n"
		       "Cache-Control: no-cache\r\n"
		       "Content-Type: application/json; charset=UTF-8\r\n\r\n";

	printf("Sending status.\n");
	time_t now = time(NULL);
	now = last_called ? last_called + 1 : now; // Make sure we don't divide by 0 for rates

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
		sprintf(txG_buff, "0x%016lx", txG[i-1]);
		sprintf(txB_buff, "0x%016lx", txB[i-1]);
		sprintf(rxG_buff, "0x%016lx", rxG[i-1]);
		sprintf(rxB_buff, "0x%016lx", rxB[i-1]);
		json_object_object_add(v, "txG", json_object_new_string(txG_buff));
		json_object_object_add(v, "txB", json_object_new_string(txB_buff));
		json_object_object_add(v, "rxG", json_object_new_string(rxG_buff));
		json_object_object_add(v, "rxB", json_object_new_string(rxB_buff));
		if (i >= 5) {
			uint16_t temp = 0x28fb + rand() / (RAND_MAX / 100);
			uint16_t vcc = 0x7eda + rand() / (RAND_MAX / 100);
			uint16_t txbias = 0x0d24 +rand() / (RAND_MAX / 100);
			uint16_t txpower = 0x14bd + rand() / (RAND_MAX / 100);
			uint16_t rxpower = 0;
			uint16_t laser = 0;
			uint16_t options = 0x68;
			
			sprintf(sfp_options, "0x%02x", options);
			sprintf(sfp_temp, "0x%04x", temp);
			sprintf(sfp_vcc, "0x%04x", vcc);
			sprintf(sfp_txbias, "0x%04x", txbias);
			sprintf(sfp_txpower, "0x%04x", txpower);
			sprintf(sfp_rxpower, "0x%04x", rxpower);
			sprintf(sfp_laser, "0x%04x", laser);
			json_object_object_add(v, "sfp_vendor", json_object_new_string("OEM"));
			json_object_object_add(v, "sfp_model", json_object_new_string("10G-SFP+"));
			json_object_object_add(v, "sfp_serial", json_object_new_string("12345678"));
			json_object_object_add(v, "sfp_options", json_object_new_string(sfp_options));
			json_object_object_add(v, "sfp_temp", json_object_new_string(sfp_temp));
			json_object_object_add(v, "sfp_vcc", json_object_new_string(sfp_vcc));
			json_object_object_add(v, "sfp_txbias", json_object_new_string(sfp_txbias));
			json_object_object_add(v, "sfp_txpower", json_object_new_string(sfp_txpower));
			json_object_object_add(v, "sfp_rxpower", json_object_new_string(sfp_rxpower));
			json_object_object_add(v, "sfp_laser", json_object_new_string(sfp_laser));
		}
		json_object_array_add(ports, v);
	}
	last_called = now;

        write(s, header, strlen(header));
	
	jstring = json_object_to_json_string_ext(ports, JSON_C_TO_STRING_PLAIN);
        write(s, jstring, strlen(jstring));
	json_object_put(v);
}


void send_eee(int s)
{
	struct json_object *ports, *v;
	const char *jstring;
        char *header = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n\r\n";

	ports = json_object_new_array_ext(PORTS);
	for (int i = 1; i <= PORTS; i++) {
		v = json_object_new_object();
		json_object_object_add(v, "portNum", json_object_new_int(i));
		json_object_object_add(v, "isSFP", json_object_new_int(i < 5 ? 0 : 1));
		uint8_t eee = 0;
		eee |= 0x02;
		char eee_buf[20];
		sprintf(eee_buf, "%08b", eee);
		json_object_object_add(v, "eee", json_object_new_string(eee_buf));
		uint8_t eee_lp = 0;
		eee_lp |= 0x04;
		sprintf(eee_buf, "%08b", eee_lp);
		json_object_object_add(v, "eee_lp", json_object_new_string(eee_buf));
		json_object_object_add(v, "active", json_object_new_int((i % 2) ? 1 : 0));
		json_object_array_add(ports, v);
	}

        write(s, header, strlen(header));
	
	jstring = json_object_to_json_string_ext(ports, JSON_C_TO_STRING_PLAIN);
        write(s, jstring, strlen(jstring));
	json_object_put(ports);
}


void send_mirror(int s)
{
	uint16_t mirror_tx, mirror_rx = 0;
	char mirror_tx_buf[20];
	char mirror_rx_buf[20];
	struct json_object *mirror;
	const char *jstring;
        char *header = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n\r\n";

	mirror = json_object_new_object();
	json_object_object_add(mirror, "mPort", json_object_new_int(1));
	json_object_object_add(mirror, "enabled", json_object_new_int(1));

	mirror_tx = 0b000110;
	mirror_rx = 0b000010;
	sprintf(mirror_tx_buf, "%016b", mirror_tx);
	sprintf(mirror_rx_buf, "%016b", mirror_rx);
	json_object_object_add(mirror, "mirror_tx", json_object_new_string(mirror_tx_buf));
	json_object_object_add(mirror, "mirror_rx", json_object_new_string(mirror_rx_buf));

        write(s, header, strlen(header));

	jstring = json_object_to_json_string_ext(mirror, JSON_C_TO_STRING_PLAIN);
        write(s, jstring, strlen(jstring));
	json_object_put(mirror);
}


void send_lag(int s)
{
	char lag_buf[20];
	struct json_object *lags = json_object_new_array_ext(4);
	struct json_object *v;
	const char *jstring;
        char *header = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json; charset=UTF-8\r\n\r\n";

	for (int i = 0; i < 4; i++) {
		v = json_object_new_object();
		json_object_object_add(v, "lagNum", json_object_new_int(i));
		sprintf(lag_buf, "%016b", 0x30 << (i * 2) & 0x1f8);
		json_object_object_add(v, "members", json_object_new_string(lag_buf));
		json_object_object_add(v, "hash", json_object_new_string("0x7e"));
		json_object_array_add(lags, v);
	}
        write(s, header, strlen(header));
	
	jstring = json_object_to_json_string_ext(lags, JSON_C_TO_STRING_PLAIN);
        write(s, jstring, strlen(jstring));
	json_object_put(lags);
}


void send_cmd_log(int s)
{
        char *header = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
        write(s, header, strlen(header));

	for (int i = 0; i < cmd_ptr; i++) {
		write(s, cmd_history[i], strlen(cmd_history[i]));
		write(s, "\n", 1);
		printf("%d: %s\n", i, cmd_history[i]);
	}
}


void send_config(int s)
{
        char *header = "HTTP/1.1 200 OK\r\n"
                         "Content-Type: text/plain; charset=UTF-8\r\n\r\n";
	printf("Sending uploaded config\n");
        write(s, header, strlen(header));
	printf("Uploaded config len: %d\n", uploaded_config_len);
	printf(">%s<", uploaded_config);
	write(s, uploaded_config, uploaded_config_len);
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

void send_not_found(int socket) {
	char *response = "HTTP/1.1 404 Not found\r\n"
			"Content-Type: text/html\r\n\r\n"
			"<!DOCTYPE html> <html><head><title>Not Found</title></head>"
			"<body><h1>Not found!</h1></html>";
	write(socket, response, strlen(response));
}


void send_bad_request(int socket) {
	char *response = "HTTP/1.1 400 Bad Request\r\n"
			"Content-Type: text/html\r\n\r\n"
			"<!DOCTYPE html> <html><head><title>Bad Request</title></head>"
			"<body><h1>Bad Request!</h1></html>";
	write(socket, response, strlen(response));
}


void send_to_login(int socket) {
	char *response = "HTTP/1.1 302 Found\r\n"
			 "Location: login.html\r\n\r\n";
	write(socket, response, strlen(response));
}


void send_unauthorized(int socket) {
	char *response = "HTTP/1.1 401 Unauthorized\r\n\r\n";
	write(socket, response, strlen(response));
}


char *scan_header(char *p)
{
	session = 0;
	authenticated = false;
	while (*p != '\r' || *(p + 1) != '\n' || *(p + 2) != '\r' || *(p + 3) != '\n') {
		if (!*p++)
			break;
		if (is_word(p, "\nContent-Type:"))
			content_type = p + 15;
		else if (is_word(p, "\nCookie:"))
			session = p + 17;
	}
	if (content_type && is_word(content_type, "multipart/form-data; boundary")) {
		printf("Found multiplart\n");
		content_type += 30;
		uint8_t i = 0;
		while (content_type[i] != '\r' && content_type[i] != '\n') {
			boundary[i + 2] = content_type[i];
			i++;
		}
		// The boundary between parts is "--" + the boundary given in the header
		boundary[0] = '-';
		boundary[1] = '-';
		boundary[i + 2] = 0;
	}

	time_t now = time(NULL);
	if (session) {
		printf("Session: >%s<. time now: %ld last %ld\n", session, now, last_session_use);
		if (now - last_session_use > SESSION_TIMEOUT) {
			printf("Session expired\n");
		} else {
			if (!strncmp(session, SESSION_ID, 12))
				authenticated = true;
			else
				printf("Invalid session cookie!\n");
		}
	}
	printf("Time now: %ld last %ld\n", now, last_session_use);
	return p;
}


char *skip_boundary(char *p)
{
	while (*p) {
		if (is_word(p, boundary))
			return p + strlen(boundary);
		p++;
	}
	return p;
}

void print_cmd_history(void)
{
	for (int i = 0; i < cmd_ptr; i++)
		printf("%d: %s\n", i, cmd_history[i]);
}

void launch(struct Server *server)
{
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
		printf("bytesRead: %ld\n", bytesRead);
		int filesize = 0;
		char *mime;

		if (bytesRead > 0) {
			buffer[bytesRead] = '\0';  // Null terminate the string
			puts(buffer);

			if (is_word(buffer, "GET")) {
				scan_header(buffer);
				if (!strncmp(&buffer[4], "/status.json", 12)) {
					printf("Status request\n");
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_status(new_socket);
					goto done;
				} else if (!strncmp(&buffer[4], "/eee.json", 9)) {
					printf("EEE request\n");
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_eee(new_socket);
					goto done;
				} else if (!strncmp(&buffer[4], "/information.json", 17)) {
					printf("Status request\n");
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_basic_info(new_socket);
					goto done;
				} else if (!strncmp(&buffer[4], "/mirror.json", 12)) {
					printf("Mirror request\n");
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_mirror(new_socket);
					goto done;
				} else if (!strncmp(&buffer[4], "/lag.json", 9)) {
					printf("LAG request\n");
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_lag(new_socket);
					goto done;
				} else if (!strncmp(&buffer[4], "/vlan.json?vid=", 15)) {
					int vlan = atoi(&buffer[19]);
					printf("VLAN request for %d\n", vlan);
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_vlan(new_socket, vlan);
					goto done;
				} else if (!strncmp(&buffer[4], "/cmd_log", 8)) {
					printf("Request cmd_log\n");
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_cmd_log(new_socket);
					goto done;
				} else if (!strncmp(&buffer[4], "/config ", 8) && uploaded_config) {
					printf("Request current config.\n");
					if (!authenticated)
						send_unauthorized(new_socket);
					else
						send_config(new_socket);
					goto done;
				}
				if (!authenticated && !(!strncmp(&buffer[4], "/login.html", 11) || !strncmp(&buffer[4], "/style.css", 10))) {
					send_to_login(new_socket);
					goto done;
				}

				// A web-page is actively accessed, we can reset session time-out
				last_session_use = time(NULL);

				int i = 0;
				while (!isspace(buffer[4 + i]))
					i++;
				buffer[4+i] = '\0';

				printf("Serving file: >%s<, name length %d\n", &buffer[5], i);
				if (i > 1)
					inptr = fopen(&buffer[5], "rb");
				else
					inptr = fopen("/index.html", "rb");
				if (inptr == NULL) {
					printf("Cannot open input file %s\n", &buffer[5]);
					send_not_found(new_socket);
					goto done;
				}

				mime = getMime(&buffer[5]);
				printf("MIME type: %s\n", mime);

				fseek(inptr, 0L, SEEK_END);
				filesize = ftell(inptr);
				printf("Filesize: %d\n", filesize);
				rewind(inptr);
				printf("Input file size: %d\n", filesize);
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
			} else if (is_word(buffer, "POST")) {
				printf("POST request\n");
				// Find end of request header
				char *p = buffer;
				boundary[0] ='\0';
				p = scan_header(p);
				printf("Boundary: >%s<\n", boundary);
				if (!*p || !content_type) {
					printf("Bad request, no content type!\n");
					send_bad_request(new_socket);
					goto done;
				}
				if (!authenticated && !is_word(&buffer[5], "/login")) {
					send_unauthorized(new_socket);
					goto done;
				}
				printf("Bytes read %ld\n", bytesRead);
				if (is_word(&buffer[5], "/cmd")) {
					printf("POST cmd\n");
					printf("CMD: %s\n", p + 4);
					strcpy(cmd_history[cmd_ptr], p + 4);
					cmd_ptr++;
					print_cmd_history();
					char *response = "HTTP/1.1 200 OK\r\n"
							"Content-Type: text/html\r\n\r\n"
							"<!DOCTYPE html> <html><head><title>Upload OK</title></head>"
							"<body><h1>Command executed successully</h1></html>";
					write(new_socket, response, strlen(response));
					goto done;
				} else if (is_word(&buffer[5], "/login")) {
					printf("POST login\n");
					p += 4;
					p += 4; // Read also over "pwd="
					char *response;
					if (is_word(p, PASSWORD)) {
						printf("Password accepted!\n");
						response = "HTTP/1.1 302 Found\r\n"
							   "Location: index.html\r\n"
							   "Set-Cookie: session=" SESSION_ID "; SameSite=Strict\r\n";
					} else {
						response = "HTTP/1.1 302 Found\r\n"
							   "Location: login.html\r\n\r\n";
					}
					write(new_socket, response, strlen(response));
					goto done;
				} else if (is_word(&buffer[5], "/upload") || is_word(&buffer[5], "/config")) {
					printf("POST upload/config request\n");
					bool config_upload = false;
					if (is_word(&buffer[5], "/config"))
						config_upload = true;
					if (!boundary[0]) {
						printf("Bad request, no boundary!\n");
						send_bad_request(new_socket);
						goto done;
					}
					// We skip the intial parts as part of the header
					do {
						p = skip_boundary(p);
						if (!*p)
							goto bad_request;
						p = scan_header(p);
						if (!*p || !content_type)
							goto bad_request;
					} while (!is_word(content_type, "application/octet-stream"));
					printf("Have content: >%s<\n", content_type);

					p += 4; // Skip \r\n\r\n after content type
					char *uptr = upload_buffer;
					int bindex = 0;
					int bptr = p - buffer;
					do {
						if (bptr >= bytesRead) {
							bptr = 0;
							bytesRead = read(new_socket, buffer, BUFFER_SIZE - 1);
							printf("bytesRead: %ld\n", bytesRead);
							if (!bytesRead)
								break;
						}
						if (!boundary[bindex])
							break;
						if (buffer[bptr] == boundary[bindex]) {
							bptr++;
							bindex++;
						} else {
							for (int i = 0; i < bindex; i++)
								*uptr++ = boundary[i];
							*uptr++ = buffer[bptr++];
							bindex = 0;
						}
					} while(1);
					printf("Done reading\n");
					printf("%s", upload_buffer);
					if (!bindex || boundary[bindex])
						goto bad_request;
					char *response = "HTTP/1.1 200 OK\r\n"
							"Content-Type: text/html\r\n\r\n"
							"<!DOCTYPE html> <html><head><title>Upload OK</title></head>"
							"<body><h1>File uploaded successully</h1></html>";
					write(new_socket, response, strlen(response));
					if (config_upload) {
						uploaded_config_len = (uptr - upload_buffer);
						if (uploaded_config)
							free (uploaded_config);
						uploaded_config = malloc(uploaded_config_len + 1);
						memcpy(uploaded_config, upload_buffer, uploaded_config_len);
					}
					goto done;
				}
			}
			char *response = "HTTP/1.1 200 OK\r\n"
					 "Cache-Control: max-age=60, must-revalidate\r\n"
					 "Content-Type: ";
			write(new_socket, response, strlen(response));

			write(new_socket, mime, strlen(mime));
			response = "; charset=UTF-8\r\n\r\n";
			write(new_socket, response, strlen(response));
			if (filesize)
				write(new_socket, buffer, filesize);
		} else if (bytesRead == 0) {
			printf("EOF\n");
			continue;
		} else {
			perror("Error reading buffer, nothing read...\n");
		}
		
done:
		close(new_socket);
		continue;
bad_request:
		printf("Bad request!\n");
		send_bad_request(new_socket);
		close(new_socket);
	}
}


int main()
{
	// Make sure we can handle writes to a dead client without a signal handler
	signal(SIGPIPE, SIG_IGN);

	struct Server server = serverConstructor(8080, launch);
	server.launch(&server);

	return 0;
}
