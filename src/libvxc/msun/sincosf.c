// written by russ cox rsc@swtch.com 2008/12/07.
// gcc 4.3.2 collapses calls to sin and cos with same arg
// to a single call to sincos when compiling -O2

extern float sinf(float x);
extern float cosf(float x);
void
sincosf(float x, float *s, float *c)
{
	*s = sinf(x);
	*c = cosf(x);
}
