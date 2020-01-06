/* Don't judge crappy code, it is merely debugging tool */
#include <iostream>
#include <fstream>
#include <limits>
#include <vector>
#include <iomanip>
#include <endian.h>
#include <byteswap.h>

/* XXTEA related */
#define DELTA 0x9e3779b9
#define MX (((z>>5^y<<2) + (y>>3^z<<4)) ^ ((sum^y) + (key[(p&3)^e] ^ z)))

uint32_t factory_key[4] = { 0x0102aabb, 0x0203aabb, 0x0304aabb, 0x0506aabb};
uint32_t session_key[4];

static void xxtea(uint32_t *v, int n, uint32_t const key[4]) {
	uint32_t y, z, sum;
	uint32_t p, rounds, e;
	if (n > 1) {
		rounds = 6 + 52 / n;
		sum = 0;
		z = v[n - 1];
		do {
			sum += DELTA;
			e = (sum >> 2) & 3;
			for (p = 0; p < n - 1; p++) {
				y = v[p + 1];
				z = v[p] += MX;
			}
			y = v[0];
			z = v[n - 1] += MX;
		} while (--rounds);
	}
	else if (n < -1) {
		n = -n;
		rounds = 6 + 52 / n;
		sum = rounds * DELTA;
		y = v[0];
		do {
			e = (sum >> 2) & 3;
			for (p = n - 1; p > 0; p--) {
				z = v[p - 1];
				y = v[p] -= MX;
			}
			z = v[n - 1];
			y = v[0] -= MX;
			sum -= DELTA;
		} while (--rounds);
	}

}

int main()
{
	std::ifstream infile("initial_dump.txt");
	std::vector<uint8_t> data;
	uint8_t data_buffer[256];
	uint32_t data_len = 0;

	printf("INITIAL FILE :");
	while (!infile.eof()) {
		char buf[256];
		uint32_t a;
		std::stringstream ss;

		infile.read(buf, 2);
		if (infile) {
			ss << buf;
			ss >> std::hex >> a;
			data_buffer[data_len++] = a;
			printf("%02hhx ", a);
			data.push_back(a);
		}
	}
	printf("\n");


	/* STM8 is BE, we are LE */	
	for (int i = 0; i < 2; i++)
		*(uint32_t*)&data_buffer[i*4] = bswap_32(*(uint32_t*)&data_buffer[i*4]);
	
	xxtea((uint32_t*)data_buffer, -2, factory_key);

	if (*(uint32_t*)&data_buffer[4] == factory_key[0]) {
		std::cout << "Decryption key is valid, sequence is " << *(uint32_t*)&data_buffer[0] << std::endl;
	}

	//*(uint32_t*)&data_buffer[0] = bswap_32(*(uint32_t*)&data_buffer[0]);
	// Calculating session key
	for (int i = 0; i < 4; i++)
		*(uint32_t*)&data_buffer[(i+1)*4] = factory_key[i];

	xxtea((uint32_t*)data_buffer, 5, factory_key);

	for (int i = 0; i < 5; i++)
		*(uint32_t*)&data_buffer[i*4] = bswap_32(*(uint32_t*)&data_buffer[i*4]);

	//std::cout << "DBG: 7c940f30fdbf265c" << std::endl;
	printf("SESSION_KEY: ");
	for (int i = 0; i < 4*4; i++)
		printf("%02hhx ", data_buffer[i]);
	printf("\n");
	session_key[0] = bswap_32(*(uint32_t*)&data_buffer[0]);
	session_key[1] = bswap_32(*(uint32_t*)&data_buffer[4]);
	session_key[2] = bswap_32(*(uint32_t*)&data_buffer[8]);
	session_key[3] = bswap_32(*(uint32_t*)&data_buffer[12]);

	std::ifstream infile2("sensordata_dump.txt");

	data_len=0;
	printf("SENSORDATA FILE :");
	while (!infile2.eof()) {
		char buf[256];
		uint32_t a;
		std::stringstream ss;

		infile2.read(buf, 2);
		if (infile2) {
			ss << buf;
			ss >> std::hex >> a;
			data_buffer[data_len++] = a;
			printf("%02hhx ", a);
		}
	}
	printf("\n");
	for (int i = 0; i < data_len/4; i++)
		*(uint32_t*)&data_buffer[i*4] = bswap_32(*(uint32_t*)&data_buffer[i*4]);

	xxtea((uint32_t*)data_buffer, (data_len/4)*-1, session_key);

	for (int i = 0; i < data_len/4; i++)
		*(uint32_t*)&data_buffer[i*4] = bswap_32(*(uint32_t*)&data_buffer[i*4]);
	
	printf("DECODED: ");
	for (int i = 0; i < data_len; i++)
		printf("%02hhx ", data_buffer[i]);



	exit(0);
	

	//0x00000001 0102aabb16

	//for (int i = 0; i < 4; i++)
	//	factory_key[i] = be32toh(factory_key[i]);
	//for (int i = 0; i < data_len; i += 4)
	//	* (uint32_t*)&data_buffer[i] = be32toh(*(uint32_t*)&data_buffer[i]);
//*/
	/*
	data_buffer[0] = 0x0;
	data_buffer[1] = 0x0;
	data_buffer[2] = 0x0;
	data_buffer[3] = 0x1;
	xxtea((uint32_t*)data_buffer,data_len/4,factory_key);
	*/
	//xxtea((uint32_t*)data_buffer, data_len / 4 * -1, factory_key);

    // 000000010102aabb16
    // 00000001 0102aabb 0203aabb 0304aabb 0506aabb 16
    // 7c940f30 fdbf265c 4c7bbedc 25699bc4 0086bce5 ae

	for (int i=0;i<4*5;i++)
		data_buffer[i] = 0;

	data_buffer[0] = 0x0;
	data_buffer[1] = 0x0;
	data_buffer[2] = 0x0;
	data_buffer[3] = 0x1;
	
	for (int i = 0; i < 4; i++) {
		*(uint32_t*)&data_buffer[(i+1)*4] = bswap_32(factory_key[i]);
	}
	for (int i = 0; i < 5; i++) {
		*(uint32_t*)&data_buffer[i*4] = bswap_32(*(uint32_t*)&data_buffer[i*4]);
	}

	printf("\nXX: %x\nENCRYPTED:\n", *(uint32_t*)&data_buffer[0]);

	#define CRYPT_BYTES 4

	xxtea((uint32_t*)data_buffer, CRYPT_BYTES, factory_key);

	for (int i = 0; i < CRYPT_BYTES; i++)
		*(uint32_t*)&data_buffer[i*4] = bswap_32(*(uint32_t*)&data_buffer[i*4]);

	for (int i = 0; i < 4*CRYPT_BYTES; i++)
		printf("%02hhx ", data_buffer[i]);	
	printf("\n");
}

