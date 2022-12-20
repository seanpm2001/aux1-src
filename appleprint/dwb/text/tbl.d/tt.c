#ifndef lint	/* .../appleprint/dwb/text/tbl.d/tt.c */
#define _AC_NAME tt_c
#define _AC_NO_MAIN "@(#) Copyright (c) 1984-85 AT&T-IS, All Rights Reserved.  {Apple version 1.2 87/11/11 22:15:02}"
#include <apple_notice.h>

#ifdef _AC_HISTORY
  static char *sccsid = "@(#)Copyright Apple Computer 1987	Version 1.2 of tt.c on 87/11/11 22:15:02";
#endif		/* _AC_HISTORY */
#endif		/* lint */

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#define _AC_MODS
 /* tt.c: subroutines for drawing horizontal lines */


# include "t..c"
ctype(il, ic)
{
if (instead[il])
	return(0);
if (fullbot[il])
	return(0);
il = stynum[il];
return(style[ic][il]);
}
min(a,b)
{
return(a<b ? a : b);
}
fspan(i,c)
{
c++;
return(c<ncol && ctype(i,c)=='s');
}
lspan(i,c)
{
int k;
if (ctype(i,c) != 's') return(0);
c++;
if (c < ncol && ctype(i,c)== 's') 
	return(0);
for(k=0; ctype(i,--c) == 's'; k++);
return(k);
}
ctspan(i,c)
{
int k;
c++;
for(k=1; c<ncol && ctype(i,c)=='s'; k++)
	c++;
return(k);
}
tohcol(ic)
{
			if (ic==0)
				fprintf(tabout, "\\h'|0'");
			else
				fprintf(tabout, "\\h'(|\\n(%2su+|\\n(%2su)/2u'", reg(ic,CLEFT), reg(ic-1,CRIGHT));
}
allh(i)
{
/* return true if every element in line i is horizontal */
/* also at least one must be horizontl */
int c, one, k;
if (fullbot[i]) return(1);
if (i>=nlin) return(dboxflg||boxflg);
for(one=c=0; c<ncol; c++)
	{
	k = thish(i,c);
	if (k==0) return(0);
	if (k==1) continue;
	one=1;
	}
return(one);
}
thish(i,c)
{
	int t;
	char *s;
	struct colstr *pc;
	if (c<0)return(0);
	if (i<0) return(0);
	t = ctype(i,c);
	if (t=='_' || t == '-')
		return('-');
	if (t=='=')return('=');
	if (t=='^') return(1);
	if (fullbot[i] )
		return(fullbot[i]);
	if (t=='s') return(thish(i,c-1));
	if (t==0) return(1);
	pc = &table[i][c];
	s = (t=='a' ? pc->rcol : pc->col);
	if (s==0 || (point(s) && *s==0))
		return(1);
	if (vspen(s)) return(1);
	if (t=barent( s))
		return(t);
	return(0);
}
