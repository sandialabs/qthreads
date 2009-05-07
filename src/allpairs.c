#include <unistd.h>		       /* for getpagesize() */
#include <qthread/qthread.h>
#include <qthread/allpairs.h>

size_t pagesize = 0;


/* The setup of this function is: we have a page-aligned block of data in
 * [page], it is [pagesize] bytes. It is an array of elements each [unitsize]
 * big. We must run [distfunc] on each pair of elements within the page, and
 * store each [outsize]-byte result to [output]. */
static void qt_allpairs_onepage(const void *page, const size_t pagesize,
				const size_t unitsize, void **output,
				const size_t outsize, const size_t outoffset,
				const dist_f distfunc)
{
    const size_t count = pagesize / unitsize;
    size_t i, j;

    for (i = 0; i < count; i++) {
	for (j = i; j < count; j++) {
	    distfunc(page + (i * unitsize), page + (j * unitsize),
		     output[i] + ((j + outoffset) * outsize));
	}
    }
}

/* This function is much like the previous function, the only difference being
 * that we're pulling our data from two separate page blocks, rather than just
 * the one. Thus, not only are we pulling from separate places in the machine,
 * but we're also doing twice the work (we can't eliminate half the task as
 * redundant). */
static void qt_allpairs_twopage(const void *page1, const void *page2,
				const size_t pagesize, const size_t unitsize,
				void **output, const size_t outsize,
				const size_t outoffset, const dist_f distfunc)
{
    const size_t count = pagesize / unitsize;
    size_t i, j;

    for (i = 0; i < count; i++) {
	for (j = 0; j < count; j++) {
	    distfunc(page1 + (i * unitsize), page2 + (j * unitsize),
		     output[i] + ((j + outoffset) * outsize));
	}
    }
}

/* This function is much like the previous function, except now the two page
 * blocks can be DIFFERENT SIZES (i.e. one may be a partial page) */
static void qt_allpairs_twopage_partial(const void *page1, const void *page2,
					const size_t pagesize1, const size_t pagesize2,
					const size_t unitsize, void **output,
					const size_t outsize, const size_t outoffset,
					const dist_f distfunc)
{
    const size_t count1 = pagesize1 / unitsize;
    const size_t count2 = pagesize2 / unitsize;
    size_t i, j;

    for (i = 0; i < count1; i++) {
	for (j = 0; j < count2; j++) {
	    distfunc(page1 + (i * unitsize), page2 + (j * unitsize),
		     output[i] + ((j + outoffset) * outsize));
	}
    }
}

/* The setup is this: we have an [array] of [count] elements that are each
 * [unitsize] bytes long. We want to call [distfunc] on each pair of elements.
 * This function will produce a result that is [outsize] bytes. Calling the
 * function on each pair will produce an array of results, [output].
 *
 * To make this faster...
 * 0. We assume that [array] is page aligned, and that [unitsize] divides
 *    evenly into page size chunks
 * 1. We break [array] into pagesize chunks
 * 2. We observe that each output region is calculated either by manipulating a
 *    single page or by manipulating two pages
 */
void qt_allpairs(const void *array, const size_t count, const size_t unitsize,
		 void **output, const size_t outsize, const dist_f distfunc)
{
    if (pagesize == 0) {
	pagesize = getpagesize();
    }
    {
	const size_t pagecount = pagesize / unitsize;	// units in a page
	const size_t numpages = count / pagecount;	// whole pages in the array
	const size_t trailing_elem = numpages * pagecount;	// first elem of last block
	const size_t trailing_page = numpages * pagesize;	// byte offset of last block
	const size_t trailing_edge = (count - trailing_elem) * unitsize;	// size of last non-page block
	size_t page1, page2;

	/* a page-to-page loop */
	for (page1 = 0; page1 < numpages; page1++) {
	    for (page2 = page1; page2 < numpages; page2++) {
		if (page2 == page1) {
		    qt_allpairs_onepage(array + (page1 * pagesize), pagesize,
					unitsize,
					output + (page1 * pagecount), outsize,
					(page1 * pagecount), distfunc);
		} else {
		    qt_allpairs_twopage(array + (page1 * pagesize),
					array + (page2 * pagesize), pagesize,
					unitsize,
					output + (page1 * pagecount), outsize,
					page2 * pagecount, distfunc);
		}
	    }
	    if (trailing_edge) {       /* pair page1 with trailing page */
		qt_allpairs_twopage_partial(array + (page1 * pagesize),
					    array + trailing_page, pagesize,
					    trailing_edge, unitsize,
					    output + (page1 * pagecount),
					    outsize, trailing_elem, distfunc);
	    }
	}
	if (trailing_edge) {	       /* pair trailing page with itself */
	    qt_allpairs_onepage(array + trailing_page, trailing_edge,
				unitsize, output + trailing_elem, outsize,
				trailing_elem, distfunc);
	}
    }
}
