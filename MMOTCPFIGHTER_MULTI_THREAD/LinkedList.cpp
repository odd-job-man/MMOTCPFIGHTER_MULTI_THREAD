#include "LinkedList.h"

void LinkToLinkedListFirst(LINKED_NODE** ppHead, LINKED_NODE** ppTail, LINKED_NODE* pNew)
{
	if (!(*ppHead))
	{
		// list is empty
		*ppTail = *ppHead = pNew;
		(*ppHead)->pNext = nullptr;
		(*ppHead)->pPrev = nullptr;
	}
	else
	{
		pNew->pNext = (*ppHead);
		(*ppHead)->pPrev = pNew;
		(*ppHead) = pNew;
		pNew->pPrev = nullptr;
	}
}

void LinkToLinkedListLast(LINKED_NODE** ppHead, LINKED_NODE** ppTail, LINKED_NODE* pNew)
{
	if (!(*ppHead))
	{
		// list is empty
		*ppTail = *ppHead = pNew;
		(*ppHead)->pNext = nullptr;
		(*ppHead)->pPrev = nullptr;
	}
	else
	{
		pNew->pPrev = (*ppTail);
		(*ppTail)->pNext = pNew;
		(*ppTail) = pNew;
		pNew->pNext = nullptr;
	}
}

void UnLinkFromLinkedList(LINKED_NODE** ppHead, LINKED_NODE** ppTail, LINKED_NODE* pVictim)
{
	//#ifdef _DEBUG
		// Check the integrity of the connections between the node you want to delete and its previous node 
	if (pVictim->pPrev && pVictim->pPrev->pNext != pVictim)
	{
		__debugbreak();
	}
	//#endif

	if (pVictim->pPrev)
	{
		pVictim->pPrev->pNext = pVictim->pNext;
	}
	else
	{
		// pVictim is the first node in the linked list,then update ppHead to pVictim's next node
//#ifdef _DEBUG
		if (pVictim != (*ppHead))
		{
			// pHead not referencing pVictim even though there are no previous nodes of pVictim
			__debugbreak();
		}
		//#endif
		(*ppHead) = pVictim->pNext;
	}

	if (pVictim->pNext)
	{
		pVictim->pNext->pPrev = pVictim->pPrev;
	}
	else
	{
		// pVictim is the last node in the linked list, update ppTail to pVictim's previous node
#ifdef _DEBUG
		if (pVictim != (*ppTail))
		{
			// pTail not referencing pVictim even though there are no next nodes of pVictim
			__debugbreak();
		}
#endif 
		(*ppTail) = pVictim->pPrev;
	}

	pVictim->pPrev = nullptr;
	pVictim->pNext = nullptr;
}