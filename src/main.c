/*
    ____  _ _   _           _   
   |  _ \(_) \ | | ___  ___| |_ 
   | |_) | |  \| |/ _ \/ __| __|
   |  __/| | |\  |  __/\__ \ |_ 
   |_|   |_|_| \_|\___||___/\__|

   Copyright Alexander Argentakis & gamersnest.org

   This file is licensed under the GPL v2.0 license

 */

#include "./binary_stream/binary_stream.h"
#include "./net/raknet/packets.h"
#include "./net/raknet/message_identifiers.h"
#include "./net/socket.h"
#ifdef _WIN32

#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")

#endif
#ifdef linux

#include <arpa/inet.h>

#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct
{
	char *address;
	unsigned short port;
	unsigned long long guid;
	unsigned short mtu_size;
	unsigned int last_sequence_number;
	unsigned int send_sequence_number;
	unsigned char has_connected;	
} connection_t;

connection_t *connections;
unsigned int connection_count;

void init_connections()
{
	connections = malloc(sizeof(connection_t));
	connection_count = 0;
}

void add_connection(connection_t connection)
{
	int i;
	for (i = 0; i < connection_count; ++i)
	{
		if (connection.address == connections[i].address && connection.port == connections[i].port)
		{
			return;
		}
	}
	++connection_count;
    connections = realloc(connections, connection_count * sizeof(connection_t));
    connections[connection_count - 1] = connection;
}

connection_t *get_connection(char *address, unsigned short port)
{
	int i;
	for (i = 0; i < connection_count; ++i)
	{
		if (address == connections[i].address && port == connections[i].port)
		{
			return &connections[i];
		}
	}
	return NULL;
}

void remove_connection(char *address, unsigned short port)
{
	int i;
	for (i = 0; i < connection_count; ++i)
	{
		if (address == connections[i].address && port == connections[i].port)
		{
			connection_t *temp = malloc(sizeof(connection_t));
			unsigned int cnt = 0;
			int j;
			for (j = 0; j < i; ++j) {
				++cnt;
    			temp = realloc(temp, cnt * sizeof(connection_t));
    			temp[cnt - 1] = connections[j];
			}
			int k;
			for (k = i + 1; k < connection_count; ++k)
			{
				++cnt;
    			temp = realloc(temp, cnt * sizeof(connection_t));
    			temp[cnt - 1] = connections[k];
			}
			--connection_count;
			connections = temp;
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	init_connections();
    int sock = create_socket("0.0.0.0", 19132);
    while (1)
    {
		sockin_t out = receive_data(sock);
		printf("0x%X -> %d\n", out.buffer[0] & 0xff, out.buffer_length);
		if ((out.buffer[0] & 0xff) == ID_UNCONNECTED_PING || out.buffer[0] == ID_UNCONNECTED_PING_OPEN_CONNECTIONS)
		{
			binary_stream_t stream;
    		stream.buffer = out.buffer;
    		stream.offset = 0;
    		stream.size = out.buffer_length;
			unconnected_ping_t packet = decode_unconnected_ping(&stream);
			unconnected_pong_t new_packet;
			new_packet.client_timestamp = packet.client_timestamp;
			new_packet.server_guid = 12345678;
			new_packet.server_name = "MCCPP;MINECON;Dedicated Server";
			binary_stream_t new_stream = encode_unconnected_pong(new_packet);
			sockin_t st;
			st.buffer = new_stream.buffer;
			st.buffer_length = new_stream.size;
			st.address = out.address;
			st.port = out.port;
			send_data(sock, st);
		}
		else if ((out.buffer[0] & 0xff) == ID_OPEN_CONNECTION_REQUEST_1)
		{
			binary_stream_t stream;
    		stream.buffer = out.buffer;
    		stream.offset = 0;
    		stream.size = out.buffer_length;
			open_connection_request_1_t packet = decode_open_connection_request_1(&stream);
			open_connection_reply_1_t new_packet;
			new_packet.server_guid = 12345678;
			new_packet.use_security = 0;
			new_packet.mtu_size = packet.mtu_size;
			binary_stream_t new_stream = encode_open_connection_reply_1(new_packet);
			sockin_t st;
			st.buffer = new_stream.buffer;
			st.buffer_length = new_stream.size;
			st.address = out.address;
			st.port = out.port;
			send_data(sock, st);
		}
		else if ((out.buffer[0] & 0xff) == ID_OPEN_CONNECTION_REQUEST_2)
		{
			binary_stream_t stream;
    		stream.buffer = out.buffer;
    		stream.offset = 0;
    		stream.size = out.buffer_length;
			open_connection_request_2_t packet = decode_open_connection_request_2(&stream);
			open_connection_reply_2_t new_packet;
			new_packet.server_guid = 12345678;
			new_packet.client_address.hostname = out.address;
			new_packet.client_address.port = out.port;
			new_packet.client_address.version = 4;
			new_packet.mtu_size = packet.mtu_size;
			new_packet.use_encryption = 0;
			binary_stream_t new_stream = encode_open_connection_reply_2(new_packet);
			sockin_t st;
			st.buffer = new_stream.buffer;
			st.buffer_length = new_stream.size;
			st.address = out.address;
			st.port = out.port;
			send_data(sock, st);
			connection_t new_connection;
			new_connection.address = out.address;
			new_connection.port = out.port;
			new_connection.guid = packet.client_guid;
			new_connection.mtu_size = packet.mtu_size;
			new_connection.last_sequence_number = 0;
			new_connection.has_connected = 0;
			new_connection.send_sequence_number = 0;
			add_connection(new_connection);
			printf("%lld\n", new_connection.guid);
			printf("%lld\n", get_connection(out.address, out.port)->guid);
		}
		else if ((out.buffer[0] & 0xff) >= 0x80 && (out.buffer[0] & 0xff) <= 0x8f)
		{
			connection_t *connection = get_connection(out.address, out.port);
			sockin_t st;
			binary_stream_t stream;
    		stream.buffer = out.buffer;
    		stream.offset = 0;
    		stream.size = out.buffer_length;
			frame_set_t packet = decode_frame_set(&stream);
			if (connection->last_sequence_number < packet.sequence_number)
			{
				acknowledgement_t nack;
				nack.sequence_numbers_count = packet.sequence_number - connection->last_sequence_number;
				nack.sequence_numbers = malloc(nack.sequence_numbers_count * sizeof(unsigned int));
				int i;
				int ii = 0;
				for (i = 0; i < nack.sequence_numbers_count; ++i)
				{
					nack.sequence_numbers[i] = i + connection->last_sequence_number;
				}
				binary_stream_t nack_stream = encode_acknowledgement(nack, 0);
				st.buffer = nack_stream.buffer;
				st.buffer_length = nack_stream.size;
				st.address = out.address;
				st.port = out.port;
				send_data(sock, st);
			}
			acknowledgement_t ack;
			ack.sequence_numbers = malloc(1 * sizeof(unsigned int));
			ack.sequence_numbers[0] = packet.sequence_number;
			ack.sequence_numbers_count = 1;
			binary_stream_t ack_stream = encode_acknowledgement(ack, 1);
			st.buffer = ack_stream.buffer;
			st.buffer_length = ack_stream.size;
			st.address = out.address;
			st.port = out.port;
			send_data(sock, st);
			connection->last_sequence_number = packet.sequence_number + 1;
			int i;
			for (i = 0; i < packet.frame_count; ++i)
			{
				printf("CUSTOM PACKET -> 0x%X\n", packet.frames[i].body[0] & 0xff);
				binary_stream_t stream;
    			stream.buffer = packet.frames[i].body;
    			stream.offset = 0;
    			stream.size = packet.frames[i].body_length;
				if (connection->has_connected == 0)
				{
					if ((stream.buffer[0] & 0xff) == ID_CONNECTION_REQUEST)
					{
						connection_request_t connection_request = decode_connection_request(&stream);
						connection_request_accepted_t connection_request_accepted;
						connection_request_accepted.client_address.hostname = out.address;
						connection_request_accepted.client_address.port = out.port;
						connection_request_accepted.client_address.version = 4;
						connection_request_accepted.request_timestamp = connection_request.request_timestamp;
						connection_request_accepted.accepted_timestamp = time(NULL);
						binary_stream_t result = encode_connection_request_accepted(connection_request_accepted);
						frame_t frame;
						frame.body = result.buffer;
						frame.body_length = result.size;
						frame.is_fragmented = 0;
						frame.reliability = 0;
						frame_set_t set;
						set.frame_count = 1;
						set.frames = malloc(sizeof(frame_t));
						set.frames[0] = frame;
						set.sequence_number = connection->send_sequence_number;
						++connection->send_sequence_number;
						binary_stream_t set_stream = encode_frame_set(set);
						st.buffer = set_stream.buffer;
						st.buffer_length = set_stream.size;
						st.address = out.address;
						send_data(sock, st);
					}
					else if (stream.buffer[0] == ID_NEW_INCOMING_CONNECTION)
					{
						connection->has_connected = 1;
					}
				}
				if ((stream.buffer[0] & 0xff) == ID_CONNECTED_PING)
				{
					connected_ping_t connected_ping = decode_connected_ping(&stream);
					connected_pong_t connected_pong;
					connected_pong.client_timestamp = connected_ping.client_timestamp;
					binary_stream_t connected_pong_stream = encode_connected_pong(connected_pong);
					frame_t frame;
					frame.body = connected_pong_stream.buffer;
					frame.body_length = connected_pong_stream.size;
					frame.is_fragmented = 0;
					frame.reliability = 0;
					frame_set_t set;
					set.frame_count = 1;
					set.frames = malloc(sizeof(frame_t));
					set.frames[0] = frame;
					set.sequence_number = connection->send_sequence_number;
					++connection->send_sequence_number;
					binary_stream_t set_stream = encode_frame_set(set);
					st.buffer = set_stream.buffer;
					st.buffer_length = set_stream.size;
					st.address = out.address;
					send_data(sock, st);
				}
				else if ((stream.buffer[0] & 0xff) == ID_DISCONNECTION_NOTIFICATION)
				{
					remove_connection(out.address, out.port);
					printf("Disconnected\n");
				}
				else if ((stream.buffer[0] & 0xff) == 0x82)
				{
					binary_stream_t status_stream;
					status_stream.buffer = malloc(0);
    				status_stream.offset = 0;
    				status_stream.size = 0;
					put_unsigned_byte(0x83, &status_stream);
					put_unsigned_int_be(0, &status_stream);
					binary_stream_t start_stream;
					start_stream.buffer = malloc(0);
    				start_stream.offset = 0;
    				start_stream.size = 0;
					put_unsigned_byte(0x87, &start_stream);
					put_unsigned_int_be(0, &start_stream);
					put_unsigned_int_be(0, &start_stream);
					put_unsigned_int_be(1, &start_stream);
					put_unsigned_int_be(1, &start_stream);
					put_float_be(0 + 128, &start_stream);
					put_float_be(9 + 64, &start_stream);
					put_float_be(0 + 128, &start_stream);
					frame_t frame;
					frame.body = status_stream.buffer;
					frame.body_length = status_stream.size;
					frame.is_fragmented = 0;
					frame.reliability = 0;
					frame_set_t set;
					set.frame_count = 1;
					set.frames = malloc(sizeof(frame_t));
					set.frames[0] = frame;
					set.sequence_number = connection->send_sequence_number;
					++connection->send_sequence_number;
					binary_stream_t set_stream = encode_frame_set(set);
					st.buffer = set_stream.buffer;
					st.buffer_length = set_stream.size;
					st.address = out.address;
					send_data(sock, st);
					frame.body = start_stream.buffer;
					frame.body_length = start_stream.size;
					frame.is_fragmented = 0;
					frame.reliability = 0;
					set.frame_count = 1;
					set.frames = malloc(sizeof(frame_t));
					set.frames[0] = frame;
					set.sequence_number = connection->send_sequence_number;
					++connection->send_sequence_number;
					set_stream = encode_frame_set(set);
					st.buffer = set_stream.buffer;
					st.buffer_length = set_stream.size;
					st.address = out.address;
					send_data(sock, st);
				}
			}
		}
	}
}