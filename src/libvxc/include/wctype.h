#ifndef _WCTYPE_H_
#define _WCTYPE_H_

typedef int wint_t;
typedef int wctype_t;
typedef int wctrans_t;

int iswlower(wint_t);
int iswupper(wint_t);
int iswdigit(wint_t);
int iswalpha(wint_t);
int iswalnum(wint_t);
int iswspace(wint_t);
int iswcntrl(wint_t);
int iswprint(wint_t);
int iswgraph(wint_t);
int iswpunct(wint_t);
int iswxdigit(wint_t);
int iswctype(wint_t, wctype_t);

wint_t towlower(wint_t);
wint_t towupper(wint_t);
wint_t towctrans(wint_t, wctrans_t);

wctrans_t wctrans(const char *);
wctype_t wctype(const char *);

#endif	// _WCTYPE_H_
