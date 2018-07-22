#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <crtdefs.h>
/*
#define HAVE_STRINGS_H
#define HAVE_LOCALE_H
#define ENABLE_NLS
#define HAVE_STRCASECMP
 


#define  HAVE_CONFIG_H
#define  libyasm_EXPORTS
#define  YASM_LIB_SOURCE
  
*/





/** Hash array mapped trie data structure (opaque type). */
typedef struct HAMT HAMT;
/** Hash array mapped trie entry (opaque type). */
typedef struct HAMTEntry HAMTEntry;



/***********************************/

/*
 * Singly-linked Tail queue declarations.
 */
#define STAILQ_HEAD(name, type)                                         \
struct name {                                                           \
        struct type *stqh_first;/* first element */                     \
        struct type **stqh_last;/* addr of last next element */         \
}

#define STAILQ_HEAD_INITIALIZER(head)                                   \
        { NULL, &(head).stqh_first }

#define STAILQ_ENTRY(type)                                              \
struct {                                                                \
        struct type *stqe_next; /* next element */                      \
}

/*
 * Singly-linked Tail queue functions.
 */
#define STAILQ_CONCAT(head1, head2) do {                                \
        if (!STAILQ_EMPTY((head2))) {                                   \
                *(head1)->stqh_last = (head2)->stqh_first;              \
                (head1)->stqh_last = (head2)->stqh_last;                \
                STAILQ_INIT((head2));                                   \
        }                                                               \
} while (0)

#define STAILQ_EMPTY(head)      ((head)->stqh_first == NULL)

#define STAILQ_FIRST(head)      ((head)->stqh_first)

#define STAILQ_FOREACH(var, head, field)                                \
        for((var) = STAILQ_FIRST((head));                               \
           (var);                                                       \
           (var) = STAILQ_NEXT((var), field))


#define STAILQ_FOREACH_SAFE(var, head, field, tvar)                     \
        for ((var) = STAILQ_FIRST((head));                              \
            (var) && ((tvar) = STAILQ_NEXT((var), field), 1);           \
            (var) = (tvar))

#define STAILQ_INIT(head) do {                                          \
        STAILQ_FIRST((head)) = NULL;                                    \
        (head)->stqh_last = &STAILQ_FIRST((head));                      \
} while (0)

#define STAILQ_INSERT_AFTER(head, tqelm, elm, field) do {               \
        if ((STAILQ_NEXT((elm), field) = STAILQ_NEXT((tqelm), field)) == NULL)\
                (head)->stqh_last = &STAILQ_NEXT((elm), field);         \
        STAILQ_NEXT((tqelm), field) = (elm);                            \
} while (0)

#define STAILQ_INSERT_HEAD(head, elm, field) do {                       \
        if ((STAILQ_NEXT((elm), field) = STAILQ_FIRST((head))) == NULL) \
                (head)->stqh_last = &STAILQ_NEXT((elm), field);         \
        STAILQ_FIRST((head)) = (elm);                                   \
} while (0)

#define STAILQ_INSERT_TAIL(head, elm, field) do {                       \
        STAILQ_NEXT((elm), field) = NULL;                               \
        *(head)->stqh_last = (elm);                                     \
        (head)->stqh_last = &STAILQ_NEXT((elm), field);                 \
} while (0)

#define STAILQ_LAST(head, type, field)                                  \
        (STAILQ_EMPTY((head)) ?                                         \
                NULL :                                                  \
                ((struct type *)                                        \
                ((char *)((head)->stqh_last) - offsetof(struct type, field))))

#define STAILQ_NEXT(elm, field) ((elm)->field.stqe_next)

#define STAILQ_REMOVE(head, elm, type, field) do {                      \
        if (STAILQ_FIRST((head)) == (elm)) {                            \
                STAILQ_REMOVE_HEAD((head), field);                      \
        }                                                               \
        else {                                                          \
                struct type *curelm = STAILQ_FIRST((head));             \
                while (STAILQ_NEXT(curelm, field) != (elm))             \
                        curelm = STAILQ_NEXT(curelm, field);            \
                if ((STAILQ_NEXT(curelm, field) =                       \
                     STAILQ_NEXT(STAILQ_NEXT(curelm, field), field)) == NULL)\
                        (head)->stqh_last = &STAILQ_NEXT((curelm), field);\
        }                                                               \
} while (0)

#define STAILQ_REMOVE_HEAD(head, field) do {                            \
        if ((STAILQ_FIRST((head)) =                                     \
             STAILQ_NEXT(STAILQ_FIRST((head)), field)) == NULL)         \
                (head)->stqh_last = &STAILQ_FIRST((head));              \
} while (0)

#define STAILQ_REMOVE_HEAD_UNTIL(head, elm, field) do {                 \
        if ((STAILQ_FIRST((head)) = STAILQ_NEXT((elm), field)) == NULL) \
                (head)->stqh_last = &STAILQ_FIRST((head));              \
} while (0)
/****************************************************************************/

/* Bit-counting: used primarily by HAMT but also in a few other places. */
#define BC_TWO(c)       (0x1ul << (c))
#define BC_MSK(c)       (((unsigned long)(-1)) / (BC_TWO(BC_TWO(c)) + 1ul))
#define BC_COUNT(x,c)   ((x) & BC_MSK(c)) + (((x) >> (BC_TWO(c))) & BC_MSK(c))
#define BitCount(d, s)          do {            \
        d = BC_COUNT(s, 0);                     \
        d = BC_COUNT(d, 1);                     \
        d = BC_COUNT(d, 2);                     \
        d = BC_COUNT(d, 3);                     \
        d = BC_COUNT(d, 4);                     \
    } while (0)



/** Create new, empty, HAMT.  error_func() is called when an internal error is
 * encountered--it should NOT return to the calling function.
 * \param   nocase          nonzero if HAMT should be case-insensitive
 * \param   error_func      function called on internal error
 * \return New, empty, hash array mapped trie.
 */

HAMT *HAMT_create(int nocase, /*@exits@*/ void (*error_func)
    (const char *file, unsigned int line, const char *message));

/** Delete HAMT and all data associated with it.  Uses deletefunc() to delete
 * each data item.
 * \param hamt          Hash array mapped trie
 * \param deletefunc    Data deletion function
 */

void HAMT_destroy(/*@only@*/ HAMT *hamt,
                  void (*deletefunc) (/*@only@*/ void *data));

/** Insert key into HAMT, associating it with data. 
 * If the key is not present in the HAMT, inserts it, sets *replace to 1, and
 *  returns the data passed in.
 * If the key is already present and *replace is 0, deletes the data passed
 *  in using deletefunc() and returns the data currently associated with the
 *  key.
 * If the key is already present and *replace is 1, deletes the data currently
 *  associated with the key using deletefunc() and replaces it with the data
 *  passed in.
 * \param hamt          Hash array mapped trie
 * \param str           Key
 * \param data          Data to associate with key
 * \param replace       See above description
 * \param deletefunc    Data deletion function if data is replaced
 * \return Data now associated with key.
 */

/*@dependent@*/ void *HAMT_insert(HAMT *hamt, /*@dependent@*/ const char *str,
                                  /*@only@*/ void *data, int *replace,
                                  void (*deletefunc) (/*@only@*/ void *data));

/** Search for the data associated with a key in the HAMT.
 * \param hamt          Hash array mapped trie
 * \param str           Key
 * \return NULL if key/data not present in HAMT, otherwise associated data.
 */

/*@dependent@*/ /*@null@*/ void *HAMT_search(HAMT *hamt, const char *str);

/** Traverse over all keys in HAMT, calling function on each data item. 
 * \param hamt          Hash array mapped trie
 * \param d             Data to pass to each call to func.
 * \param func          Function to call
 * \return Stops early (and returns func's return value) if func returns a
 *         nonzero value; otherwise 0.
 */

int HAMT_traverse(HAMT *hamt, /*@null@*/ void *d,
                  int (*func) (/*@dependent@*/ /*@null@*/ void *node,
                               /*@null@*/ void *d));

/** Get the first entry in a HAMT.
 * \param hamt          Hash array mapped trie
 * \return First entry in HAMT, or NULL if HAMT is empty.
 */

const HAMTEntry *HAMT_first(const HAMT *hamt);

/** Get the next entry in a HAMT.
 * \param prev          Previous entry in HAMT
 * \return Next entry in HAMT, or NULL if no more entries.
 */

/*@null@*/ const HAMTEntry *HAMT_next(const HAMTEntry *prev);

/** Get the corresponding data for a HAMT entry.
 * \param entry         HAMT entry (as returned by HAMT_first() and HAMT_next())
 * \return Corresponding data item.
 */

void *HAMTEntry_get_data(const HAMTEntry *entry);




void *def_xmalloc(size_t size);
//static void *def_xcalloc(size_t nelem, size_t elsize);
//static void *def_xrealloc    (void *oldmem, size_t size);
void def_xfree(/*@only@*/ /*@out@*/ /*@null@*/ void *p);
void yasm__fatal(const char *message, ...);
void def_internal_error_(const char *file, unsigned int line, const char *message);
# define N_(String)     (String)




struct HAMTEntry {
    STAILQ_ENTRY(HAMTEntry) next;       /* next hash table entry */
    /*@dependent@*/ const char *str;    /* string being hashed */
    /*@owned@*/ void *data;             /* data pointer being stored */
};

typedef struct HAMTNode {
    unsigned long BitMapKey;            /* 32 bits, bitmap or hash key */
    uintptr_t BaseValue;                /* Base of HAMTNode list or value */
} HAMTNode;

struct HAMT {
    STAILQ_HEAD(HAMTEntryHead, HAMTEntry) entries;
    HAMTNode *root;
    /*@exits@*/ void (*error_func) (const char *file, unsigned int line,
                                    const char *message);
    unsigned long (*HashKey) (const char *key);
    unsigned long (*ReHashKey) (const char *key, int Level);
    int (*CmpKey) (const char *s1, const char *s2);
};
























