// written by russ cox rsc@swtch.com 2008/12/07.
// gcc 4.3.2 collapses calls to sin and cos with same arg
// to a single call to sincos when compiling -O2

extern double sin(double x);
extern double cos(double x);
void
sincos(double x, double *s, double *c)
{
	*s = sin(x);
	*c = cos(x);
}
