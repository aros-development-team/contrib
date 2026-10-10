/*
**         $Filename: ilace.c $
**         $Release: 2 $
**         $Revision: 1 $
**         $Date: 2014 $
**
**         (C) Copyright 2010-2014 Yannick Erb
**         GNU General Public License
*/

/*
	ilace.c -- generate interlace sequences for rasters with n scan lines.
	rj@elilabs.com Tue Nov  5 08:43:21 CST 2002
*/


#include <stdlib.h>
#include <stdio.h>

#define FALSE (0)
#define TRUE (!(FALSE))

#define min(a, b) ((a) < (b) ? (a) : (b))


typedef struct _interval {                                       /* a semi-open interval |left, rite> */
  int left;
  int rite;
} interval;


int* slot;
int nslots;


void construct_slots(int n) {                                    /* create the array of markers */
  int i;
  nslots = n;                                                    /* we need to know its size externally */
  slot = malloc(n*sizeof(int));                                  /* allocate space for it */
  for (i = 0; i < n; i++) {                                      /* all slots are initially unmarked */
    slot[i] = FALSE;
  }
}


void destruct_slots(void) {                                      /* destroy the array of slots */
  free(slot);                                                    /* give back the space we allocated for it */
}


int dist(int p, int q) {                                         /* return the distance between  2 points */
  if (p > q) {                                                   /* swap them if out of order */
    int r = p;
    p = q;
    q = r;
  }
  return min(q - p, p + nslots - q);                             /* choose the shortest path */
}


int mark_closest_to(int p) {                                     /* mark slot closest to p, return numer of slot marked */
  int i;
  int q;

  for (i = 0; i < nslots/2 + 1; i++) {                           /* search for closest unmarked slot */

    q = (p + i) % nslots;                                        /* look forwards */
    if (!slot[q]) {
      slot[q] = TRUE;
      return q;
    } 

    q = (p - i + nslots) % nslots;                               /* look backwards */
    if (!slot[q]) {
      slot[q] = TRUE;
      return q;
    }
  }

  printf("%s:%d All slots marked!\n", __FILE__, __LINE__);
  exit(1);
  return 0; /* NOT REACHED */
}


int midpoint(interval* I) {
  return (I->left + I->rite)/2;
}


static interval* largest_interval_array;


void construct_largest_interval(int n) {
  int i;
  largest_interval_array = (interval*)malloc(2*n*sizeof(interval));
  for (i = 0; i < n; i++) {
    largest_interval_array[i].left = 0;
    largest_interval_array[i].rite = 0;
  }
}


void destruct_largest_interval(void) {
  free(largest_interval_array);
}


void remove_largest_interval(interval* I) {
  int i;
  
  I->left = largest_interval_array[0].left;
  I->rite = largest_interval_array[0].rite;

  for (i = 1; i < nslots; i++) {
    largest_interval_array[i - 1].left = largest_interval_array[i].left;
    largest_interval_array[i - 1].rite = largest_interval_array[i].rite;
  }
}


void insert_interval(interval* I) {
  int i;
  int j;
  int size0 = I->rite - I->left;
  int size;

  if (size0 <= 1) {                                              /* don't do degenerate intervals */
    return;
  }

  for (j = 0; j < nslots; j++) {                                 /* search for place to insert */
    size = largest_interval_array[j].rite - largest_interval_array[j].left;
    if (size0 >= size) {                                         /* found right place? */
      for (i = nslots - 1; i >= j; i--) {                        /* yes, slide over to make some room */
        largest_interval_array[i + 1].left = largest_interval_array[i].left;
        largest_interval_array[i + 1].rite = largest_interval_array[i].rite;    
      }
      largest_interval_array[j].left = I->left;                  /* insert this interval */
      largest_interval_array[j].rite = I->rite;

      return;
    }
  }
  printf("%s:%d Fell off end of sorted list!\n", __FILE__, __LINE__);
  exit(1);
}


void construct_ilace(int n) {
  construct_slots(n);
  construct_largest_interval(n);
}

void destruct_ilace(void) {
  destruct_largest_interval();
  destruct_slots();
}


int* ilace(int n) {                                              /* generate a "good" interlace sequence for n scan lines */
  int line = 0;
  interval I;
  interval J;
  int i;
  int* seq = malloc(n*sizeof(int));                              /* make room for the resule */

  construct_ilace(n);                                            /* initialize */

  J.left = 0;
  J.rite = n - 1;
  insert_interval(&J);

  for(i = 0; i < n; i++) {                                       /* for each scan line */

    remove_largest_interval(&I);                                 /* get largest remaining interval */
    line = (I.left + I.rite)/2;                                  /* midpoint of interval */
    line = mark_closest_to(line);                                /* mark the closest available scan line */
    seq[i] = line;                                               /* put its number in the sequence */

    J.left = I.left;                                             /* left half */
    J.rite = line;
    insert_interval(&J);

    J.left = line + 1;                                           /* rite half */
    J.rite = I.rite;
    insert_interval(&J);
  }    
  destruct_ilace();                                              /* get rid of temporary objects */

  return seq;                                                    /* return the pointer to the interlace sequence array */
}

