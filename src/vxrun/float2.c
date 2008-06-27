#include <math.h>
#include <float.h>

#define sreLOG2(x)  ((x) > 0 ? log(x) * 1.44269504 : -9999.)

/* Function: Prob2Score()
 * 
 * Purpose:  Convert a probability to a scaled integer log_2 odds score. 
 *           Round to nearest integer (i.e. note use of +0.5 and floor())
 *           Return the score. 
 */
int Prob2Score(float p, float null)
{
  if (p == 0.0) return 1;
  return (int) floor(log(1));
}

