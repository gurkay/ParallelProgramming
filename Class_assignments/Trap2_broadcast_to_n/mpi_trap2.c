/* File:     mpi_trap2.c
 * Purpose:  Use MPI to implement a parallel version of the trapezoidal
 *           rule.  This version accepts input of the endpoints of the
 *           interval and the number of trapezoids.
 *
 * Input:    The endpoints of the interval of integration and the number
 *           of trapezoids
 * Output:   Estimate of the integral from a to b of f(x)
 *           using the trapezoidal rule and n trapezoids.
 *
 * Compile:  mpicc -g -Wall -o mpi_trap2 mpi_trap2.c
 * Run:      mpiexec -n <number of processes> ./mpi_trap2
 *
 * Algorithm:
 *    1.  Each process calculates "its" interval of
 *        integration.
 *    2.  Each process estimates the integral of f(x)
 *        over its interval using the trapezoidal rule.
 *    3a. Each process != 0 sends its integral to 0.
 *    3b. Process 0 sums the calculations received from
 *        the individual processes and prints the result.
 *
 * Note:  f(x) is all hardwired.
 *
 * IPP:   Section 3.3.2  (pp. 100 and ff.)
 */
#include <stdio.h>
#include <math.h>

/* We'll be using MPI routines, definitions, etc. */
#include <mpi.h>

/* Get the input values */
void Get_input(int my_rank, int comm_sz, double* a_p, double* b_p,
      int* n_p);

/* Calculate local integral  */
double Trap(double left_endpt, double right_endpt, int trap_count,
   double base_len);

/* Function we're integrating */
double f(double x);

int main(void) {
   int my_rank, comm_sz, n, local_n;
   double a, b, h, local_a, local_b;
   double local_int, total_int;
   int source;

   /* Let the system do what it needs to start up MPI */
   MPI_Init(NULL, NULL);

   /* Get my process rank */
   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

   /* Find out how many processes are being used */
   MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);

   Get_input(my_rank, comm_sz, &a, &b, &n);

   h = (b-a)/n;          /* h is the same for all processes */
   local_n = n/comm_sz;  /* So is the number of trapezoids  */

   /* Length of each process' interval of
    * integration = local_n*h.  So my interval
    * starts at: */
   local_a = a + my_rank*local_n*h;
   local_b = local_a + local_n*h;
   local_int = Trap(local_a, local_b, local_n, h);

   /* Add up the integrals calculated by each process */
   if (my_rank != 0)
      MPI_Send(&local_int, 1, MPI_DOUBLE, 0, 0,
            MPI_COMM_WORLD);
   else {
      total_int = local_int;
      for (source = 1; source < comm_sz; source++) {
         MPI_Recv(&local_int, 1, MPI_DOUBLE, source, 0,
            MPI_COMM_WORLD, MPI_STATUS_IGNORE);
         total_int += local_int;
      }
   }

   /* Print the result */
   if (my_rank == 0) {
      printf("With n = %d trapezoids, our estimate\n", n);
      printf("of the integral from %f to %f = %.15e\n",
          a, b, total_int);
   }

   /* Shut down MPI */
   MPI_Finalize();

   return 0;
} /*  main  */

/*------------------------------------------------------------------
 * Function:     Get_input
 * Purpose:      Get the user input:  the left and right endpoints
 *               and the number of trapezoids
 * Input args:   my_rank:  process rank in MPI_COMM_WORLD
 *               comm_sz:  number of processes in MPI_COMM_WORLD
 * Output args:  a_p:  pointer to left endpoint
 *               b_p:  pointer to right endpoint
 *               n_p:  pointer to number of trapezoids
 */
void Get_input(int my_rank, int comm_sz, double* a_p, double* b_p,
      int* n_p) {
    int count, divisor, core_difference;
    double baseChangeResult;
                                          // we know that the number of iterations is the log(#processors) in base 2. Since we don't have base 2 natively, we must change basis.
    baseChangeResult=log(comm_sz)/log(2); // Here we are changing the logarithm base to the base 2. See image (https://wikimedia.org/api/rest_v1/media/math/render/svg/97a21bb377232d1d0fb8927a6e78501008bf794b)
    count = ceil(baseChangeResult); //since baseChangeResult is an double, we want o find the colsest integer to it. The reason for this is quite simple
                                  // in case we have an odd # of processors, we will have the number of iterations count equal to the closest integer which is greater than baseChangeResult
    divisor=pow(2,count);
    core_difference=divisor/2;

    //read input from an external file
    if (my_rank == 0){
      printf("I am %d!!\n", my_rank);
      //core 0 must load the values
      FILE *fp;
      fp = fopen("mpi_trap2-inputs.txt", "r");
      fscanf(fp, "%lf %lf %d", a_p, b_p, n_p);
      printf("The input values were: a=%lf b=%lf n=%d \n", *a_p, *b_p, *n_p);
      fclose(fp);
    }

    //broadcasting to n processors
   for (int i=0; i<count; i++){                                     //Be aware the necessity of the second condition. It will make the las odd processor to receive only once. Which is tue amount necessary.
     if (my_rank%divisor==0 && (my_rank+core_difference < comm_sz)) //Note that is not necessary to check my_rank==0, this will aways be true.
     {
       MPI_Send(a_p, 1, MPI_DOUBLE, my_rank+core_difference, 0, MPI_COMM_WORLD);
       MPI_Send(b_p, 1, MPI_DOUBLE, my_rank+core_difference, 0, MPI_COMM_WORLD);
       MPI_Send(n_p, 1, MPI_INT, my_rank+core_difference, 0, MPI_COMM_WORLD);

     } else if (my_rank%core_difference==0){

       MPI_Recv(a_p, 1, MPI_DOUBLE, my_rank-core_difference, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
       MPI_Recv(b_p, 1, MPI_DOUBLE, my_rank-core_difference, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
       MPI_Recv(n_p, 1, MPI_INT, my_rank-core_difference, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

     }
     divisor /= 2;
     core_difference /= 2;
   }

}  /* Get_input */

/*------------------------------------------------------------------
 * Function:     Trap
 * Purpose:      Serial function for estimating a definite integral
 *               using the trapezoidal rule
 * Input args:   left_endpt
 *               right_endpt
 *               trap_count
 *               base_len
 * Return val:   Trapezoidal rule estimate of integral from
 *               left_endpt to right_endpt using trap_count
 *               trapezoids
 */
double Trap(
      double left_endpt  /* in */,
      double right_endpt /* in */,
      int    trap_count  /* in */,
      double base_len    /* in */) {
   double estimate, x;
   int i;

   estimate = (f(left_endpt) + f(right_endpt))/2.0;
   for (i = 1; i <= trap_count-1; i++) {
      x = left_endpt + i*base_len;
      estimate += f(x);
   }
   estimate = estimate*base_len;

   return estimate;
} /*  Trap  */


/*------------------------------------------------------------------
 * Function:    f
 * Purpose:     Compute value of function to be integrated
 * Input args:  x
 */
double f(double x) {
   return x*x;
} /* f */