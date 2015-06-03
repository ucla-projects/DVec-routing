/* my-router.c
 *
 * Usage:
 *			$ ./my-router [PORT]
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdbool.h> /* boolean */
#include <limits.h> /* INT_MAX */

#include <sys/time.h>   /* For FD_SET, FD_SELECT */
#include <sys/select.h>

#define BUFSIZE 	128
#define ROUTERA 	10000
#define ROUTERB		10001
#define ROUTERC		10002
#define ROUTERD		10003
#define ROUTERE		10004
#define ROUTERF		10005
#define INDEXA		0
#define INDEXB		1
#define INDEXC		2
#define INDEXD		3
#define INDEXE		4
#define INDEXF		5
#define NUMROUTERS	6


#ifndef max
	#define max( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

// struct node is size 84
struct router
{
	int index;
	char otherRouters[6];
	int costs[6];
	int outgoingPorts[6];
	int destinationPorts[6];
};

/* printRouter()
 *
 * Prints out the given router table.
 */
void printRouter(struct router* r)
{
        printf("*** ROUTER INFO ***\nIndex: %d\n", r->index);
        printf("otherRouters\tcosts\t\toutgoingPorts\tdestPorts\n");
        int i;
        for (i=0; i < NUMROUTERS; i++)
                printf("%c\t\t%d\t\t%d\t\t%d\n", r->otherRouters[i], r->costs[i], r->outgoingPorts[i], r->destinationPorts[i]);
        printf("*** END RTRINFO ***\n");
}

/* printBuffer()
 *
 * Prints out the given int buffer.
 */
void printBuffer(int * buf)
{
	int i;
	for (i = 0; i < BUFSIZE; i++)
		printf("%d ", buf[i]);
	printf("\n");
}

struct matrix
{
	int r[6][6];
};

struct flatmatrix
{
	int r[NUMROUTERS];
};

void error(char *msg) {
	perror(msg);
	exit(1);
}

/* stableState()
 *
 * Checks to see if the network has reached a stable state.
 */
int stableState(struct router* network) {
	int i;
	for (i = 1; i < NUMROUTERS; i++)
	{
		if (!tablesEqual(network, network[i]))
			return 0;
	}
	return 1;
}

/* tablesEqual()
 *
 * Checks if two routing tables have equal contents
 */
int tablesEqual(struct router *table1, struct router table2) {
	int idx;
	for (idx=0; idx<NUMROUTERS; idx++) {
		if (
			(table1->otherRouters[idx] != table2.otherRouters[idx])
			|| (table1->costs[idx] != table2.costs[idx])
			|| (table1->outgoingPorts[idx] != table2.outgoingPorts[idx])
			|| (table1->destinationPorts[idx] != table2.destinationPorts[idx]) )
			return 0;
	}
	return 1;
}

/* tableToBuffer()
 *
 * Converts a router struct representation into an int buffer representation of a router.
 */
void tableToBuffer(struct router *table, int *buf) {
	
	int offset = 0;
	buf[offset] = table->index;
	offset++;

	int i;
	for (i = 0; i < NUMROUTERS; i++)
	{
		buf[offset + 0*NUMROUTERS] = (int) table->otherRouters[i];
		buf[offset + 1*NUMROUTERS] = table->costs[i];
		buf[offset + 2*NUMROUTERS] = table->outgoingPorts[i];
		buf[offset + 3*NUMROUTERS] = table->destinationPorts[i];
		offset++;
	}
}

/* bufferToTable()
 *
 * Converts an integer array representation into a struct representation of a router.
 */
void bufferToTable(int *buf, struct router *table) {

	int offset = 0;
	table->index = buf[offset];
	offset++;

	int i;
	for (i = 0; i < NUMROUTERS; i++)
	{
		table->otherRouters[i] = (char) buf[offset + 0*NUMROUTERS];
		table->costs[i] =		buf[offset + 1*NUMROUTERS];
		table->outgoingPorts[i] =	buf[offset + 2*NUMROUTERS];
		table->destinationPorts[i] =	buf[offset + 3*NUMROUTERS];
		offset++;
	}
}

/* outputTable()
 *
 * Writes the routing table to its output file.
 */
void outputTable(struct router *table) {
	FILE *f = NULL;
        int timeBufferSize = 64;
        char timeBuffer[timeBufferSize];

	switch (table->index) {
		case INDEXA:
			f = fopen("routing-outputA.txt", "a");
			break;
		case INDEXB:
			f = fopen("routing-outputB.txt", "a");
			break;
		case INDEXC:
			f = fopen("routing-outputC.txt", "a");
			break;
		case INDEXD:
			f = fopen("routing-outputD.txt", "a");
			break;
		case INDEXE:
			f = fopen("routing-outputE.txt", "a");
			break;
		case INDEXF:
			f = fopen("routing-outputF.txt", "a");
			break;
	}

	
    time_t ltime;
    struct tm* tm_info;

    time(&ltime);
    tm_info = localtime(&ltime);
    snprintf(timeBuffer, timeBufferSize, "%ld", tm_info);
    fprintf(f, "Timestamp: %s\nDestination, Cost, Outgoing Port, Destination Port\n", timeBuffer);

	int i;
    for (i=0; i<NUMROUTERS; i++) {
    	fprintf(f, "%c %i %i %i\n",
    		table->otherRouters[i],
    		table->costs[i],
    		table->outgoingPorts[i],
    		table->destinationPorts[i]);
    }
    return;
}

/* updateTable()
 *
 * Updates table if possible. If table is changed, output to file.
 */
void updateTable(struct router *currTable, struct router rcvdTable) {
	bool isChanged = false;
	int i;
	for (i=0; i<NUMROUTERS; i++) {
		// ignore own entry in table
		if (i != currTable->index) {
			// find shortest paths to other routers
			if (rcvdTable.costs[i] == INT_MAX) {
				continue;
			} else if ( currTable->costs[i] > rcvdTable.costs[i] + currTable->costs[rcvdTable.index] ) {

				currTable->otherRouters[i] = rcvdTable.otherRouters[i];
				currTable->costs[i] = rcvdTable.costs[i] + currTable->costs[rcvdTable.index];
				currTable->outgoingPorts[i] = rcvdTable.index + 10000;
				currTable->destinationPorts[i] = rcvdTable.destinationPorts[i];

				isChanged = true;
			}
		}
	}
	if (isChanged) {
		outputTable(currTable);
	}
	return;
}

/* initializeOutputFiles()
 *
 * Initializes the routing-outputX.txt files from the routing tables.
 */
void initializeOutputFiles(struct router *network) {
		char routerLetters[6] = "ABCDEF";

    	int tableIndex = 0, timeBufferSize = 64, writeBufferSize = 1024;

    	char timeBuffer[timeBufferSize];
		char writeBuffer[writeBufferSize];

        for ( ; tableIndex < 6; tableIndex++ ) {
                char path[20];
                snprintf(path, sizeof(path), "routing-output%c%s\0", routerLetters[tableIndex], ".txt");

                FILE *f = fopen(path, "w");

                if (f == NULL)
                        error("Error opening file");
                // Timestamp
                time_t ltime;
                struct tm* tm_info;

                time(&ltime);
                tm_info = localtime(&ltime);
                snprintf(timeBuffer, timeBufferSize, "%ld", tm_info);
                fprintf(f, "Timestamp: %s\nDestination, Cost, Outgoing Port, Destination Port\n", timeBuffer);
                int i;
                for (i=0; i<NUMROUTERS; i++) {
                        fprintf(f, "%c %i %i %i\n",
                        network[tableIndex].otherRouters[i],
                        network[tableIndex].costs[i],
                        network[tableIndex].outgoingPorts[i],
                        network[tableIndex].destinationPorts[i]);
                }
                fclose(f);
        }
        return;
}

/* initializeFromFile()
 *
 * Initializes the routing tables from the information in 'sample.txt'.
 */
struct matrix initializeFromFile(struct router *table) {
	FILE *f = fopen("sample.txt", "r");
	if (f == NULL)
		error("Error opening sample file");

	int numNeighbors[NUMROUTERS] = {0};

	struct flatmatrix neighborMatrix;
	memset(neighborMatrix.r, -1, sizeof(int) * 6);

	char line[12];
	while (fgets(line, 12, f) != NULL) {
		char *ptr = strtok(line, ",");
		if (strcmp(ptr, "A") == 0) {
			ptr = strtok(NULL, ",");
			char *ptrOP = strtok(NULL, ",");
			char *ptrCost = strtok(NULL, ",");
			switch (atoi(ptrOP)) {
				case 10001:
					tableA->otherRouters[atoi(ptrOP)-10000] = 'B';
					numNeighbors[0]++;
					break;
				case 10002:
					tableA->otherRouters[atoi(ptrOP)-10000] = 'C';
					numNeighbors[0]++;
					break;
				case 10003:
					tableA->otherRouters[atoi(ptrOP)-10000] = 'D';
					numNeighbors[0]++;
					break;
				case 10004:
					tableA->otherRouters[atoi(ptrOP)-10000] = 'E';
					numNeighbors[0]++;
					break;
				case 10005:
					tableA->otherRouters[atoi(ptrOP)-10000] = 'F';
					numNeighbors[0]++;
					break;
			}
			tableA->costs[atoi(ptrOP)-10000] = atoi(ptrCost);
			tableA->outgoingPorts[atoi(ptrOP)-10000] = ROUTERA;
			tableA->destinationPorts[atoi(ptrOP)-10000] = atoi(ptrOP);
		} else if (strcmp(ptr, "B") == 0) {
			ptr = strtok(NULL, ",");
			char *ptrOP = strtok(NULL, ",");
			char *ptrCost = strtok(NULL, ",");
			switch (atoi(ptrOP)) {
				case 10000:
					tableB->otherRouters[atoi(ptrOP)-10000] = 'A';
					numNeighbors[1]++;
					break;
				case 10002:
					tableB->otherRouters[atoi(ptrOP)-10000] = 'C';
					numNeighbors[1]++;
					break;
				case 10003:
					tableB->otherRouters[atoi(ptrOP)-10000] = 'D';
					numNeighbors[1]++;
					break;
				case 10004:
					tableB->otherRouters[atoi(ptrOP)-10000] = 'E';
					numNeighbors[1]++;
					break;
				case 10005:
					tableB->otherRouters[atoi(ptrOP)-10000] = 'F';
					numNeighbors[1]++;
					break;
			}
			tableB->costs[atoi(ptrOP)-10000] = atoi(ptrCost);
			tableB->outgoingPorts[atoi(ptrOP)-10000] = ROUTERB;
			tableB->destinationPorts[atoi(ptrOP)-10000] = atoi(ptrOP);		
		} else if (strcmp(ptr, "C") == 0) {
			ptr = strtok(NULL, ",");
			char *ptrOP = strtok(NULL, ",");
			char *ptrCost = strtok(NULL, ",");
			switch (atoi(ptrOP)) {
				case 10000:
					tableC->otherRouters[atoi(ptrOP)-10000] = 'A';
					numNeighbors[2]++;
					break;
				case 10001:
					tableC->otherRouters[atoi(ptrOP)-10000] = 'B';
					numNeighbors[2]++;
					break;
				case 10003:
					tableC->otherRouters[atoi(ptrOP)-10000] = 'D';
					numNeighbors[2]++;
					break;
				case 10004:
					tableC->otherRouters[atoi(ptrOP)-10000] = 'E';
					numNeighbors[2]++;
					break;
				case 10005:
					tableC->otherRouters[atoi(ptrOP)-10000] = 'F';
					numNeighbors[2]++;
					break;
			}
			tableC->costs[atoi(ptrOP)-10000] = atoi(ptrCost);
			tableC->outgoingPorts[atoi(ptrOP)-10000] = ROUTERC;
			tableC->destinationPorts[atoi(ptrOP)-10000] = atoi(ptrOP);
		} else if (strcmp(ptr, "D") == 0) {
			ptr = strtok(NULL, ",");
			char *ptrOP = strtok(NULL, ",");
			char *ptrCost = strtok(NULL, ",");
			switch (atoi(ptrOP)) {
				case 10000:
					tableD->otherRouters[atoi(ptrOP)-10000] = 'A';
					numNeighbors[3]++;
					break;
				case 10001:
					tableD->otherRouters[atoi(ptrOP)-10000] = 'B';
					numNeighbors[3]++;
					break;
				case 10002:
					tableD->otherRouters[atoi(ptrOP)-10000] = 'C';
					numNeighbors[3]++;
					break;
				case 10004:
					tableD->otherRouters[atoi(ptrOP)-10000] = 'E';
					numNeighbors[3]++;
					break;
				case 10005:
					tableD->otherRouters[atoi(ptrOP)-10000] = 'F';
					numNeighbors[3]++;
					break;
			}
			tableD->costs[atoi(ptrOP)-10000] = atoi(ptrCost);
			tableD->outgoingPorts[atoi(ptrOP)-10000] = ROUTERD;
			tableD->destinationPorts[atoi(ptrOP)-10000] = atoi(ptrOP);
		} else if (strcmp(ptr, "E") == 0) {
			ptr = strtok(NULL, ",");
			char *ptrOP = strtok(NULL, ",");
			char *ptrCost = strtok(NULL, ",");
			switch (atoi(ptrOP)) {
				case 10000:
					tableE->otherRouters[atoi(ptrOP)-10000] = 'A';
					numNeighbors[4]++;
					break;
				case 10001:
					tableE->otherRouters[atoi(ptrOP)-10000] = 'B';
					numNeighbors[4]++;
					break;
				case 10002:
					tableE->otherRouters[atoi(ptrOP)-10000] = 'C';
					numNeighbors[4]++;
					break;
				case 10003:
					tableE->otherRouters[atoi(ptrOP)-10000] = 'D';
					numNeighbors[4]++;
					break;
				case 10005:
					tableE->otherRouters[atoi(ptrOP)-10000] = 'F';
					numNeighbors[4]++;
					break;
			}
			tableE->costs[atoi(ptrOP)-10000] = atoi(ptrCost);
			tableE->outgoingPorts[atoi(ptrOP)-10000] = ROUTERE;
			tableE->destinationPorts[atoi(ptrOP)-10000] = atoi(ptrOP);
		} else if (strcmp(ptr, "F") == 0) {
			ptr = strtok(NULL, ",");
			char *ptrOP = strtok(NULL, ",");
			char *ptrCost = strtok(NULL, ",");
			switch (atoi(ptrOP)) {
				case 10000:
					tableF->otherRouters[atoi(ptrOP)-10000] = 'A';
					numNeighbors[5]++;
					break;
				case 10001:
					tableF->otherRouters[atoi(ptrOP)-10000] = 'B';
					numNeighbors[5]++;
					break;
				case 10002:
					tableF->otherRouters[atoi(ptrOP)-10000] = 'C';
					numNeighbors[5]++;
					break;
				case 10003:
					tableF->otherRouters[atoi(ptrOP)-10000] = 'D';
					numNeighbors[5]++;
					break;
				case 10004:
					tableF->otherRouters[atoi(ptrOP)-10000] = 'E';
					numNeighbors[5]++;
					break;
			}
			tableF->costs[atoi(ptrOP)-10000] = atoi(ptrCost);
			tableF->outgoingPorts[atoi(ptrOP)-10000] = ROUTERF;
			tableF->destinationPorts[atoi(ptrOP)-10000] = atoi(ptrOP);
		}
	}

	int index=0;
	int i;
	for (i=0; i<NUMROUTERS; i++) {
		if (tableA->costs[i] != INT_MAX && tableA->costs[i] != 0) {
			neighborMatrix.r[0][index] = i;
			index++;
		}
	}
	index=0;
	for (i=0; i<NUMROUTERS; i++) {
		if (tableB->costs[i] != INT_MAX && tableB->costs[i] != 0) {
			neighborMatrix.r[1][index] = i;
			index++;
		}
	}
	index=0;
	for (i=0; i<NUMROUTERS; i++) {
		if (tableC->costs[i] != INT_MAX && tableC->costs[i] != 0) {
			neighborMatrix.r[2][index] = i;
			index++;
		}
	}
	index=0;
	for (i=0; i<NUMROUTERS; i++) {
		if (tableD->costs[i] != INT_MAX && tableD->costs[i] != 0) {
			neighborMatrix.r[3][index] = i;
			index++;
		}
	}
	index=0;
	for (i=0; i<NUMROUTERS; i++) {
		if (tableE->costs[i] != INT_MAX && tableE->costs[i] != 0) {
			neighborMatrix.r[4][index] = i;
			index++;
		}
	}
	index=0;
	for (i=0; i<NUMROUTERS; i++) {
		if (tableF->costs[i] != INT_MAX && tableF->costs[i] != 0) {
			neighborMatrix.r[5][index] = i;
			index++;
		}
	}
	
	return neighborMatrix;
}

int main(int argc, char *argv[])
{
	int sockfd[6]; /* socket */
	int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr[6]; /* server's address */
	struct sockaddr_in clientaddr; /* client's address */
	struct hostent *hostp; /* client host info */
	int buf[BUFSIZE]; /* message buffer */
	char *hostaddrp; /* dotted decimal host address string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
	fd_set socks;

	if (argc < 2)
	{
		error("incorrect usage!");
		exit(1);
	}

	int index = atoi(argv[1]);

	struct router table;
	switch (index)
	{
		case INDEXA:
			table = {
				INDEXA,
				{   'A',     NULL,    NULL,    NULL,    NULL,    NULL   },
				{    0,     INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX },
				{ ROUTERA,   NULL,    NULL,    NULL,    NULL,    NULL   },
				{ ROUTERA,   NULL,    NULL,    NULL,    NULL,    NULL   }
			};
			break;
		case INDEXB:
			table = {
				INDEXB,
				{  NULL,     'B',    NULL,    NULL,    NULL,    NULL    },
				{ INT_MAX,    0,    INT_MAX, INT_MAX, INT_MAX, INT_MAX  },
				{  NULL,   ROUTERB,   NULL,    NULL,    NULL,    NULL   },
				{  NULL,   ROUTERB,   NULL,    NULL,    NULL,    NULL   }
			};
			break;
		case INDEXC:
			table = {
				INDEXC,
				{  NULL,     NULL,     'C',      NULL,    NULL,    NULL   },
				{ INT_MAX, INT_MAX,     0,     INT_MAX, INT_MAX, INT_MAX  },
				{  NULL,     NULL,   ROUTERC,    NULL,    NULL,    NULL   },
				{  NULL,     NULL,   ROUTERC,    NULL,    NULL,    NULL   }
			};
			break;
		case INDEXD:
			table = {
				INDEXD,
				{  NULL,     NULL,    NULL,    'D',     NULL,    NULL    },
				{ INT_MAX, INT_MAX, INT_MAX,    0,    INT_MAX, INT_MAX   },
				{  NULL,     NULL,    NULL,  ROUTERD,    NULL,    NULL   },
				{  NULL,     NULL,    NULL,  ROUTERD,    NULL,    NULL   }
			};
			break;
		case INDEXE:
			table = {
				INDEXE,
				{  NULL,     NULL,    NULL,    NULL,    'E',    NULL     },
				{ INT_MAX, INT_MAX, INT_MAX, INT_MAX,    0,    INT_MAX   },
				{  NULL,     NULL,    NULL,    NULL,  ROUTERE,    NULL   },
				{  NULL,     NULL,    NULL,    NULL,  ROUTERE,    NULL   }
			};
			break;
		case INDEXF:
			table = {
				INDEXF,
				{  NULL,     NULL,    NULL,    NULL,    NULL,     'F'   },
				{ INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX,    0     },
				{  NULL,     NULL,    NULL,    NULL,    NULL,  ROUTERF  },
				{  NULL,     NULL,    NULL,    NULL,    NULL,  ROUTERF  }
			};
			break;
	}

//	struct matrix neighborMatrix = initializeFromFile(&tableA, &tableB, &tableC, &tableD, &tableE, &tableF);	
	/* end testing */

	struct flatmatrix neighborMatrix = initializeFromFile(&table);

	struct router *network = malloc(NUMROUTERS * sizeof(struct router));
	network[0] = tableA;
	network[1] = tableB;
	network[2] = tableC;
	network[3] = tableD;
	network[4] = tableE;
	network[5] = tableF;

	initializeOutputFiles(network);
	int i;
	for (i=0; i<NUMROUTERS; i++) {
		/* create parent socket */
		if ( (sockfd[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
			error("Error opening socket");

		/* server can be rerun immediately after killed */
		optval = 1;
		setsockopt(sockfd[i], SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

		/* build server's Internet address */
		bzero((char *) &serveraddr[i], sizeof(serveraddr[i]));
		serveraddr[i].sin_family = AF_INET;
		serveraddr[i].sin_addr.s_addr = htonl(INADDR_ANY);

		switch(i+10000) {
			case ROUTERA:
				serveraddr[i].sin_port = htons(ROUTERA);
				break;
			case ROUTERB:
				serveraddr[i].sin_port = htons(ROUTERB);
				break;
			case ROUTERC:
				serveraddr[i].sin_port = htons(ROUTERC);
				break;
			case ROUTERD:
				serveraddr[i].sin_port = htons(ROUTERD);
				break;
			case ROUTERE:
				serveraddr[i].sin_port = htons(ROUTERE);
				break;
			case ROUTERF:
				serveraddr[i].sin_port = htons(ROUTERF);
				break;
		}
		/* bind: associate parent socket with port */
		if (bind(sockfd[i], (struct sockaddr *) &serveraddr[i], sizeof(serveraddr[i])) < 0)
			error("Error on binding");
	}

	clientlen = sizeof(clientaddr);
	int serverlen = sizeof(serveraddr[0]);

	/* begin by having ROUTERA send DV to one neighbor */
	/* for this implementation, ROUTERA will send to ROUTERB */
	int start;
	for (i=0; i<NUMROUTERS; i++) {
		if (neighborMatrix.r[0][i] != -1) {
			start = neighborMatrix.r[0][i];
			break;
		}
	}
	/* in this case, start=1 */
/*
	printf("here is the router\n");
	printRouter(&tableA);
	*/
	tableToBuffer(&tableA, &buf);
//
	/*
	struct router back;
	bufferToTable(buf, &back);
	printf("here is the router as a buffer\n");
	printBuffer(buf);
	printf("here is the router again\n");
	printRouter(&back);
	exit(0);
	*/
	n = sendto(sockfd[0], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[start], clientlen);

	int nsocks = max(sockfd[0], sockfd[1]);
	for (i=2; i<6; i++) {
		nsocks = max(nsocks, sockfd[i]);
	}

	/* loop: wait for datagram, then echo it */
	while (1) {
		printf("\n\n### Starting next iteration of while loop ###\n\n");
		FD_ZERO(&socks);
		for (i=0; i<NUMROUTERS; i++) {
			FD_SET(sockfd[i], &socks);
		}
		
		if (select(nsocks+1, &socks, NULL, NULL, NULL) < 0) {
			printf("Error selecting socket\n");
		} else {
			/* receives UDP datagram from client */
			memset(buf, 0, BUFSIZE);
			if (FD_ISSET(sockfd[0], &socks)) {
				if ( (n = recvfrom(sockfd[0], buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen)) < 0 )
					error("Error receiving datagram from client\n");

				struct router compTable;
				bufferToTable(&buf, &compTable);

				printf("Received table:\n");
				printRouter(&compTable);
				printf("A's original table:\n");
				printRouter(&tableA);

				updateTable(&tableA, compTable);

				printf("A's updated table:\n");
				printRouter(&tableA);

				tableToBuffer(&tableA, &buf);

				for (i=0; i<NUMROUTERS; i++) {
					if (neighborMatrix.r[0][i] != -1) {
						n = sendto(sockfd[0], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[neighborMatrix.r[0][i]], clientlen);
						if (n < 0)
							error("Error sending to client");
						printf("Reached here A - and neighborMatrix[0][%i]: Value = %i\n", i, neighborMatrix.r[0][i]);
					}
				}
				// n = sendto(sockfd[0], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[1], clientlen);
				// break;
			}

			// else
			if (FD_ISSET(sockfd[1], &socks)) {
				printf("8\n");
				if ( (n = recvfrom(sockfd[1], buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen)) < 0 )
				{
					error("Error receiving datagram from client\n");
					printf("error\n");
				}

				struct router compTable;
/*
				printBuffer(buf);
				printf("%d %d\n\n\n", sizeof(buf), sizeof(compTable));
*/
				
				bufferToTable(&buf, &compTable);

				printf("Received table:\n");
				printRouter(&compTable);
				printf("B's original table:\n");
				printRouter(&tableB);

				updateTable(&tableB, compTable);

				printf("B's updated table:\n");
				printRouter(&tableB);

				tableToBuffer(&tableB, &buf);

				for (i=0; i<NUMROUTERS; i++) {
					if (neighborMatrix.r[1][i] != -1) {
						n = sendto(sockfd[1], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[neighborMatrix.r[1][i]], clientlen);
						if (n < 0)
							error("Error sending to client");
						printf("Reached here B - and neighborMatrix[1][%i]: Value = %i\n", i, neighborMatrix.r[1][i]);
					}
				}
				// n = sendto(sockfd[1], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[0], clientlen);
			}


			// else
			if (FD_ISSET(sockfd[2], &socks)) {
				if ( (n = recvfrom(sockfd[2], buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen)) < 0 )
					error("Error receiving datagram from client\n");

				struct router compTable;

				bufferToTable(&buf, &compTable);

				printf("Received table:\n");
				printRouter(&compTable);
				printf("C's original table:\n");
				printRouter(&tableC);

				updateTable(&tableC, compTable);

				printf("C's updated table:\n");
				printRouter(&tableC);

				tableToBuffer(&tableC, &buf);

				for (i=0; i<NUMROUTERS; i++) {
					if (neighborMatrix.r[2][i] != -1) {
						n = sendto(sockfd[2], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[neighborMatrix.r[2][i]], clientlen);
						if (n < 0)
							error("Error sending to client");
						printf("Reached here C - and neighborMatrix[2][%i]: Value = %i\n", i, neighborMatrix.r[2][i]);
					}
				}
				// n = sendto(sockfd[2], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[1], clientlen);
				// break;
			}
			//else
			if (FD_ISSET(sockfd[3], &socks)) {
				if ( (n = recvfrom(sockfd[3], buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen)) < 0 )
					error("Error receiving datagram from client\n");

				struct router compTable;

				bufferToTable(&buf, &compTable);

				printf("Received table:\n");
				printRouter(&compTable);
				printf("D's original table:\n");
				printRouter(&tableD);

				updateTable(&tableD, compTable);

				printf("D's updated table:\n");
				printRouter(&tableD);

				tableToBuffer(&tableD, &buf);
				
				for (i=0; i<NUMROUTERS; i++) {
					if (neighborMatrix.r[3][i] != -1) {
						n = sendto(sockfd[3], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[neighborMatrix.r[3][i]], clientlen);
						if (n < 0)
							error("Error sending to client");
						printf("Reached here D - and neighborMatrix[3][%i]: Value = %i\n", i, neighborMatrix.r[3][i]);
					}
				}
				// n = sendto(sockfd[2], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[1], clientlen);
				// n = sendto(sockfd[3], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[2], clientlen);
				// if (n < 0)
				// 	error("Error sending to client");
			}
			// else
			if (FD_ISSET(sockfd[4], &socks)) {
				if ( (n = recvfrom(sockfd[4], buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen)) < 0 )
					error("Error receiving datagram from client\n");

				struct router compTable;

				bufferToTable(&buf, &compTable);

				printf("Received table:\n");
				printRouter(&compTable);
				printf("E's original table:\n");
				printRouter(&tableE);

				updateTable(&tableE, compTable);

				printf("E's updated table:\n");
				printRouter(&tableE);

				tableToBuffer(&tableE, &buf);

				for (i=0; i<NUMROUTERS; i++) {
					if (neighborMatrix.r[4][i] != -1) {
						n = sendto(sockfd[4], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[neighborMatrix.r[4][i]], clientlen);
						if (n < 0)
							error("Error sending to client");
						printf("Reached here E - and neighborMatrix[4][%i]: Value = %i\n", i, neighborMatrix.r[4][i]);
					}
				}
				// n = sendto(sockfd[4], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[1], clientlen);
				// if (n < 0)
				// 	error("Error sending to client");
			}
			// else
			if (FD_ISSET(sockfd[5], &socks)) {
				if ( (n = recvfrom(sockfd[5], buf, BUFSIZE, 0, (struct sockaddr *)&clientaddr, &clientlen)) < 0 )
					error("Error receiving datagram from client\n");

				struct router compTable;

				bufferToTable(&buf, &compTable);

				printf("Received table:\n");
				printRouter(&compTable);
				printf("F's original table:\n");
				printRouter(&tableF);

				updateTable(&tableF, compTable);

				printf("F's updated table:\n");
				printRouter(&tableF);

				tableToBuffer(&tableF, &buf);

				for (i=0; i<NUMROUTERS; i++) {
					if (neighborMatrix.r[5][i] != -1) {
						n = sendto(sockfd[5], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[neighborMatrix.r[5][i]], clientlen);
						if (n < 0)
							error("Error sending to client");
						printf("Reached here F - and neighborMatrix[5][%i]: Value = %i\n", i, neighborMatrix.r[5][i]);
					}
				}
				// n = sendto(sockfd[5], buf, sizeof(struct router), 0, (struct sockaddr *)&serveraddr[2], clientlen);
				// if (n < 0)
				// 	error("Error sending to client");
			}


			if (stableState(network)) {
				break;
			}

		}
	}
}