#define NUMCITIES 17
#define THRESHOLD 1500

#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string.h>

// Matrix containing the costs between cities
int costs[NUMCITIES][NUMCITIES];
// Heap and heap data
struct data *heap;
int len;
int size;

// Holds route data, the length of the route, the cost of the route, and the route itself
struct data {
	int len;
	int cost;
	int route[NUMCITIES];
};

//Initialize the heap
struct data* initializeHeap(int s) {
	struct data *h = malloc (s * sizeof(struct data));
	len = 0;
	size = s;
	return h;
}

// Insert an element in the heap
void push(struct data d) {
	if (len+1 >= size) {
		size = size * 2;
		struct data *heap2 = realloc(heap, size * sizeof(struct data));
		heap = heap2;
	}

	int curr = len+1;
	int parent = curr/2;
	while (curr > 1 && heap[parent].cost > d.cost) {
		heap[curr] = heap[parent];
		curr = parent;
		parent = parent/2;
	}
	heap[curr] = d;
	len++;
}

// Pop an element from the heap
struct data pop() {
	struct data d = heap[1];
	heap[1] = heap[len];
	len--;
	int curr = 1;
	int left, right;
	while (1) {
		left = 2*curr;
		right = 2*curr + 1;
		if (left <= len && heap[left].cost < heap[curr].cost) {
			struct data temp = heap[left];
			heap[left] = heap[curr];
			heap[curr] = temp;
			curr = left;
		}

		else if (right <= len && heap[right].cost < heap[curr].cost) {
			struct data temp = heap[right];
			heap[right] = heap[curr];
			heap[curr] = temp;
			curr = right;
		}
		else {
			break;
		}
	}
	return d;
}

// Gets the cost matrix from the given filename
void getInput(char *filename) {
	FILE *in = fopen(filename, "r");
	int i, j;
	for (i = 0; i < NUMCITIES; i++) {
		for (j = 0; j < NUMCITIES; j++) {
			fscanf(in, "%d ", &costs[i][j]);
		}
	}
}

int main(int argc, char **argv) {
	// Initialize MPI
	MPI_Init(&argc, &argv);
	
	// Record the start time of the program
	time_t start_time = time(NULL);
	
	// Get the processor number of the current node
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	
	// Get the total number of nodes
	int ranks; 
	MPI_Comm_size(MPI_COMM_WORLD, &ranks);
	
	// Extract the input parameters from the command line arguments
	if (argc < 2) {
		MPI_Finalize();
	}
	char* filename = argv[1];
	
	// Get the cost matrix from file
	getInput(filename);
	
	// Initialize the heap
	heap = initializeHeap(2000);
	
	// Place to store heap and communication data
	struct data in, out;
	
	// Used to remove segfaults
	MPI_Request myRequest;
	MPI_Status myStatus;
	
	int i;
	
	// Master Node
	if(rank == 0) {
		// Keep track of the local best routes and whether to send the best route
		struct data best[ranks];
		int sendBest = 0;
		
		// If best route has length 0, then no route has been found
		best[0].len = 0;
		
		// Used for synchronization
		MPI_Request workRequests[ranks];
		MPI_Request bestFound[ranks];
		
		// Used to remove segfaults
		MPI_Status myStatuses[ranks];
		
		// Keep track of nodes that currently have work to do
		int activeNodes[ranks];
		
		// Insert routes with the first route
		for (i = 1; i < NUMCITIES; i++) {
			in.len = 2;
			in.cost = costs[0][i];
			in.route[0] = 0;
			in.route[1] = i;
			push(in);
		}
		
		// Fill the heap with some routes so the subproblems passed to workers can remain relatively small
		while (len < THRESHOLD) {
			out = pop();
			int l = out.len;
			
			// If route is completed, compare its cost with the current best route
			if (l == NUMCITIES) {
				int cost = out.cost + costs[out.route[NUMCITIES-1]][0];
				if (best[0].len == 0 || cost < best[0].cost) {
					best[0] = out;
					best[0].cost = cost;
					sendBest = 1;
				}
			}
			
			// Otherwise insert the next city to the route
			else {	
				// Filters out cities that have already been visited
				int next[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
				for (i = 1; i < l; i++) {
					next[out.route[i]] = 0;		
				}
				// Get the last city in the route
				int last = out.route[l-1];
				
				// Insert the next city to the heap provided that the cost is lower then the best route
				for (i = 1; i < NUMCITIES; i++) {
					if (next[i]) {
						in.len = l+1;
						in.cost = out.cost + costs[last][i];
						memcpy(&in.route[0], &out.route[0], l*sizeof(int));
						in.route[l] = i;
						if (best[0].len == 0 || in.cost < best[0].cost) {
							push(in);
						}
					}
				}
			}
		}
		
		// Send the best current route, if a route has been found
		if (sendBest) {
			for (i = 1; i < ranks; i++) {
				MPI_Isend(&best[0], 2+NUMCITIES, MPI_INT, i, 3, MPI_COMM_WORLD, &myRequest);
			}
			sendBest = 0;
		}
		
		// Recieve work request and get best route from nodes
		for (i = 1; i < ranks; i++) {
			MPI_Irecv(&activeNodes[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &workRequests[i]);
			MPI_Irecv(&best[i], 2+NUMCITIES, MPI_INT, i, 1, MPI_COMM_WORLD, &bestFound[i]);
		}
		
		// While heap is not empty
		while (len > 0) {
			// Give work to idle nodes			
			for (i = 1; i < ranks; i++) {
				int flag = 0;
				MPI_Test(&workRequests[i], &flag, &myStatus);
				if (flag && len > 1) {
					out = pop();
					MPI_Send(&out, 2+NUMCITIES, MPI_INT, i, 4, MPI_COMM_WORLD);
					// Get next request
					MPI_Irecv(&activeNodes[i], 1, MPI_INT, i, 0, MPI_COMM_WORLD, &workRequests[i]);					
				}
			}
			
			// Get local best routes from worker nodes
			for (i = 1; i < ranks; i++) {
				int flag = 0;
				MPI_Test(&bestFound[i], &flag, &myStatus);
				if (flag) {
					if (best[i].cost < best[0].cost) {
						best[0] = best[i];
						sendBest = 1;
					}
					MPI_Irecv(&best[i], 2+NUMCITIES, MPI_INT, i, 1, MPI_COMM_WORLD, &bestFound[i]);
				}
			}
			
			// Send the current best route to worker nodes, if best route has been updated
			if (sendBest) {
				for (i = 1; i < ranks; i++) {
					MPI_Isend(&best[0], 2+NUMCITIES, MPI_INT, i, 3, MPI_COMM_WORLD, &myRequest);
				}
				sendBest = 0;
			}
			
			// The branch and bound algorithm
			out = pop();
			int l = out.len;
			
			// If route is completed, compare its cost with the current best route
			if (l == NUMCITIES) {
				int cost = out.cost + costs[out.route[NUMCITIES-1]][0];
				if (best[0].len == 0 || cost < best[0].cost) {
					best[0] = out;
					best[0].cost = cost;
				}
			}
			// Otherwise insert the next city to the route
			else {	
				// Filters out cities that have already been visited
				int next[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
				for (i = 1; i < l; i++) {
					next[out.route[i]] = 0;		
				}
				// Get the last city in the route
				int last = out.route[l-1];
				
				// Insert the next city to the heap provided that the cost is lower then the best route
				for (i = 1; i < NUMCITIES; i++) {
					if (next[i]) {
						in.len = l+1;
						in.cost = out.cost + costs[last][i];
						memcpy(&in.route[0], &out.route[0], l*sizeof(int));
						in.route[l] = i;
						if (best[0].len == 0 || in.cost < best[0].cost) {
							push(in);
						}
					}
				}
			}
			
		}
		// Send termination request to worker nodes
		struct data terminated;
		terminated.cost = -1;
		for (i = 1; i < ranks; i++) {
			MPI_Isend(&terminated, 2+NUMCITIES, MPI_INT, i, 4, MPI_COMM_WORLD, &myRequest);
		}
		
		// Get the best route from the worker nodes
		for (i = 1; i < ranks; i++) {
			MPI_Recv(&best[i], 2+NUMCITIES, MPI_INT, i, 2, MPI_COMM_WORLD, &myStatus);
			if (best[i].len > 0 && best[i].cost < best[0].cost) {
			    best[0] = best[i];
			}
		}
		// Print the cost of the best route
		printf("Cost: %d\n", best[0].cost);
		
		// Print the best route
		printf("Route: ");
		for (i = 0; i < NUMCITIES; i++) {
			printf("%d, ", best[0].route[i]);
		}
		printf("%d\n", best[0].route[0]);
		
		//Get final CPU time
		time_t end_time = time(NULL);
		printf("%d: Execution time: %d seconds\n", rank, (int) difftime(end_time, start_time));
		fflush(stdout);
	}
	
	else {
		// Used for storing the best route the current node has found and the best route from the master node 
		struct data localBest, masterBest;
		localBest.len = 0;
		
		// Used for synchronization
		MPI_Request bestFound;
		
		// Receive best route from master
		MPI_Irecv(&masterBest, 2+NUMCITIES, MPI_INT, 0, 3, MPI_COMM_WORLD, &bestFound);
		
		while (1) {
			// If node has no work, request for work
			if (len == 0) {
				int active = 0;	
				MPI_Isend(&active, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &myRequest);
				MPI_Recv(&in, 2+NUMCITIES, MPI_INT, 0, 4, MPI_COMM_WORLD, &myStatus);
				if (in.cost == -1) {
					break;
				}
				push(in);
			}
			
			// Receive the current best route from the master
			int flag = 0;
			MPI_Test(&bestFound, &flag, &myStatus);
			
			if (flag) { 
				if (masterBest.cost < localBest.cost) {
				  localBest = masterBest;
				}
				MPI_Irecv(&masterBest, 2+NUMCITIES, MPI_INT, 0, 3, MPI_COMM_WORLD, &bestFound);
			}
			
			// The branch and bound algorithm
			out = pop();
			int l = out.len;
			
			// If route is completed, compared its cost with the current best route
			if (l == NUMCITIES) {
				int cost = out.cost + costs[out.route[NUMCITIES-1]][0];
				if (localBest.len == 0 || cost < localBest.cost) {
					localBest = out;
					localBest.cost = cost;
					// Tell the master node that a good route has been found
					MPI_Isend(&localBest, 2+NUMCITIES, MPI_INT, 0, 1, MPI_COMM_WORLD, &myRequest);
				}
			}
			
			// Otherwise insert the next city to the route
			else {		
				// Filters out cities that have already been visited
				int next[] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
				for (i = 1; i < l; i++) {
					next[out.route[i]] = 0;	
				}
				
				// Get the last city in the route
				int last = out.route[l-1];
				// Insert the next city to the heap provided that the cost is lower then the best route
				for (i = 1; i < NUMCITIES; i++) {
					if (next[i]) {
						in.len = l+1;
						in.cost = out.cost + costs[last][i];
						memcpy(&in.route[0], &out.route[0], l*sizeof(int));
						in.route[l] = i;
						if (localBest.len == 0 || in.cost < localBest.cost) {
							push(in);
						}
					}
				}
			}
		}
		// Send the best route to the master node
		MPI_Isend(&localBest, 2+NUMCITIES, MPI_INT, 0, 2, MPI_COMM_WORLD, &myRequest);
		
		//Get final CPU time
		time_t end_time = time(NULL);
		printf("%d: Execution time: %d seconds\n", rank, (int) difftime(end_time, start_time));
		fflush(stdout);
	}
	MPI_Finalize();
}
