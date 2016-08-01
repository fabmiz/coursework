// This program simulates the flow of heat through a two-dimensional plate.
// The number of grid cells used to model the plate as well as the number of
//  iterations to simulate can be specified on the command-line as follows:
//  ./heated_plate_sequential <columns> <rows> <iterations>
// For example, to execute with a 500 x 500 grid for 250 iterations, use:
//  ./heated_plate_sequential 500 500 250

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "mpi.h"

// Define the immutable boundary conditions and the inital cell value
#define TOP_BOUNDARY_VALUE 0.0
#define BOTTOM_BOUNDARY_VALUE 100.0
#define LEFT_BOUNDARY_VALUE 0.0
#define RIGHT_BOUNDARY_VALUE 100.0
#define INITIAL_CELL_VALUE 50.0
#define hotSpotRow 4500
#define hotSptCol 6500
#define hotSpotTemp 1000
#define NUMROWS 10000
#define NUMCOLS 10000
#define MASTER 0

// Function prototypes
void print_cells(float **cells, int n_x, int n_y);
void initialize_cells(float **cells, int n_x, int n_y);
void create_snapshot(float **cells, int n_x, int n_y, int id);
float **allocate_cells(int n_x, int n_y);
void die(const char *error);

float **cells[2];
int tag,rank, comm_size,chunk_size;
int cur_cells_index;	//current cell index
int next_cells_index;	//next cell index

//PartialHeatPlate routine computes
//a part of the 2D plane in the amount of iterations
//given as CL arguments to this code

//PartialHeatPlate algorithm. In each iteration,
//0) If current iteration is divisible by iter_per_snapshot:
//wait to receive permission to write to the iteration file.
//1) Compute the boundary cells/rows (bottom and top, bottom, top)
//2) In asynchronous mode, post sends and recvs for the afore-computed boundaries
//3) Compute internal cell values.
//4) Wait on all posted sends and receives to complete.

//Note: For simplicity and ease of debugging;
//the code for simulation of last, MASTER, and middle rows
//are purposely separated. Though, they follow the same algorithm
//outline above.

void PartialHeatPlate(int iters_per_cell, 
					  int iters_per_snapshot, 
					  int bndry_thickness, 
					  int iterations) 
{

	
	int k,y,x;
	//len is the length of status and requests handle arrays
	int len 		 = (rank == 0 || rank == comm_size - 1) ? 2 : 4;
	int my_turn = (rank == MASTER) ? 1: 0;
	int rows = (rank == comm_size - 1 || rank == MASTER) ? chunk_size + bndry_thickness: chunk_size + (bndry_thickness * 2);	
	
	//Compute the location of the hotspot
	int rank_with_hotspot =  hotSpotRow / chunk_size;
    int hotspot_row       =   hotSpotRow % chunk_size;
	int iters;
	//Initialize MPI status and request arrays for Waitall routine	
	MPI_Status status,req_status[len];
	MPI_Request send_recv_requests[len];
	
	for( iters = 1; iters <= iterations; iters++ ) {
//0) If current iteration is divisible by iter_per_snapshot:
//wait to receive permission to write to the iteration file.
		
		if (iters % iters_per_snapshot == 0) {	
			if (rank != MASTER) MPI_Recv(&my_turn,1,MPI_INT,rank-1,tag,MPI_COMM_WORLD, &status);
			if ( my_turn == 1) create_snapshot(cells[0], NUMCOLS, rows, iters);
			if (rank != comm_size - 1) MPI_Send(&my_turn,1, MPI_INT, rank+1, tag, MPI_COMM_WORLD);
			if(rank == comm_size - 1) {
			}
		}
		
		if( rank != MASTER && rank != comm_size - 1){
			
//1) Compute the boundary cells/rows (bottom and top, bottom, top)
			for (y = bndry_thickness; y < bndry_thickness * 2; y++) {
				for (x = 1; x <= NUMCOLS; x++) {
					cells[next_cells_index][y][x] = (cells[cur_cells_index][y][x - 1]  +
													 cells[cur_cells_index][y][x + 1]  +
													 cells[cur_cells_index][y - 1][x]  +
													 cells[cur_cells_index][y + 1][x]) * 0.25;
				}
			}
			
			for (y = chunk_size; y < chunk_size + bndry_thickness; y++) {
				for (x = 1; x <= NUMCOLS; x++) {
					cells[next_cells_index][y][x] = (cells[cur_cells_index][y][x - 1]  +
													 cells[cur_cells_index][y][x + 1]  +
													 cells[cur_cells_index][y - 1][x]  +
													 cells[cur_cells_index][y + 1][x]) * 0.25;
				}
			}
	
//2) In asynchronous mode, post sends and recvs for the afore-computed boundaries
			MPI_Isend(&cells[cur_cells_index][chunk_size][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank + 1, iters, MPI_COMM_WORLD, &send_recv_requests[0]);		
			MPI_Isend(&cells[cur_cells_index][bndry_thickness][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank - 1, iters, MPI_COMM_WORLD, &send_recv_requests[1]);		
			MPI_Irecv(&cells[cur_cells_index][chunk_size][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank + 1, iters, MPI_COMM_WORLD, &send_recv_requests[2]);		
			MPI_Irecv(&cells[cur_cells_index][0][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank - 1, iters, MPI_COMM_WORLD, &send_recv_requests[3]);		
		
//3) Compute internal cell values.
			for(k=0; k < iters_per_cell ;k++) {
				for (y = 2*bndry_thickness; y < chunk_size; y++) {
					for (x = 1; x <= NUMCOLS; x++) {
						cells[next_cells_index][y][x] = (cells[cur_cells_index][y][x - 1]  +
												   cells[cur_cells_index][y][x + 1]  +
												   cells[cur_cells_index][y - 1][x]  +
												   cells[cur_cells_index][y + 1][x]) * 0.25;
					}
				}
			}
			
			cur_cells_index = next_cells_index;
			next_cells_index = !cur_cells_index;
			if(rank == rank_with_hotspot) cells[cur_cells_index][hotspot_row][hotSptCol]=hotSpotTemp;
//4) Wait on all posted sends and receives to complete.
			MPI_Waitall(len,send_recv_requests,req_status);

		}



		if( rank == comm_size - 1){
			
			for (y = bndry_thickness; y < bndry_thickness * 2; y++) {
				for (x = 1; x <= NUMCOLS; x++) {
					cells[next_cells_index][y][x] = (cells[cur_cells_index][y][x - 1]  +
													 cells[cur_cells_index][y][x + 1]  +
													 cells[cur_cells_index][y - 1][x]  +
													 cells[cur_cells_index][y + 1][x]) * 0.25;
				}
			}
			
		MPI_Isend(&cells[cur_cells_index][bndry_thickness][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank - 1, iters, MPI_COMM_WORLD, &send_recv_requests[0]);		  MPI_Irecv(&cells[cur_cells_index][0][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank - 1, iters, MPI_COMM_WORLD, &send_recv_requests[1]);		
		
			for(k=0; k < iters_per_cell ;k++) {
				for (y = 2*bndry_thickness; y < chunk_size; y++) {
					for (x = 1; x <= NUMCOLS; x++) {
						cells[next_cells_index][y][x] = (cells[cur_cells_index][y][x - 1]  +
												   cells[cur_cells_index][y][x + 1]  +
												   cells[cur_cells_index][y - 1][x]  +
												   cells[cur_cells_index][y + 1][x]) * 0.25;
					}
				}
			}
			
			cur_cells_index = next_cells_index;
			next_cells_index = !cur_cells_index;
			
			MPI_Waitall(len,send_recv_requests,req_status);

		}


		if( rank == MASTER){
			
			for (y = chunk_size - 1; y < chunk_size + bndry_thickness; y++) {
				for (x = 1; x <= NUMCOLS; x++) {
					cells[next_cells_index][y][x] = (cells[cur_cells_index][y][x - 1]  +
													 cells[cur_cells_index][y][x + 1]  +
													 cells[cur_cells_index][y - 1][x]  +
													 cells[cur_cells_index][y + 1][x]) * 0.25;
				}
			}
	
		MPI_Isend(&cells[cur_cells_index][chunk_size - 1][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank + 1, iters, MPI_COMM_WORLD, &send_recv_requests[0]);		  MPI_Irecv(&cells[cur_cells_index][chunk_size + 1][0], NUMCOLS * bndry_thickness, MPI_FLOAT, rank + 1, iters, MPI_COMM_WORLD, &send_recv_requests[1]);
			for(k=0; k < iters_per_cell ;k++) {
				for (y = 1; y <= chunk_size - bndry_thickness; y++) {
					for (x = 1; x <= NUMCOLS; x++) {
						cells[next_cells_index][y][x] = (cells[cur_cells_index][y][x - 1]  +
												   cells[cur_cells_index][y][x + 1]  +
												   cells[cur_cells_index][y - 1][x]  +
												   cells[cur_cells_index][y + 1][x]) * 0.25;
					}
				}
			}
			
			cur_cells_index = next_cells_index;
			next_cells_index = !cur_cells_index;
			
			MPI_Waitall(len,send_recv_requests,req_status);

		}

	}

}

int main(int argc, char **argv) {
	
	// Initialize MPI environment
	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD,&comm_size);
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Status status;


	int rc;
	// Record the start time of the program
	double start_time, end_time; 
	start_time = MPI_Wtime();
	//Initialize general purpose mpi tag for sends and receives.
	tag = 1;
		
	// Extract the input parameters from the command line arguments
	
	// Number of iterations to simulate (default = 100)
	int iterations 	= (argc > 1) ? atoi(argv[1]) : 100;
	// Number of chunks
	int y_dim = (argc > 2) ? atoi(argv[2]) : 20;      
	// Number of iterations per cell
	int iters_per_cell = (argc > 3) ? atoi(argv[3]) : 1 ;	
	// Number of iterations per snapshot 
	int iters_per_snapshot = (argc > 4) ? atoi(argv[4]) : 1000; 
	// Boundary thickness 
	int bndry_thickness = (argc > 5) ? atoi(argv[5]) : 1; 
	
	cur_cells_index = 0;
	next_cells_index = 1;

	//Compute the number of rows assigned to each core/MPI task.	
	chunk_size = NUMROWS / y_dim; 	
 
	printf("y_dim: %d, iters_per_cell: %d, iters_per_snapshot: %d, boundary_thickness: %d, iterations: %d\n", 
			y_dim, iters_per_cell, iters_per_snapshot, bndry_thickness, iterations);
	int my_rows, x, y, i;

	//Each rank initializes its own chunk of the cells/rows.
	//as the MASTER rank (0) and the last rank have different have additional
	//rows, they are initialized differently.
	if(rank != MASTER && rank != comm_size - 1 ) {
		my_rows  = chunk_size + (2 * bndry_thickness); 
		cells[0] = allocate_cells(NUMCOLS + 2, my_rows);
		cells[1] = allocate_cells(NUMCOLS + 2, my_rows);
		initialize_cells(cells[0], NUMCOLS, my_rows);
		for (y = 0; y < my_rows; y++) cells[0][y][0] = cells[1][y][0] = LEFT_BOUNDARY_VALUE;
		for (y = 0; y < my_rows; y++) cells[0][y][NUMCOLS + 1] = cells[1][y][NUMCOLS + 1] = RIGHT_BOUNDARY_VALUE;
	}
	
	else if (rank == comm_size - 1) {
		my_rows  = chunk_size + bndry_thickness;
		cells[0] = allocate_cells(NUMCOLS + 2, my_rows + 1);
		cells[1] = allocate_cells(NUMCOLS + 2, my_rows + 1);
		initialize_cells(cells[0], NUMCOLS, my_rows);
		for (x = 1; x <= NUMCOLS ; x++) cells[0][my_rows][x]     = cells[1][my_rows][x] 	   = BOTTOM_BOUNDARY_VALUE;
		for (y = 0; y <= my_rows; y++) cells[0][y][0] 		    = cells[1][y][0] 		   = LEFT_BOUNDARY_VALUE;
		for (y = 0; y <= my_rows; y++) cells[0][y][NUMCOLS + 1]  = cells[1][y][NUMCOLS + 1] = RIGHT_BOUNDARY_VALUE;
	}
	
	else {	
		// We allocate two arrays: one for the current time step and one for the next time step.
		// At the end of each iteration, we switch the arrays in order to avoid copying.
		// The arrays are allocated with an extra surrounding layer which contains
		//  the immutable boundary conditions (this simplifies the logic in the inner loop).
		my_rows  = chunk_size + bndry_thickness;
		cells[0] = allocate_cells(NUMCOLS + 2, my_rows + 1);
		cells[1] = allocate_cells(NUMCOLS + 2, my_rows + 1);
		// Initialize the interior (non-boundary) cells to their initial value.
		// Note that we only need to initialize the array for the current time
		//  step, since we will write to the array for the next time step
		//  during the first iteration.
		initialize_cells(cells[0], NUMCOLS, my_rows);
		
		// Set the immutable boundary conditions in both copies of the array
		for (x = 0; x <= NUMCOLS ; x++) cells[0][0][x] 	    	= cells[1][0][x] 		   = TOP_BOUNDARY_VALUE;
		for (y = 1; y <= my_rows; y++)  cells[0][y][0] 		    = cells[1][y][0] 		   = LEFT_BOUNDARY_VALUE;
		for (y = 1; y <= my_rows; y++)  cells[0][y][NUMCOLS + 1] = cells[1][y][NUMCOLS + 1]  = RIGHT_BOUNDARY_VALUE;
	}

	//After successfully initializing chunk
	//Compute partial heated plate (HALO) on chunk and perform boundary exchanges as needed.
	PartialHeatPlate(iters_per_cell, iters_per_snapshot,bndry_thickness, iterations);
	
	// Compute and output the execution time
  	// wait for all ranks to complete work. 	
	MPI_Barrier(MPI_COMM_WORLD);	

	if(rank == MASTER) {	

		end_time = MPI_Wtime();
		printf("\nRank: %d. Execution time: %.3f seconds\n", rank, end_time - start_time);
		printf("\nRank: %d. Total IO time: %.3f seconds\n", rank, total_io_time);

	}
	MPI_Finalize();
	return 0;
}


// Allocates and returns a pointer to a 2D array of floats
float **allocate_cells(int num_cols, int num_rows) {
	float **array = (float **) malloc(num_rows * sizeof(float *));
	if (array == NULL) die("Error allocating array!\n");
	
	array[0] = (float *) malloc(num_rows * num_cols * sizeof(float));
	if (array[0] == NULL) die("Error allocating array!\n");

	int i;
	for (i = 1; i < num_rows; i++) {
		array[i] = array[0] + (i * num_cols);
	}

	return array;
}


// Sets all of the specified cells to their initial value.
// Assumes the existence of a one-cell thick boundary layer.
// initialize_cells is slightly modified to accommodate the differences
// between MASTER, middle, and last rank initializations.
void initialize_cells(float **cells, int num_cols, int num_rows) {
	int x, y;
	int y_start = (rank != 0) ? 0 : 1;
    int y_end   = (rank != 0) ? num_rows :  num_rows+1;
	for (y = y_start; y < y_end; y++) {
		for (x = 1; x <= num_cols; x++) {
			cells[y][x] = INITIAL_CELL_VALUE;
		}
	}
}


// Creates a snapshot of the current state of the cells in PPM format.
// The plate is scaled down so the image is at most 1,000 x 1,000 pixels.
// This function assumes the existence of a boundary layer, which is not
//  included in the snapshot (i.e., it assumes that valid array indices
//  are [1..NUMROWS][1..NUMCOLS]).
// initialize_cells is slightly modified to allow MASTER to start the
// initialization/writing of the ppm file with RGB info and to accommodate
// differences in indices between last, MASTER, and middle ranks.
void create_snapshot(float **cells, int num_cols, int num_rows, int id) {
	int scale_x, scale_y;
	scale_x = scale_y = 1;
	
	// Figure out if we need to scale down the snapshot (to 1,000 x 1,000)
	//  and, if so, how much to scale down
	if (num_cols > 1000) {
		if ((num_cols % 1000) == 0) scale_x = num_cols / 1000;
		else {
			die("Cannot create snapshot for x-dimensions >1,000 that are not multiples of 1,000!\n");
			return;
		}
	}
	if (num_rows > 1000) {
		if ((num_rows % 1000) == 0) scale_y = num_rows / 1000;
		else {
			printf("Cannot create snapshot for y-dimensions >1,000 that are not multiples of 1,000!\n");
			return;
		}
	}
	
	// Open/create the file
	char text[255];
	sprintf(text, "/scratch/fm9ab/snapshot.%d.ppm", id);
	
	FILE *out = (rank == 0)? fopen(text, "w"): fopen(text,"a");
	// Make sure the file was created
	if (out == NULL) {
		printf("Error creating snapshot file!\n");
		return;
	}

	// Write header information to file
	// P3 = RGB values in decimal (P6 = RGB values in binary)
	
	if(rank == 0) fprintf(out, "P3 %d %d 100\n", num_cols / scale_x, num_rows / scale_y);
	
	// Precompute the value needed to scale down the cells
	float inverse_cells_per_pixel = 1.0 / ((float) scale_x * scale_y);
	
	// Write the values of the cells to the file
	int x, y, i, j;

	int y_start = (rank != 0) ? 0 : 1;
    int y_end   = (rank != 0) ? num_rows :  num_rows+1;
	for (y = y_start; y < y_end; y += scale_y) {
		for (x = 1; x <= num_cols; x += scale_x) {
			float sum = 0.0;
			for (j = y; j < y + scale_y; j++) {
				for (i = x; i < x + scale_x; i++) {
					sum += cells[j][i];
				}
			}
			// Write out the average value of the cells we just visited
			int average = (int) (sum * inverse_cells_per_pixel);
			if (average > 100) {
				average = 100;
			}
			fprintf(out, "%d 0 %d\t", average, 100 - average);
		}
		fwrite("\n", sizeof(char), 1, out);
	}
	
	// Close the file
	fclose(out);
}


// Prints the specified error message and then exits
void die(const char *error) {
	printf("%s", error);
	exit(1);
}
