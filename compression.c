/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

/**************************************************************
 LZSS.C -- A Data Compression Program
***************************************************************
    4/6/1989 Haruhiko Okumura
    Use, distribute, and modify this program freely.
    Please send me your improved versions.
        PC-VAN      SCIENCE
        NIFTY-Serve PAF01022
        CompuServe  74050,1022

**************************************************************/

#define N         4096  /* size of ring buffer - must be power of 2 */
#define F         18    /* upper limit for match_length */
#define THRESHOLD 2     /* encode string into position and length
                           if match_length is greater than this */
#define NIL       N     /* index for root of binary search trees */

struct encode_state {
    /*
     * left & right children & parent. These constitute binary search trees.
     */
    int lchild[N + 1], rchild[N + 257], parent[N + 1];

    /* ring buffer of size N, with extra F-1 bytes to aid string comparison */
    u_int8_t text_buf[N + F - 1];

    /*
     * match_length of longest match.
     * These are set by the insert_node() procedure.
     */
    int match_position, match_length;
};


/*******************************************************************************
*******************************************************************************/
int decompress_lzss(
    u_int8_t       * dst,
    u_int32_t        dstlen,
    u_int8_t       * src,
    u_int32_t        srclen)
{
    /* ring buffer of size N, with extra F-1 bytes to aid string comparison */
    u_int8_t text_buf[N + F - 1];
    u_int8_t * dststart = dst;
    const u_int8_t * dstend = dst + dstlen;
    const u_int8_t * srcend = src + srclen;
    int  i, j, k, r, c;
    unsigned int flags;
    
    dst = dststart;
    for (i = 0; i < N - F; i++)
        text_buf[i] = ' ';
    r = N - F;
    flags = 0;
    for ( ; ; ) {
        if (((flags >>= 1) & 0x100) == 0) {
            if (src < srcend)
		c = *src++;
	    else
	        break;
            flags = c | 0xFF00;  /* uses higher byte cleverly */
        }   /* to count eight */
        if (flags & 1) {
            if (src < srcend)
		c = *src++;
	    else
		break;
            if (dst < dstend)
		*dst++ = c;
	    else
		break;
            text_buf[r++] = c;
            r &= (N - 1);
        } else {
            if (src < srcend)
		i = *src++;
            else
		break;
            if (src < srcend)
		j = *src++;
            else
		break;
            i |= ((j & 0xF0) << 4);
            j  =  (j & 0x0F) + THRESHOLD;
            for (k = 0; k <= j; k++) {
                c = text_buf[(i + k) & (N - 1)];
                if (dst < dstend)
		    *dst++ = c;
		else
		    break;
                text_buf[r++] = c;
                r &= (N - 1);
            }
        }
    }
    
    return (int)(dst - dststart);
}

/*
 * initialize state, mostly the trees
 *
 * For i = 0 to N - 1, rchild[i] and lchild[i] will be the right and left 
 * children of node i.  These nodes need not be initialized.  Also, parent[i] 
 * is the parent of node i.  These are initialized to NIL (= N), which stands 
 * for 'not used.'  For i = 0 to 255, rchild[N + i + 1] is the root of the 
 * tree for strings that begin with character i.  These are initialized to NIL. 
 * Note there are 256 trees. */
static void init_state(struct encode_state *sp)
{
    int  i;

    bzero(sp, sizeof(*sp));

    for (i = 0; i < N - F; i++)
        sp->text_buf[i] = ' ';
    for (i = N + 1; i <= N + 256; i++)
        sp->rchild[i] = NIL;
    for (i = 0; i < N; i++)
        sp->parent[i] = NIL;
}

/*
 * Inserts string of length F, text_buf[r..r+F-1], into one of the trees
 * (text_buf[r]'th tree) and returns the longest-match position and length
 * via the global variables match_position and match_length.
 * If match_length = F, then removes the old node in favor of the new one,
 * because the old one will be deleted sooner. Note r plays double role,
 * as tree node and position in buffer.
 */
static void insert_node(struct encode_state *sp, int r)
{
    int  i, p, cmp;
    u_int8_t  *key;

    cmp = 1;
    key = &sp->text_buf[r];
    p = N + 1 + key[0];
    sp->rchild[r] = sp->lchild[r] = NIL;
    sp->match_length = 0;
    for ( ; ; ) {
        if (cmp >= 0) {
            if (sp->rchild[p] != NIL)
                p = sp->rchild[p];
            else {
                sp->rchild[p] = r; 
                sp->parent[r] = p;
                return;
            }
        } else {
            if (sp->lchild[p] != NIL)
                p = sp->lchild[p];
            else {
                sp->lchild[p] = r;
                sp->parent[r] = p;
                return;
            }
        }
        for (i = 1; i < F; i++) {
            if ((cmp = key[i] - sp->text_buf[p + i]) != 0)
                break;
        }
        if (i > sp->match_length) {
            sp->match_position = p;
            if ((sp->match_length = i) >= F)
                break;
        }
    }
    sp->parent[r] = sp->parent[p];
    sp->lchild[r] = sp->lchild[p];
    sp->rchild[r] = sp->rchild[p];
    sp->parent[sp->lchild[p]] = r;
    sp->parent[sp->rchild[p]] = r;
    if (sp->rchild[sp->parent[p]] == p)
        sp->rchild[sp->parent[p]] = r;
    else
        sp->lchild[sp->parent[p]] = r;
    sp->parent[p] = NIL;  /* remove p */
}

/* deletes node p from tree */
static void delete_node(struct encode_state *sp, int p)
{
    int  q;
    
    if (sp->parent[p] == NIL)
        return;  /* not in tree */
    if (sp->rchild[p] == NIL)
        q = sp->lchild[p];
    else if (sp->lchild[p] == NIL)
        q = sp->rchild[p];
    else {
        q = sp->lchild[p];
        if (sp->rchild[q] != NIL) {
            do {
                q = sp->rchild[q];
            } while (sp->rchild[q] != NIL);
            sp->rchild[sp->parent[q]] = sp->lchild[q];
            sp->parent[sp->lchild[q]] = sp->parent[q];
            sp->lchild[q] = sp->lchild[p];
            sp->parent[sp->lchild[p]] = q;
        }
        sp->rchild[q] = sp->rchild[p];
        sp->parent[sp->rchild[p]] = q;
    }
    sp->parent[q] = sp->parent[p];
    if (sp->rchild[sp->parent[p]] == p)
        sp->rchild[sp->parent[p]] = q;
    else
        sp->lchild[sp->parent[p]] = q;
    sp->parent[p] = NIL;
}

/*******************************************************************************
*******************************************************************************/
u_int8_t * compress_lzss(
    u_int8_t       * dst,
    u_int32_t        dstlen,
    u_int8_t       * src,
    u_int32_t        srclen)
{
    u_int8_t * result = NULL;
    /* Encoding state, mostly tree but some current match stuff */
    struct encode_state *sp;

    int  i, c, len, r, s, last_match_length, code_buf_ptr;
    u_int8_t code_buf[17], mask;
    u_int8_t * srcend = src + srclen;
    u_int8_t *dstend = dst + dstlen;

    /* initialize trees */
    sp = (struct encode_state *) malloc(sizeof(*sp));
    if (!sp) goto finish;

    init_state(sp);

    /*
     * code_buf[1..16] saves eight units of code, and code_buf[0] works
     * as eight flags, "1" representing that the unit is an unencoded
     * letter (1 byte), "0" a position-and-length pair (2 bytes).
     * Thus, eight units require at most 16 bytes of code.
     */
    code_buf[0] = 0;
    code_buf_ptr = mask = 1;

    /* Clear the buffer with any character that will appear often. */
    s = 0;  r = N - F;

    /* Read F bytes into the last F bytes of the buffer */
    for (len = 0; len < F && src < srcend; len++)
        sp->text_buf[r + len] = *src++;  
    if (!len)
        goto finish;

    /*
     * Insert the F strings, each of which begins with one or more
     * 'space' characters.  Note the order in which these strings are
     * inserted.  This way, degenerate trees will be less likely to occur.
     */
    for (i = 1; i <= F; i++)
        insert_node(sp, r - i); 

    /*
     * Finally, insert the whole string just read.
     * The global variables match_length and match_position are set.
     */
    insert_node(sp, r);
    do {
        /* match_length may be spuriously long near the end of text. */
        if (sp->match_length > len)
            sp->match_length = len;
        if (sp->match_length <= THRESHOLD) {
            sp->match_length = 1;  /* Not long enough match.  Send one byte. */
            code_buf[0] |= mask;  /* 'send one byte' flag */
            code_buf[code_buf_ptr++] = sp->text_buf[r];  /* Send uncoded. */
        } else {
            /* Send position and length pair. Note match_length > THRESHOLD. */
            code_buf[code_buf_ptr++] = (u_int8_t) sp->match_position;
            code_buf[code_buf_ptr++] = (u_int8_t)
                ( ((sp->match_position >> 4) & 0xF0)
                |  (sp->match_length - (THRESHOLD + 1)) );
        }
        if ((mask <<= 1) == 0) {  /* Shift mask left one bit. */
                /* Send at most 8 units of code together */
            for (i = 0; i < code_buf_ptr; i++)
                if (dst < dstend)
                    *dst++ = code_buf[i]; 
                else
                    goto finish;
            code_buf[0] = 0;
            code_buf_ptr = mask = 1;
        }
        last_match_length = sp->match_length;
        for (i = 0; i < last_match_length && src < srcend; i++) {
            delete_node(sp, s);    /* Delete old strings and */
            c = *src++;
            sp->text_buf[s] = c;    /* read new bytes */

            /*
             * If the position is near the end of buffer, extend the buffer
             * to make string comparison easier.
             */
            if (s < F - 1)
                sp->text_buf[s + N] = c;

            /* Since this is a ring buffer, increment the position modulo N. */
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);

            /* Register the string in text_buf[r..r+F-1] */
            insert_node(sp, r); 
        }
        while (i++ < last_match_length) {
        delete_node(sp, s);

            /* After the end of text, no need to read, */
            s = (s + 1) & (N - 1); 
            r = (r + 1) & (N - 1);
            /* but buffer may not be empty. */
            if (--len)
                insert_node(sp, r);
        }
    } while (len > 0);   /* until length of string to be processed is zero */

    if (code_buf_ptr > 1) {    /* Send remaining code. */
        for (i = 0; i < code_buf_ptr; i++)
            if (dst < dstend)
                *dst++ = code_buf[i]; 
            else
                goto finish;
    }

    result = dst;

finish:
    if (sp) free(sp);

    return result;
}

#define PLAIN_BUF_MAX_LEN 4096
// just in case, compressed buffer is bigger, since the data is random and has high level of entropy
#define COMPRESSED_BUF_LEN PLAIN_BUF_MAX_LEN*2


void try_seed(uint32_t seed)
{
	printf ("%s(%d)\n", __FUNCTION__, seed);
	srand(seed);

	size_t compressed_len=rand()&(PLAIN_BUF_MAX_LEN-1);

	uint8_t test[PLAIN_BUF_MAX_LEN];
	for (int i=0; i<compressed_len; i++)
		test[i]=rand()&0xFF;

	uint8_t compressed[COMPRESSED_BUF_LEN];
	uint8_t decompressed[PLAIN_BUF_MAX_LEN];

	compress_lzss(compressed, COMPRESSED_BUF_LEN, test, compressed_len);

	decompress_lzss(decompressed, PLAIN_BUF_MAX_LEN, compressed, COMPRESSED_BUF_LEN);

	assert (memcmp (test, decompressed, compressed_len)==0);
};

int main(int argc, char *argv[])
{
	assert (argc==2);
	uint32_t seed;
	assert (sscanf (argv[1], "%d", &seed)==1);

	try_seed (seed);
/*
	try_seed(7);
	try_seed(48);
	try_seed(134);
*/
};