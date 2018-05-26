#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#define HEADER_OF_ELECTION
const int COMPLETED_VOTER=3;
const int ASSIGNED_VOTER=2;
const int WAITING_VOTER=1;
const int NEW_VOTER=0;

struct Booth;
struct EVM ;
struct Voter ;

typedef struct Voter
{
	int status;
	int idx;
	struct Booth * booth;
	struct EVM * evm;
	pthread_t voter_thread_id;
}Voter;

typedef struct EVM
{
	int flag;
	struct Booth * booth;
	int idx;
	pthread_t evm_thread_id;
	int number_of_slots;
}EVM;

typedef struct Booth
{
	struct Voter ** voters;
	struct EVM ** evms;
	pthread_t booth_thread_id;
	pthread_mutex_t mutex;
	pthread_cond_t cv_2;
	pthread_cond_t cv_1;
	int number_of_evms;
	int max_slots_in_evm;
	int number_of_voters;
	int done_voters;
	int idx;
}Booth;

Voter* voter_init(int , Booth *, Voter *);

void * voter_thread(void * args)
{
	Voter * voter = (Voter*)args;
	voter->status = WAITING_VOTER;
	pthread_cond_t * cv_2_ptr;
	pthread_cond_t * cv_1_ptr;
	pthread_mutex_t * mutex_ptr;
	cv_2_ptr = &(voter->booth->cv_2);
	cv_1_ptr = &(voter->booth->cv_1);
	mutex_ptr = &(voter->booth->mutex);
	pthread_mutex_lock(mutex_ptr);
	EVM * evm;
	while(1)
	{
		if(voter->status == WAITING_VOTER)
			pthread_cond_wait(cv_1_ptr, mutex_ptr);
		else
		{
			pthread_mutex_unlock(mutex_ptr);
			break;
		}
	}
	evm = voter->evm;
	pthread_mutex_lock(mutex_ptr);
	while(1)
	{
		if(evm->flag == 0)
			pthread_cond_wait(cv_1_ptr, mutex_ptr);
		else
		{
			pthread_cond_broadcast(cv_2_ptr);
			pthread_mutex_unlock(mutex_ptr);
			break;
		}
	}
	(evm->number_of_slots)--;
	printf("%d booth %d evm %d voter, done voting.\n",evm->booth->idx+1, evm->idx+1, voter->idx+1);
	return NULL;
}
int getrand(int mod)
{
	return rand()%mod+1;
}
void * evm_thread(void * args)
{
	EVM * evm;
	evm = (EVM*)args;
	Booth * booth;
	booth = evm->booth;
	pthread_cond_t * cv_2_ptr;
	pthread_cond_t * cv_1_ptr = &(booth->cv_1);
	pthread_mutex_t * mutex_ptr;
	cv_2_ptr = &(booth->cv_2);
	mutex_ptr = &(booth->mutex);
	int i, j, max_;
	while(1)
	{
		pthread_mutex_lock(mutex_ptr);
		if(booth->number_of_voters == booth->done_voters )
		{
			pthread_mutex_unlock(mutex_ptr);
			break;
		}
		else
		{
			pthread_mutex_unlock(mutex_ptr);
			pthread_mutex_lock(mutex_ptr);
			evm->number_of_slots = max_= getrand(booth->max_slots_in_evm);
			evm->flag = 0;
			pthread_mutex_unlock(mutex_ptr);
			printf("%d booth %d evm %d slots free.\n", booth->idx+1, evm->idx+1, max_);
			j = 0;
			while(j<max_)
			{
				i=getrand(booth->number_of_voters)-1;
				pthread_mutex_lock(mutex_ptr);
				if(booth->voters[i]->status == WAITING_VOTER)
				{
					booth->voters[i]->evm = evm;
					j+=1;
					booth->voters[i]->status = ASSIGNED_VOTER;
					(booth->done_voters)+=1;
					printf("%d booth %d evm %d voter alloted.\n", booth->idx+1, evm->idx+1, i+1);
				}
				if(booth->done_voters == booth->number_of_voters)
				{
					pthread_mutex_unlock(mutex_ptr);
					break;
				}
				pthread_mutex_unlock(mutex_ptr);
			}
			if(j==0)break;

			printf("%d booth %d evm active.\n", booth->idx+1, evm->idx+1);
			pthread_mutex_lock(mutex_ptr);
			pthread_cond_broadcast(cv_1_ptr);
			evm->flag = 1;
			evm->number_of_slots = j;
			while(1)
			{
				if(evm->number_of_slots)
					pthread_cond_wait(cv_2_ptr, mutex_ptr);
				else
					break;
			}
			pthread_mutex_unlock(mutex_ptr);
			printf("%d booth %d evm, voting completed.\n", booth->idx+1, evm->idx+1);
		}
	}
	printf("%d booth %d evm closing.\n", booth->idx+1, evm->idx+1);
	return NULL;
}
EVM* evm_init( int idx,EVM * evm, Booth * booth)
{
	evm = (EVM*)malloc(sizeof(EVM));
	evm->flag = 0;
	evm->booth = booth;
	evm->number_of_slots = 0;
	evm->idx = idx;
	return evm;
}

void booth_cleanup(Booth * booth)
{
	int i=0;
	i=0;
	while(i<booth->number_of_voters)
		free(booth->voters[i++]);
	while(i<booth->number_of_evms)
		free(booth->evms[i++]);
	free(booth->evms);
	free(booth->voters);
}

void * booth_thread(void* args)
{
	Booth * booth = (Booth*)args;
	/* evms and voters init */

	int i=0;

	while(i<booth->number_of_voters)
	{
		booth->voters[i] = voter_init( i, booth, booth->voters[i]);
		i++;
	}
	i=0;
	while(i<booth->number_of_evms)
	{
		booth->evms[i] = evm_init(i,booth->evms[i], booth);
		i++;
	}
	/* evms and voters threads start */
	i=0;
	while(i<booth->number_of_voters)
	{
		pthread_create(&(booth->voters[i]->voter_thread_id),NULL, voter_thread, booth->voters[i]);
		i++;
	}  
	i=0;
	while(i<booth->number_of_evms)
	{
		pthread_create(&(booth->evms[i]->evm_thread_id),NULL, evm_thread, booth->evms[i]);
		i++;
	}
	i=0;
	while(i<booth->number_of_voters)
	{
		pthread_join(booth->voters[i]->voter_thread_id, 0);
		i++;
	}
	/* evms and voters threads joined */
	i=0;
	while(i<booth->number_of_evms)
	{
		pthread_join(booth->evms[i]->evm_thread_id, 0);
		i++;
	}
	printf("%d booth is closing.\n", booth->idx+1);
	booth_cleanup(booth);
	return NULL;
}

Booth* booth_init( int idx,Booth * booth, int number_of_voters, int max_slots_in_evm,int number_of_evms)
{
	booth = (Booth*)malloc(sizeof(Booth));
	booth->evms = (EVM**)malloc(sizeof(EVM*)*number_of_evms);
	pthread_mutex_init(&(booth->mutex), NULL);
	booth->done_voters = 0;
	booth->number_of_evms = number_of_evms;
	booth->number_of_voters = number_of_voters;
	pthread_cond_init(&(booth->cv_2), NULL);
	booth->idx = idx;
	booth->voters = (Voter**)malloc(sizeof(Voter*)*number_of_voters);
	booth->max_slots_in_evm = max_slots_in_evm;
	pthread_cond_init(&(booth->cv_1), NULL);
	return booth;
}

Voter* voter_init( int idx, Booth * booth, Voter * voter)
{
	voter = (Voter*)malloc(sizeof(Voter));
	voter->booth = booth;
	voter->evm = NULL;
	voter->idx = idx;
	voter->status = 0;
	return voter;
}
int main()
{	
	int _max_slots_in_evm = 4,number_of_booths = 1;
	srand(time(0));
	scanf("%d", &number_of_booths);
	int * number_of_voters = (int*)malloc(sizeof(int)*number_of_booths),* number_of_evms = (int*)malloc(sizeof(int)*number_of_booths);
	int * max_slots_in_evm = (int*)malloc(sizeof(int)*number_of_booths);
	Booth ** booths = (Booth**)malloc(sizeof(Booth*)*number_of_booths);
	int i=0;
	while(i<number_of_booths)
	{
		scanf("%d%d",number_of_voters+i,number_of_evms+i);
		max_slots_in_evm[i] = _max_slots_in_evm;
		i++;
	}
	printf("ELECTION BEGINS.\n");
	i=0;
	while(i<number_of_booths)
	{
		booths[i] = booth_init(i,*(booths+i),number_of_voters[i],max_slots_in_evm[i],number_of_evms[i]);
		i++;
	}

	/* booth thread start */
	i=0;
	while(i<number_of_booths)
	{
		pthread_create(&((*(booths+i))->booth_thread_id),NULL, booth_thread, *(booths+i));
		i++;
	}
	/* booth thread joined */
	i=0;
	while(i<number_of_booths)
	{
		pthread_join(booths[i]->booth_thread_id, 0);
		i++;
	}
	free(max_slots_in_evm);
	free(number_of_evms);
	i=0;
	while(i<number_of_booths)
	{
		free(*(booths+i));
		i++;
	}
	free(number_of_voters);
	free(booths);
	printf("ELECTION COMPLETED.\n");
	return 0;
}
