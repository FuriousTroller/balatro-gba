#include "sort.h"

#include <stdio.h>

static inline void* get_element_at()
{

}

void insertion_sort(SortArgs args)
{
    for (int i = 1; i < args.size; i++)
    {
        void* key = args.array[0] + (i * args.elem_size());
        printf("key: %c\n", *(char*)key);
        int j;

        // Shift elements that don't satisfy the comparison
        for (j = i - 1; j >= 0 && !args.compare(args.array[j], key); j--)
        {
            args.array[j + 1] = args.array[j];
        }
        args.array[j + 1] = key;
    }
}
