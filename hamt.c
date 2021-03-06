#include "hamt.h"





/* XXX make a portable version of this.  This depends on the pointer being
 * 4 or 2-byte aligned (as it uses the LSB of the pointer variable to store
 * the subtrie flag!
 */
#define IsSubTrie(n)            ((n)->BaseValue & 1)
#define SetSubTrie(h, n, v)     do {                            \
        if ((uintptr_t)(v) & 1)                                 \
            h->error_func(__FILE__, __LINE__,                   \
                          N_("Subtrie is seen as subtrie before flag is set (misaligned?)"));   \
        (n)->BaseValue = (uintptr_t)(v) | 1;    \
    } while (0)
#define SetValue(h, n, v)       do {                            \
        if ((uintptr_t)(v) & 1)                                 \
            h->error_func(__FILE__, __LINE__,                   \
                          N_("Value is seen as subtrie (misaligned?)")); \
        (n)->BaseValue = (uintptr_t)(v);        \
    } while (0)
#define GetSubTrie(n)           (HAMTNode *)(((n)->BaseValue | 1) ^ 1)

static unsigned long
HashKey(const char *key)
{
    unsigned long a=31415, b=27183, vHash;
    for (vHash=0; *key; key++, a*=b)
        vHash = a*vHash + *key;
    return vHash;
}

static unsigned long 
ReHashKey(const char *key, int Level)
{
    unsigned long a=31415, b=27183, vHash;
    for (vHash=0; *key; key++, a*=b)
        vHash = a*vHash*(unsigned long)Level + *key;
    return vHash;
}

static unsigned long
HashKey_nocase(const char *key)
{
    unsigned long a=31415, b=27183, vHash;
    for (vHash=0; *key; key++, a*=b)
        vHash = a*vHash + tolower(*key);
    return vHash;
}

static unsigned long
ReHashKey_nocase(const char *key, int Level)
{
    unsigned long a=31415, b=27183, vHash;
    for (vHash=0; *key; key++, a*=b)
        vHash = a*vHash*(unsigned long)Level + tolower(*key);
    return vHash;
}

HAMT *
HAMT_create(int nocase, /*@exits@*/ void (*error_func)
    (const char *file, unsigned int line, const char *message))
{
    /*@out@*/ HAMT *hamt = def_xmalloc(sizeof(HAMT));
    int i;

    STAILQ_INIT(&hamt->entries);
    hamt->root = def_xmalloc(32*sizeof(HAMTNode));

    for (i=0; i<32; i++) {
        hamt->root[i].BitMapKey = 0;
        hamt->root[i].BaseValue = 0;
    }

    


    
    hamt->error_func = error_func;
    if (nocase) {
        hamt->HashKey = HashKey_nocase;
        hamt->ReHashKey = ReHashKey_nocase;
        //hamt->CmpKey = &strcasecmp;
    } else {
        hamt->HashKey = HashKey;
        hamt->ReHashKey = ReHashKey;
        hamt->CmpKey = strcmp;
    }

    return hamt;
}

static void
HAMT_delete_trie(HAMTNode *node)
{
    if (IsSubTrie(node)) {
        unsigned long i, Size;

        /* Count total number of bits in bitmap to determine size */
        BitCount(Size, node->BitMapKey);
        Size &= 0x1F;
        if (Size == 0)
            Size = 32;

        for (i=0; i<Size; i++)
            HAMT_delete_trie(&(GetSubTrie(node))[i]);
        def_xfree(GetSubTrie(node));
    }
}

void
HAMT_destroy(HAMT *hamt, void (*deletefunc) (/*@only@*/ void *data))
{
    int i;

    /* delete entries */
    while (!STAILQ_EMPTY(&hamt->entries)) {
        HAMTEntry *entry;
        entry = STAILQ_FIRST(&hamt->entries);
        STAILQ_REMOVE_HEAD(&hamt->entries, next);
        deletefunc(entry->data);
        def_xfree(entry);
    }

    /* delete trie */
    for (i=0; i<32; i++)
        HAMT_delete_trie(&hamt->root[i]);

    def_xfree(hamt->root);
    def_xfree(hamt);
}

int
HAMT_traverse(HAMT *hamt, void *d,
              int (*func) (/*@dependent@*/ /*@null@*/ void *node,
                            /*@null@*/ void *d))
{
    HAMTEntry *entry;
    STAILQ_FOREACH(entry, &hamt->entries, next) {
        int retval = func(entry->data, d);
        if (retval != 0)
            return retval;
    }
    return 0;
}

const HAMTEntry *
HAMT_first(const HAMT *hamt)
{
    return STAILQ_FIRST(&hamt->entries);
}

const HAMTEntry *
HAMT_next(const HAMTEntry *prev)
{
    return STAILQ_NEXT(prev, next);
}

void *
HAMTEntry_get_data(const HAMTEntry *entry)
{
    return entry->data;
}

/*@-temptrans -kepttrans -mustfree@*/
void *
HAMT_insert(HAMT *hamt, const char *str, void *data, int *replace,
            void (*deletefunc) ( void *data))
{
    HAMTNode *node, *newnodes;
    HAMTEntry *entry;
    unsigned long key, keypart, Map;
    int keypartbits = 0;
    int level = 0;

    key = hamt->HashKey(str);
    keypart = key & 0x1F;
    node = &hamt->root[keypart];

    if (!node->BaseValue) {
        node->BitMapKey = key;
        entry = def_xmalloc(sizeof(HAMTEntry));
        entry->str = str;
        entry->data = data;
        STAILQ_INSERT_TAIL(&hamt->entries, entry, next);
        SetValue(hamt, node, entry);
        if (IsSubTrie(node))
            hamt->error_func(__FILE__, __LINE__,
                             N_("Data is seen as subtrie (misaligned?)"));
        *replace = 1;
        return data;
    }

    for (;;) {
        if (!(IsSubTrie(node))) {
            if (node->BitMapKey == key
                && hamt->CmpKey(((HAMTEntry *)(node->BaseValue))->str,
                                str) == 0) {
                /*@-branchstate@*/
                if (*replace) {
                    deletefunc(((HAMTEntry *)(node->BaseValue))->data);
                    ((HAMTEntry *)(node->BaseValue))->str = str;
                    ((HAMTEntry *)(node->BaseValue))->data = data;
                } else
                    deletefunc(data);
                /*@=branchstate@*/
                return ((HAMTEntry *)(node->BaseValue))->data;
            } else {
                unsigned long key2 = node->BitMapKey;
                /* build tree downward until keys differ */
                for (;;) {
                    unsigned long keypart2;

                    /* replace node with subtrie */
                    keypartbits += 5;
                    if (keypartbits > 30) {
                        /* Exceeded 32 bits: rehash */
                        key = hamt->ReHashKey(str, level);
                        key2 = hamt->ReHashKey(
                            ((HAMTEntry *)(node->BaseValue))->str, level);
                        keypartbits = 0;
                    }
                    keypart = (key >> keypartbits) & 0x1F;
                    keypart2 = (key2 >> keypartbits) & 0x1F;

                    if (keypart == keypart2) {
                        /* Still equal, build one-node subtrie and continue
                         * downward.
                         */
                        newnodes = def_xmalloc(sizeof(HAMTNode));
                        newnodes[0].BitMapKey = key2;
                        newnodes[0].BaseValue = node->BaseValue;
                        node->BitMapKey = 1<<keypart;
                        SetSubTrie(hamt, node, newnodes);
                        node = &newnodes[0];
                        level++;
                    } else {
                        /* partitioned: allocate two-node subtrie */
                        newnodes = def_xmalloc(2*sizeof(HAMTNode));

                        entry = def_xmalloc(sizeof(HAMTEntry));
                        entry->str = str;
                        entry->data = data;
                        STAILQ_INSERT_TAIL(&hamt->entries, entry, next);

                        /* Copy nodes into subtrie based on order */
                        if (keypart2 < keypart) {
                            newnodes[0].BitMapKey = key2;
                            newnodes[0].BaseValue = node->BaseValue;
                            newnodes[1].BitMapKey = key;
                            SetValue(hamt, &newnodes[1], entry);
                        } else {
                            newnodes[0].BitMapKey = key;
                            SetValue(hamt, &newnodes[0], entry);
                            newnodes[1].BitMapKey = key2;
                            newnodes[1].BaseValue = node->BaseValue;
                        }

                        /* Set bits in bitmap corresponding to keys */
                        node->BitMapKey = (1UL<<keypart) | (1UL<<keypart2);
                        SetSubTrie(hamt, node, newnodes);
                        *replace = 1;
                        return data;
                    }
                }
            }
        }

        /* Subtrie: look up in bitmap */
        keypartbits += 5;
        if (keypartbits > 30) {
            /* Exceeded 32 bits of current key: rehash */
            key = hamt->ReHashKey(str, level);
            keypartbits = 0;
        }
        keypart = (key >> keypartbits) & 0x1F;
        if (!(node->BitMapKey & (1<<keypart))) {
            /* bit is 0 in bitmap -> add node to table */
            unsigned long Size;

            /* set bit to 1 */
            node->BitMapKey |= 1<<keypart;

            /* Count total number of bits in bitmap to determine new size */
            BitCount(Size, node->BitMapKey);
            Size &= 0x1F;
            if (Size == 0)
                Size = 32;
            newnodes = def_xmalloc(Size*sizeof(HAMTNode));

            /* Count bits below to find where to insert new node at */
            BitCount(Map, node->BitMapKey & ~((~0UL)<<keypart));
            Map &= 0x1F;        /* Clamp to <32 */
            /* Copy existing nodes leaving gap for new node */
            memcpy(newnodes, GetSubTrie(node), Map*sizeof(HAMTNode));
            memcpy(&newnodes[Map+1], &(GetSubTrie(node))[Map],
                   (Size-Map-1)*sizeof(HAMTNode));
            /* Delete old subtrie */
            def_xfree(GetSubTrie(node));
            /* Set up new node */
            newnodes[Map].BitMapKey = key;
            entry = def_xmalloc(sizeof(HAMTEntry));
            entry->str = str;
            entry->data = data;
            STAILQ_INSERT_TAIL(&hamt->entries, entry, next);
            SetValue(hamt, &newnodes[Map], entry);
            SetSubTrie(hamt, node, newnodes);

            *replace = 1;
            return data;
        }

        /* Count bits below */
        BitCount(Map, node->BitMapKey & ~((~0UL)<<keypart));
        Map &= 0x1F;    /* Clamp to <32 */

        /* Go down a level */
        level++;
        node = &(GetSubTrie(node))[Map];
    }
}
/*@=temptrans =kepttrans =mustfree@*/

void *
HAMT_search(HAMT *hamt, const char *str)
{
    HAMTNode *node;
    unsigned long key, keypart, Map;
    int keypartbits = 0;
    int level = 0;
    
    key = hamt->HashKey(str);
    keypart = key & 0x1F;
    node = &hamt->root[keypart];

    if (!node->BaseValue)
        return NULL;

    for (;;) {
        if (!(IsSubTrie(node))) {
            if (node->BitMapKey == key
                && hamt->CmpKey(((HAMTEntry *)(node->BaseValue))->str,
                                str) == 0)
                return ((HAMTEntry *)(node->BaseValue))->data;
            else
                return NULL;
        }

        /* Subtree: look up in bitmap */
        keypartbits += 5;
        if (keypartbits > 30) {
            /* Exceeded 32 bits of current key: rehash */
            key = hamt->ReHashKey(str, level);
            keypartbits = 0;
        }
        keypart = (key >> keypartbits) & 0x1F;
        if (!(node->BitMapKey & (1<<keypart)))
            return NULL;        /* bit is 0 in bitmap -> no match */

        /* Count bits below */
        BitCount(Map, node->BitMapKey & ~((~0UL)<<keypart));
        Map &= 0x1F;    /* Clamp to <32 */

        /* Go down a level */
        level++;
        node = &(GetSubTrie(node))[Map];
    }
}




void *
def_xmalloc(size_t size)
{
    void *newmem;

    if (size == 0)
        size = 1;
    newmem = malloc(size);
    if (!newmem)
        yasm__fatal(N_("out of memory"));

    return newmem;
}
/*
static void *
def_xcalloc(size_t nelem, size_t elsize)
{
    void *newmem;

    if (nelem == 0 || elsize == 0)
        nelem = elsize = 1;

    newmem = calloc(nelem, elsize);
    if (!newmem)
        yasm__fatal(N_("out of memory"));

    return newmem;
}
//*/

/*
static void *
def_xrealloc(void *oldmem, size_t size)
{
    void *newmem;

    if (size == 0)
        size = 1;
    if (!oldmem)
        newmem = malloc(size);
    else
        newmem = realloc(oldmem, size);
    if (!newmem)
        yasm__fatal(N_("out of memory"));

    return newmem;
}
//*/
void def_xfree(void *p)
{
    if (!p)
        return;
    free(p);
}



static void
def_fatal(const char *fmt, va_list va)
{
    fprintf(stderr, "%s: ", N_("FATAL"));
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

void
yasm__fatal(const char *message, ...)
{
    va_list va;
    va_start(va, message);
    def_fatal(message, va);
    /*@notreached@*/
    va_end(va);
}

void
def_internal_error_(const char *file, unsigned int line, const char *message)
{
    fprintf(stderr,
            N_("INTERNAL ERROR at %s, line %u: %s\n"),
            file, line, message);

    exit(EXIT_FAILURE);

}

