/*
Simple Implementation of Ring buffer
*/

#include <linux/mutex.h>

#define MAX_SIZE 100

//Globals
static DEFINE_MUTEX(kernel_lock);
static int current_index = 0, remove_index = 0,current_size = 0;


/*
Linux mutex operations

mutex_unlock –release the mutex
mutex_lock –get the mutex(can block)
mutex_lock_interruptible –get the mutex, but allow interrupts
mutex_trylock –try to get the mutexwithout blocking, otherwise return an error 
mutex_is_locked –determine if mutexis locked
*/

struct RBuffer
{
	long timestamp;
};


void RBuffer_Init(struct RBuffer* pRBuffer)
{
	mutex_init(&kernel_lock);
	current_index = remove_index = current_size = 0;
}

int RBuffer_Insert(struct RBuffer* pRBuffer, long value)
{
	if (current_size  >= MAX_SIZE)
	{
		return 1;
	}

	//Protect current_index and current_size
	mutex_lock(&kernel_lock);

	pRBuffer[current_index].timestamp = value;

	if (current_index + 1 >= MAX_SIZE)
	{
		current_index = 0;
	}
	else
	{
		current_index++;
	}

	current_size++;
	mutex_unlock(&kernel_lock);

	return 0;
}




long RBuffer_Remove(struct RBuffer* pRBuffer)
{
	if (current_size == 0)
	{
		return 0;
	}

	//Protect remove_index and current_size
	mutex_lock(&kernel_lock);

	long ts = pRBuffer[remove_index].timestamp;
	pRBuffer[remove_index].timestamp = 0;

	if ( remove_index +1 >= MAX_SIZE)
	{
		remove_index = 0;
	}
	else
	{
		remove_index++;
	}

	current_size--;

	mutex_unlock(&kernel_lock);

	return ts;


int RBuffer_Size(void)
{
	return current_size;
}

int RBuffer_Index(void)
{
	return current_index;
}

void RBuffer_ShowContents(struct RBuffer* pRBuffer)
{
	int i;
	printk("\n\n");
	for (i = 0; i < MAX_SIZE; i++)
	{
		printk(" Value = %ld Index = %d  current index = %d remove index = %d "
			"size = %d\n", pRBuffer[i].timestamp, i, current_index, remove_index, current_size);
	}
}

