/*
 * Swarthmore College, CS 31
 * Copyright (c) 2023 Swarthmore College Computer Science Department,
 * Swarthmore PA
 */

/*
Program that plays Conway's Game of Life in one of three output modes;
takes input from files to determine game details. Also allows for
the user to specify how many threads they would like to run the program on.
*/

/*
 * To run:
 * ./gol file1.txt  0  # run with config file file1.txt, do not print board
 * ./gol file1.txt  1  # run with config file file1.txt, ascii animation
 * ./gol file1.txt  2  # run with config file file1.txt, ParaVis animation
 *
 */
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "colors.h"

/****************** Definitions **********************/
/* Three possible modes in which the GOL simulation can run */
#define OUTPUT_NONE   (0)   // with no animation
#define OUTPUT_ASCII  (1)   // with ascii animation
#define OUTPUT_VISI   (2)   // with ParaVis animation

/* Used to slow down animation run modes: usleep(SLEEP_USECS);
 * Change this value to make the animation run faster or slower
 */
//#define SLEEP_USECS  (1000000)
#define SLEEP_USECS    (100000)

/* A global variable to keep track of the number of live cells in the
 * world (this is the ONLY global variable you may use in your program)
 */
static int total_live = 0;

/* This struct represents all the data you need to keep track of your GOL
 * simulation.  Rather than passing individual arguments into each function,
 * we'll pass in everything in just one of these structs.
 * this is passed to play_gol, the main gol playing loop
 *
 * NOTE: You will need to use the provided fields here, but you'll also
 *       need to add additional fields. (note the nice field comments!)
 * NOTE: DO NOT CHANGE THE NAME OF THIS STRUCT!!!!
 */
struct gol_data {

    // NOTE: DO NOT CHANGE the names of these 4 fields (but USE them)
    int curr_iter; //for visi
    int rows;  // the row dimension
    int cols;  // the column dimension
    int iters; // number of iterations to run the gol simulation
    int output_mode; // set to:  OUTPUT_NONE, OUTPUT_ASCII, or OUTPUT_VISI
    int* gol_board; //represents the current board (base next round on this)
    int *next_board; //the next board to play 
    int threads; //number of threads the user determines
    int ntids; // the identifier for which thread is running
    int part_mode; // A 0/1 flag to specify how to parallelize the GOL program 
    //(0: row-wise grid cell allocation, 1: column-wise grid cell allocation)
    int print_config; // A 0/1 flag to specify should the per-thread board
    // allocation be printed
    int start; // the starting col/row for each thread to run
    int end; // the ending col/row for each thread to run

    /* fields used by ParaVis library (when run in OUTPUT_VISI mode). */
    // NOTE: DO NOT CHANGE their definitions BUT USE these fields
    visi_handle handle;
    color3 *image_buff;
};


/****************** Function Prototypes **********************/

/* the main gol game playing loop (prototype must match this) */
void* play_gol(void * arg);

/* init gol data from the input file and run mode cmdline args */
int init_game_data_from_args(struct gol_data *data, char **argv);

// A mostly implemented function, but a bit more for you to add.
/* print board to the terminal (for OUTPUT_ASCII mode) */
void print_board(struct gol_data *data, int round);

/*Checks a cells neighbors (in the copy board) and returns how many of them are alive*/
int count_neighbors(struct gol_data *data, int i, int j);

/*Updates the playing board based on one iteration of the game*/
void play_round(struct gol_data *data);

//DELETE??
/*Sets the copy array equal to the current state of the playing board*/
//void update_copy(struct gol_data *data);

/* use updated data to set colors for visualization */
void update_colors(struct gol_data *data);

/*initialize board with starting cells*/
void init_board(struct gol_data *data, FILE *file);

void partition (struct gol_data *data);

/**************************************************************/


/************ Definitions for using ParVisi library ***********/
/* initialization for the ParaVisi library (DO NOT MODIFY) */
int setup_animation(struct gol_data* data);
/* register animation with ParaVisi library (DO NOT MODIFY) */
int connect_animation(void (*applfunc)(struct gol_data *data),
        struct gol_data* data);
/* name for visi (you may change the string value if you'd like) */
static char visi_name[] = "GOL!";


/**************************************************************/
 static pthread_mutex_t mutex;
 static pthread_barrier_t barrier;


/************************ Main Function ***********************/
int main(int argc, char **argv) {

    int ret;
    struct gol_data data;
    double secs;
    struct timeval start, stop;
    struct gol_data *targs;
    int ntids;
    pthread_t *tid;
    

    /* check number of command line arguments */
    if (argc < 6){
        printf("usage: %s <infile.txt> <output_mode>[0|1|2]\n", argv[0]); // CHANGE ACCORDINGLY
        printf("(0: no visualization, 1: ASCII, 2: ParaVisi)\n");
        exit(1);
    }

    
    /* Initialize game state (all fields in data) from information
     * read from input file */
    
    //different command line arguments for visi library ()

    ret = init_game_data_from_args(&data, argv);
    if (ret != 0) {
        printf("Initialization error: file %s, mode %s\n", argv[1], argv[2]);
        exit(1);
    }
    
    /* initialize ParaVisi animation (if applicable) */
    if (data.output_mode == OUTPUT_VISI) {
        
        setup_animation(&data);
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_barrier_init(&barrier, NULL, data.threads);

    ntids = atoi(argv[3]);
    // make ntids a sane value if insane
    if ((ntids < 1) || (ntids > 50)) { ntids = 10; }

    tid = malloc(sizeof(pthread_t) * ntids);
    if (!tid) { perror("malloc: pthread_t array"); exit(1); }
    //Malloc the array of the structs needed for parallelization

    targs = malloc (sizeof(struct gol_data)*ntids); 

    ret = gettimeofday(&start, NULL);
        //throw error if cant get the time
    if (ret == -1){
        printf("Error in getting time\n");
        exit(0);
    }

    for (int i = 0; i < ntids; i++){
        targs[i] = data;
        targs[i].ntids = i;

        partition(&targs[i]);

        ret = pthread_create(&tid[i], 0, play_gol, &targs[i]); 
        if (ret) { perror("Error pthread_create\n"); exit(1); }
    }


    /* ASCII output: clear screen & print the initial board */
    if (data.output_mode == OUTPUT_ASCII) {
        if (system("clear")) { perror("clear"); exit(1); }
        print_board(&data, 0);
    }


    if (data.output_mode == OUTPUT_ASCII) { // run with ascii animation
 
        // clear the previous print_board output from the terminal:
        // (NOTE: you can comment out this line while debugging)
        if (system("clear")) { perror("clear"); exit(1); }

        // NOTE: DO NOT modify this call to print_board at the end
        //       (it's to help us with grading your output)

        if (data.ntids == 0){
            print_board(&data, data.iters);
        }
        
        // for (int i = 0; i < ntids; i++){
        //      pthread_join(tid[i], 0);
        // }
        

    }
    else if (data.output_mode == OUTPUT_VISI) {  
        // OUTPUT_VISI: run with ParaVisi animation
        // tell ParaVisi that it should run play_gol
        // connect_animation(play_gol, &data);
        // start ParaVisi animation
        run_animation(data.handle, data.iters);
    } else if (data.output_mode != OUTPUT_NONE){
        printf("Invalid output mode: %d\n", data.output_mode);
        printf("Check your game data initialization\n");
        exit(1);
    }

    for (int i = 0; i < ntids; i++){
        pthread_join(tid[i], 0);
    }
    
    ret = gettimeofday(&stop, NULL);
        
    if (ret == -1){
        printf("Error in getting time\n");
        exit(0);
    }

    if (data.output_mode != OUTPUT_VISI) {

        
        secs = (stop.tv_sec + (stop.tv_usec * .000001)) - (start.tv_sec + (start.tv_usec * .000001)); 
        
        /* Print the total runtime, in seconds. */
        // NOTE: do not modify these calls to fprintf

        fprintf(stdout, "Total time: %0.3f seconds\n", secs);
        fprintf(stdout, "Number of live cells after %d rounds: %d\n\n",
                data.iters, total_live);
    }



    free(data.gol_board);
    free(data.next_board);
    free(targs);
    free(tid);
    targs = NULL;
    tid = NULL;
  
    return 0;
}


/******************** Function Prototypes ************************/

/**************************************************************/
/* initialize the gol game state from command line arguments
 * additionally, fill in relevant struct details from the input txt file. 
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode value
 * data: pointer to gol_data struct to initialize
 * argv: command line args
 *       argv[1]: name of file to read game config state from
 *       argv[2]: run mode
 * returns: 0 on success, 1 on error
 */
int init_game_data_from_args(struct gol_data *data, char **argv) {
    int ret, i;

    FILE *infile; 

    data->curr_iter = 0;

    //Reads in each value from the file

    data->threads = atoi(argv[3]);
    data->part_mode = atoi(argv[4]);
    data->print_config = atoi(argv[5]);

    if(atoi(argv[2]) == 0){
        data->output_mode = OUTPUT_NONE;
    }
    else if (atoi(argv[2]) == 1){
        data->output_mode = OUTPUT_ASCII;
    }
    else if (atoi(argv[2]) == 2){
        data->output_mode = OUTPUT_VISI;
    }

    else{
        printf("ERROR: Invalid Output Mode\n");
        exit(1);
    }
    infile = fopen(argv[1], "r");

    if (infile == NULL){
        printf("Unable to open provided file");
        exit(1);
    }

   
    ret = fscanf(infile, "%d", &data->rows);
    if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
    }


    ret = fscanf(infile, "%d", &data->cols);
    if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
    }


    ret = fscanf(infile, "%d", &data->iters);
    if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
    }


    ret = fscanf(infile, "%d", &total_live);
    if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
    }

    //allocating gol_board
    data->gol_board = malloc(sizeof(int) * data->rows * data->cols);

    if (data->gol_board == NULL){
        printf("Unable to initialize board\n");
        exit(1);
    }
    for (i = 0; i< (data->rows * data->cols); i++){
        //initialize as all zeroes
        data->gol_board[i] = 0;
    }
    //initialize STARTING board with cells
    init_board(data, infile);

    //Next board init with all zeroes
    data->next_board = malloc(sizeof(int) * data->rows * data->cols);
    if (data->next_board == NULL){
        printf("Unable to initialize board\n");
        exit(1);
    }
    for (i = 0; i< (data->rows * data->cols); i++){
        //initialize as all zeroes
        data->next_board[i] = 0;
    }
    
    
    return 0;
}

//Function that takes in each struct and changes
//their start and stop members based on their thread ID 

void partition (struct gol_data *data){
    
    int alloc; // the partitioned number of row/col
    int remainder; // number of row/col that needs to be reassigned
    int count;

    //partition for horizontal 
    if (data->part_mode == 0){
        alloc = (data->rows / data->threads);
        remainder = (data->rows % data->threads);
        count = remainder - data->ntids;

        if (count > 0){
            data->start = (alloc + 1) * data->ntids;
            data->end = data->start + alloc;
        }
        else if (count == 0){
            data->start = (alloc + 1) * data->ntids;
            data->end = data->start + alloc - 1;
        }
        else {
            data->start = data->ntids * alloc + remainder;
            data->end = data->start + alloc - 1;
        }
    }
    //partition for vertical
    if (data->part_mode == 1){
        alloc = (data->cols / data->threads);
        remainder = (data->cols % data->threads);
        count = remainder - data->ntids;

        if (count > 0){
            data->start = (alloc + 1) * data->ntids;
            data->end = data->start + alloc;
        }
        else if (count == 0){
            data->start = (alloc + 1) * data->ntids;
            data->end = data->start + alloc - 1;
        }
        else {
            data->start = data->ntids * alloc + remainder;
            data->end = data->start + alloc - 1;
        }
    }
    return;
}

/*
* Scans in cell data from an input file and populates the board
* data -> pointer to gol_data struct
* file -> input file
*/
void init_board(struct gol_data *data, FILE *file){
    int ret, i, j, row, col;

    for (int n = 0; n < total_live; n++){

        ret = fscanf(file, "%d%d", &row, &col);
        if (ret == 0) {
        printf("Improper file format.\n");
        exit(1);
        }

        if ((row > data->rows - 1) | (col > data->cols - 1 )){
            printf("One or more cells in the input file are out of range.\n");
            exit(1);
        }
        
        //initializing board with file data
        for (i=0; i<data->rows; i++){
            for (j=0; j<data->cols; j++){
                //condition for adding a cell
                if ((i == row) && (j == col)){
                    data->gol_board[i * data->cols + j] = 1;
                }
            }
        }

    }

}
/**************************************************************/

/* the gol application main loop function:
 *  runs rounds of GOL,
 *    * updates program state for next round (world and total_live)
 *    * performs any animation step based on the output/run mode
 *
 *   data: pointer to a struct gol_data  initialized with
 *         all GOL game playing state
 */
void* play_gol(void * arg) {
    //  at the end of each round of GOL, determine if there is an
    //  animation step to take based on the output_mode,
    //   if ascii animation:
    //     (a) call system("clear") to clear previous world state from terminal
    //     (b) call print_board function to print current world state
    //     (c) call usleep(SLEEP_USECS) to slow down the animation
    //   if ParaVis animation:
    //     (a) call your function to update the color3 buffer
    //     (b) call draw_ready(data->handle)
    //     (c) call usleep(SLEEP_USECS) to slow down the animation
    int * temp; 
    int diff;
  
    struct gol_data *data = ((struct gol_data *)arg);
    
    pthread_mutex_lock(&mutex);
    //buffer for printing row/col data
    diff = data->end - data->start + 1;


    printf("Thread ID %d \n", data->ntids);

    if (data->print_config == 1){
        if (data->part_mode == 0){

            
            printf("ntid %d:  ", data->ntids);
            printf("rows: %d:%d (%d)  ", data->start, data->end, diff);
            printf("cols: 0:%d (%d) \n", data->cols - 1, data->cols);
            
        }
        if (data->part_mode == 1){

    
            printf("ntid %d:  ", data->ntids);
            printf("rows: 0:%d (%d) ", data->rows - 1, data->rows);
            printf("cols: %d:%d (%d) \n", data->start, data->end, diff);

        }
    }

    pthread_mutex_unlock(&mutex);

    //no output
    if (data->output_mode == OUTPUT_NONE){
            
            
            for(int i=0; i<data->iters; i++){


                pthread_barrier_wait(&barrier);
                //play one round
                play_round(data);
                pthread_barrier_wait(&barrier);
                //BARRIER/


                //switch pointers (no need for copy function)
                temp = data->gol_board;
                data->gol_board = data->next_board;
                data->next_board = temp;
                

            }
        
    }

    //output with visi
    else if (data->output_mode == OUTPUT_VISI){

        for (int i = 0; i < data->iters; i++ ) {  /* run some number of iters */

  
        
            pthread_barrier_wait(&barrier);
            //play one round
            play_round(data);
            pthread_barrier_wait(&barrier);

        
    
        /* when we're all done updating our grid, update the visualization */
        update_colors(data);
        draw_ready(data->handle);

        temp = data->gol_board;
        data->gol_board = data->next_board;
        data->next_board = temp;

        usleep(100000);
    }   
    }

    //with asciimation
    else if (data->output_mode == OUTPUT_ASCII){
       
            
        for(int i=0; i<data->iters; i++){
            
            //initial board

            system("clear");
            
            if(data->ntids == 0){
                print_board(data, i);
            }
           

       
            pthread_barrier_wait(&barrier);

            play_round(data);

            pthread_barrier_wait(&barrier);
           

            usleep(100000);

            temp = data->gol_board;
            data->gol_board = data->next_board;
            data->next_board = temp;
            
        }

    }
   return 0; 
    
}

/*
Gets the neighbor counts for every cell using the helper function count_neighbors
and then sets the value of each cell accordingly. To keep every cell 
independent, it checks the neighbors of a copy of the originial board
from the beginning of the round. 
    data-> The struct containing information for the game 
    copy-> a copy of the playing board from the beginning of the round
*/
void play_round(struct gol_data *data){
    int neighbors;
    int live = 0;
    
    if(data->part_mode == 0){
        for (int i=data->start; i<=data->end; i++){
            for (int j=0; j<data->cols; j++){

                
                //for dead cell
                if (data->gol_board[i * data->cols + j] == 0){
                    neighbors = count_neighbors( data, i, j);

                    if (neighbors == 3){
                        data->next_board[i * data->cols + j] = 1;

                        live += 1; 
                    }

                    else {
                        data->next_board[i * data->cols + j] = 0;
                    }
                }
                
                
                    //living cell
                else if (data->gol_board[i * data->cols + j] == 1){

                    neighbors = count_neighbors( data, i, j);

                    if ((neighbors == 2) || (neighbors == 3)){
                        //the cell stays alive
                        data->next_board[i * data->cols + j] = 1;
                    }


                    else{
                        //the alive cell dies
                        data->next_board[i * data->cols + j] = 0;
                        live -= 1;
                    }
                    
                }
            }
        }

    }
    

    if(data->part_mode == 1 ){
        for (int i=0; i<data->rows; i++){
            for (int j=data->start; j<=data->end; j++){

                
                //for dead cell
                if (data->gol_board[i * data->cols + j] == 0){
                    neighbors = count_neighbors( data, i, j);

                    if (neighbors == 3){
                        data->next_board[i * data->cols + j] = 1;

                        live += 1; 
                    }

                    else {
                        data->next_board[i * data->cols + j] = 0;
                    }
                }
                
                
                    //living cell
                else if (data->gol_board[i * data->cols + j] == 1){

                    neighbors = count_neighbors( data, i, j);

                    if ((neighbors == 2) || (neighbors == 3)){
                        //the cell stays alive
                        data->next_board[i * data->cols + j] = 1;
                    }


                    else{
                        //the alive cell dies
                        data->next_board[i * data->cols + j] = 0;
                        live -= 1;
                    }
                    
                }
            }
        }

    }
    

    pthread_mutex_lock(&mutex);
    total_live += live;
    pthread_mutex_unlock(&mutex);
}






/*
counts how many of a cell's neighbors are alive using addition
    data-> The struct containing information for the game 
    copy-> a copy of the playing board from the beginning of the round
    i -> the row data of the cell whos neighbor is being counted
    j -> the column data of said cell
*/
int count_neighbors( struct gol_data *data, int i, int j) {
    int count = 0;
    
    // Top row neighbors


    count = data->gol_board[(((i-1) + data->rows)  % data->rows) * data->cols + (((j-1)+ data->cols) % data->cols)] +  // Top left
            data->gol_board[(((i-1)+ data->rows) % data->rows) * data->cols + ((j + data->cols) % data->cols)] +                       // Top center
            data->gol_board[(((i-1)+ data->rows) % data->rows) * data->cols + (((j+1)+ data->cols) % data->cols)] +   // Top right
            
            // Middle row neighbors
            data->gol_board[((i+ data->rows) % data->rows) * data->cols + (((j-1)+ data->cols) % data->cols)] +       // Middle left
            data->gol_board[((i+ data->rows) % data->rows) * data->cols + (((j+1)+ data->cols) % data->cols)] +       // Middle right
            
            // Bottom row neighbors
            data->gol_board[(((i+1)+ data->rows) % data->rows) * data->cols + (((j-1)+ data->cols) % data->cols)] +   // Bottom left
            data->gol_board[(((i+1)+ data->rows) % data->rows) * data->cols + ((j + data->cols) % data->cols)] +                    // Bottom center
            data->gol_board[(((i+1)+ data->rows) % data->rows) * data->cols + (((j+1)+ data->cols) % data->cols)];    // Bottom right
    


    return count;
}

/*  
Sets all of the values in the copy array equal to the playing board
    data-> The struct containing information for the game 
    copy-> a copy of the playing board from the beginning of the round
 */
void update_copy(struct gol_data *data){
    
    for (int i=0; i<data->rows; i++){
            for (int j=0; j<data->cols; j++){
                //copy over all values
                data->next_board[i * data->cols + j] = data->gol_board[i * data->cols + j];
            }
        }
        
   
};

/**************************************************************/

/* Print the board to the terminal.
 *   data: gol game specific data
 *   round: the current round number
 * NOTE: You may add extra printfs if you'd like, but please
 *       leave these fprintf calls exactly as they are to make
 *       grading easier!
 */
void print_board(struct gol_data *data, int round) {

    int i, j;

    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", round);

    for (i = 0; i < data->rows; ++i) {
        for (j = 0; j < data->cols; ++j) {
            if (data->gol_board[i * data->cols + j] == 1){
                fprintf(stderr, " @");
            }
            else {
                fprintf(stderr, " .");
            }
          
          
        }
        fprintf(stderr, "\n");
    }

    /* Print the total number of live cells. */
    fprintf(stderr, "Live cells: %d\n\n", total_live);
}



/* Describes how the pixels in the image buffer should be
 * colored based on the data in the grid.
 (Take this for the main function)
 */
void update_colors(struct gol_data *data) {

    int i, j, r, c, index, buff_i;
    int start, end;
    color3 *buff;

    buff = data->image_buff;  // just for readability
    r = data->rows;
    c = data->cols;
    start = data->start;
    end = data->end;
    if(data->part_mode == 0){
        for (i = start; i <= end; i++) {
            for (j = 0; j < c; j++) {
                index = i*c + j;
                // translate row index to y-coordinate value because in
                // the image buffer, (r,c)=(0,0) is the _lower_ left but
                // in the grid, (r,c)=(0,0) is _upper_ left.
                buff_i = (r - (i+1))*c + j;

                // update animation buffer
                if (data->gol_board[index] == 1) {
                    buff[buff_i] = c3_black;  // set live cells to black
                } else {
                    buff[buff_i] = colors[((data->ntids)%8)]; // dead to my tid color
                } 
            }
        }
    }

    if(data->part_mode == 1){
        for (i = 0; i < r; i++) {
            for (j = start; j <= end; j++) {
                index = i*c + j;
                // translate row index to y-coordinate value because in
                // the image buffer, (r,c)=(0,0) is the _lower_ left but
                // in the grid, (r,c)=(0,0) is _upper_ left.
                buff_i = (r - (i+1))*c + j;

                // update animation buffer
                if (data->gol_board[index] == 1) {
                    buff[buff_i] = c3_black;  // set live cells to black
                } else {
                    buff[buff_i] = colors[((data->ntids)%8)]; // dead to my tid color
                } 
            }
        }
    }


}


/**************************************************************/


/**************************************************************/
/***** START: DO NOT MODIFY THIS CODE *****/
/* initialize ParaVisi animation */
int setup_animation(struct gol_data* data) {
    /* connect handle to the animation */
    int num_threads = data->threads;
    data->handle = init_pthread_animation(num_threads, data->rows,
            data->cols, visi_name);
    if (data->handle == NULL) {
        printf("ERROR init_pthread_animation\n");
        exit(1);
    }
    // get the animation buffer
    data->image_buff = get_animation_buffer(data->handle);
    if(data->image_buff == NULL) {
        printf("ERROR get_animation_buffer returned NULL\n");
        exit(1);
    }
    return 0;
}

/* sequential wrapper functions around ParaVis library functions */
void (*mainloop)(struct gol_data *data);

void* seq_do_something(void * args){
    mainloop((struct gol_data *)args);
    return 0;
}

int connect_animation(void (*applfunc)(struct gol_data *data),
        struct gol_data* data)
{
    pthread_t pid;

    mainloop = applfunc;
    if( pthread_create(&pid, NULL, seq_do_something, (void *)data) ) {
        printf("pthread_created failed\n");
        return 1;
    }
    return 0;
}
/***** END: DO NOT MODIFY THIS CODE *****/
/**************************************************************/