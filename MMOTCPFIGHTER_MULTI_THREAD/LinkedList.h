#pragma once
struct LINKED_NODE
{
	LINKED_NODE* pNext = nullptr;
	LINKED_NODE* pPrev = nullptr;
};

void LinkToLinkedListFirst(LINKED_NODE** ppHead, LINKED_NODE** ppTail, LINKED_NODE* pNew);
void LinkToLinkedListLast(LINKED_NODE** ppHead, LINKED_NODE** ppTail, LINKED_NODE* pNew);
void UnLinkFromLinkedList(LINKED_NODE** ppHead, LINKED_NODE** ppTail, LINKED_NODE* pVictim);
