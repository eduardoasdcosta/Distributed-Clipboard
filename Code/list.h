#include <stdlib.h>
#include <stdio.h>

typedef struct Node list_node; //definition of the node of the list

list_node* list_init(); //initializes list
list_node* list_add_element(list_node* head, int data); //adds a list element
list_node* list_get_next(list_node* node); //gets the next list element
int list_get_data(list_node* node); //gets data of the list element
void list_remove_element(list_node** head, list_node* node, list_node* prev); //removes element of the list
list_node* list_destroy(list_node* head); //destroys list and frees memory
