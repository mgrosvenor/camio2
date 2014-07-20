// CamIO 2: camio2_test_uri_parser.c
// Copyright (C) 2013: Matthew P. Grosvenor (matthew.grosvenor@cl.cam.ac.uk) 
// Licensed under BSD 3 Clause, please see LICENSE for more details. 


#include "../src/types/stdinclude.h"
#include "../src/errors/errors.h"
#include "../src/uri_parser/uri_parser.h"

int test1()
{
    uri* u;
    int result = parse_uri("", &u);
    if(result != CIO_ENOERROR){
        return result;
    }
    free_uri(&u);

    return 0;
}


int test2()
{
    char* scheme = "http";

    uri* u;
    int result = parse_uri(scheme, &u);
    if(result != CIO_ENOERROR){
        return result;
    }

    if(u->scheme_name_len != strlen(scheme)){
        return 1;
    }


    if(strncmp(u->scheme_name, scheme, u->scheme_name_len) != 0){
        return 1;
    }

    free_uri(&u);

    return 0;
}


int test3()
{
    char* uri_str = "http:";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }

    if(u->scheme_name_len != strlen(uri_str) -1){
        free_uri(&u);
        return 1;
    }


    if(strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }

    free_uri(&u);

    return 0;
}

int test4()
{
    char* uri_str = "http?";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }

    if(u->scheme_name_len != strlen(uri_str) -1){
        free_uri(&u);
        return 1;
    }


    if(strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }

    free_uri(&u);

    return 0;
}

int test5()
{
    char* uri_str = "http#";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }

    if(u->scheme_name_len != strlen(uri_str) -1){
        free_uri(&u);
        return 1;
    }


    if(strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }

    free_uri(&u);

    return 0;
}

int test6()
{
    char* uri_str = "http://www.example.com:8080";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }


    if(u->scheme_name_len != 4 || strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }


    if(u->hierarchical_len != 22 || strncmp(u->hierarchical, uri_str + 5, 22) != 0){
        free_uri(&u);
        return 1;
    }


    free_uri(&u);
    return 0;
}


int test7()
{
    char* uri_str = "http://www.example.com:8080?";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }


    if(u->scheme_name_len != 4 || strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }


    if(u->hierarchical_len != 22 || strncmp(u->hierarchical, uri_str + 5, 22) != 0){
        free_uri(&u);
        return 1;
    }


    free_uri(&u);
    return 0;
}


int test8()
{
    char* uri_str = "http://www.example.com:8080?key1";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }


    if(u->scheme_name_len != 4 || strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }


    if(u->hierarchical_len != 22 || strncmp(u->hierarchical, uri_str + 5, 22) != 0){
        free_uri(&u);
        return 1;
    }

    if(!u->key_vals || !(u->key_vals->count == 1))
    {
        free_uri(&u);
        return 1;

    }

    if(u->key_vals->first(u->key_vals).value->key_len != 4  || strncmp(u->key_vals->first(u->key_vals).value->key,"key1",4))
    {
        free_uri(&u);
        return 1;
    }


    free_uri(&u);
    return 0;
}

int test9()
{
    char* uri_str = "http://www.example.com:8080?key1,key2,key3";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }


    if(u->scheme_name_len != 4 || strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }


    if(u->hierarchical_len != 22 || strncmp(u->hierarchical, uri_str + 5, 22) != 0){
        free_uri(&u);
        return 1;
    }

    if(!u->key_vals || !(u->key_vals->count == 3))
    {
        free_uri(&u);
        return 1;

    }

    char* keys[3] = { "key1", "key2", "key3" };

    int i = 0;
    for(CH_LIST_IT(KV) it = u->key_vals->first(u->key_vals); it.value; u->key_vals->next(u->key_vals,&it), ++i)
    {
        if(it.value->key_len != strlen(keys[i])  || strncmp(it.value->key,keys[i], strlen(keys[i])))
        {
            free_uri(&u);
            return 1;
        }
    }


    free_uri(&u);
    return 0;
}

int test10()
{
    char* uri_str = "http://www.example.com:8080?key1?key2=val2?key3";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }


    if(u->scheme_name_len != 4 || strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }


    if(u->hierarchical_len != 22 || strncmp(u->hierarchical, uri_str + 5, 22) != 0){
        free_uri(&u);
        return 1;
    }

    if(!u->key_vals || !(u->key_vals->count == 3))
    {
        free_uri(&u);
        return 1;

    }


    char* keys[3] = { "key1", "key2", "key3" };
    char* vals[3] = { "", "val2", "" };

    int i = 0;
    for(CH_LIST_IT(KV) it = u->key_vals->first(u->key_vals); it.value; u->key_vals->next(u->key_vals,&it), ++i)
    {
        if(it.value->key_len != strlen(keys[i])  || strncmp(it.value->key,keys[i], strlen(keys[i])))
        {
            free_uri(&u);
            return 1;
        }

        if(it.value->value_len != strlen(vals[i])  || strncmp(it.value->value,vals[i], strlen(vals[i])) )
        {
            free_uri(&u);
            return 1;
        }

    }


    free_uri(&u);
    return 0;
}


int test11()
{
    char* uri_str = "http://www.example.com:8080?key1?key2=val2?key3#frag1";

    uri* u;
    int result = parse_uri(uri_str, &u);
    if(result != CIO_ENOERROR){
        return result;
    }


    if(u->scheme_name_len != 4 || strncmp(u->scheme_name, uri_str, u->scheme_name_len) != 0){
        free_uri(&u);
        return 1;
    }


    if(u->hierarchical_len != 22 || strncmp(u->hierarchical, uri_str + 5, 22) != 0){
        free_uri(&u);
        return 1;
    }

    if(!u->key_vals || !(u->key_vals->count == 3))
    {
        free_uri(&u);
        return 1;

    }


    char* keys[3] = { "key1", "key2", "key3" };
    char* vals[3] = { "", "val2", "" };

    int i = 0;
    for(CH_LIST_IT(KV) it = u->key_vals->first(u->key_vals); it.value; u->key_vals->next(u->key_vals,&it), ++i)
    {
        if(it.value->key_len != strlen(keys[i])  || strncmp(it.value->key,keys[i], strlen(keys[i])))
        {
            free_uri(&u);
            return 1;
        }

        if(it.value->value_len != strlen(vals[i])  || strncmp(it.value->value,vals[i], strlen(vals[i])) )
        {
            free_uri(&u);
            return 1;
        }

    }

    if( u->fragement_len != 5 || strncmp(u->fragement, "frag1", 5)){
        free_uri(&u);
        return 1;
    }


    free_uri(&u);
    return 0;
}



int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    int test_result = 0;
    printf("Camio2: URI Parser Test 01: ");  printf("%s", (test_result = test1()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 02: ");  printf("%s", (test_result = test2()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 03: ");  printf("%s", (test_result = test3()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 04: ");  printf("%s", (test_result = test4()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 05: ");  printf("%s", (test_result = test5()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 06: ");  printf("%s", (test_result = test6()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 07: ");  printf("%s", (test_result = test7()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 08: ");  printf("%s", (test_result = test8()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 09: ");  printf("%s", (test_result = test9()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 10: ");  printf("%s", (test_result = test10()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    printf("Camio2: URI Parser Test 11: ");  printf("%s", (test_result = test11()) ? "FAIL\n" : "PASS\n"); if(test_result) return 1;
    return 0;
}
