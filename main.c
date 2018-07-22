/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   main.c
 * Author: palemoon
 *
 * Created on 11 июня 2018 г., 23:56
 */

#include <stdio.h>
#include <stdlib.h>
#include "hamt.h"
/*
 * 
 */

static void
no_delete(void *data)
{
}

int main(int argc, char** argv) {

    //int val=100;
    int replace=0;
    struct HAMT *H = HAMT_create(1, def_internal_error_);
    int *V=malloc(sizeof(int));
    *V=123;
    HAMT_insert(H, "abc", V, &replace, no_delete);
    V=malloc(sizeof(int));
    *V=1234;
    HAMT_insert(H, "def", V, &replace, no_delete);
    V=malloc(sizeof(int));
    *V=12345;
    HAMT_insert(H, "hij", V, &replace, no_delete);
    V=malloc(sizeof(int));
    *V=123456;
    HAMT_insert(H, "hlm", V, &replace, no_delete);
    V=malloc(sizeof(int));
    *V=1234567;
    HAMT_insert(H, "opz", V, &replace, no_delete);
    
    
    
    
    const HAMTEntry *E = HAMT_first(H);
    

    for(;;){
    printf("%s -> %i\n",E->str,*(int*)E->data);
    E=HAMT_next(E);
    if(!E)
        break;
    }
    
    
    
    return (EXIT_SUCCESS);
}

