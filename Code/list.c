#include "list.h"
//structure used as node of the list
struct Node
{
	int data; 			// data of the list node
	list_node* next;	// pointer to the next list node
};	

//create list head pointer
list_node* list_init()
{
	list_node* head = NULL;
	
	return head;
}

//function used to add a new list node with an integer value
list_node* list_add_element(list_node* head, int data)
{
	list_node* aux;

	aux = malloc(sizeof(list_node));
	if(aux == NULL)
	{
		printf("Failed to add new element to list.\n");
		return head;
	}
	//replaces the head by the new node. the original head is now the 2nd element of the list
	aux->data = data;
	aux->next = head;
	
	return aux;
}

//function used to get next list node
list_node* list_get_next(list_node* node)
{
	if(node == NULL) return NULL;
	return node->next;
}

//function used to get list node data 
int list_get_data(list_node* node)
{
	if(node == NULL) return -1;
	return node->data;
}

//function used to remove a list node from the list
void list_remove_element(list_node** head, list_node* node, list_node* prev)
{
	//checks if there is list and the current node isnt null
	if(*head == NULL || node == NULL) return;
	//if the node is the head
	if(prev == NULL)
	{		
		*head = node->next;
		free(node);
		return;
	}
	//if the node exists and its not the head
	prev->next = node->next;
	free(node);

	return;
}

//function used to destroy the list(free allocated memory)
list_node* list_destroy(list_node* head)
{	
	if(head == NULL)
		return NULL;
	list_node* aux = head->next;

	while(aux != NULL)
	{
		free(head);
		head = aux;
		aux = aux->next;
	}
	
	free(head);
	
	return NULL;
}