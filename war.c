#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define DokRequestTAG   1
#define DokResponseTAG  2
#define UnlockDokTAG    3

int N;                                          //liczba okrętów
int M;                                          //liczba dostępnych mechaników
int K;                                          //liczba dostępnych doków

int tid;					//id procesu
int m = 1;					//liczba wymaganych przez okręt mechaników
long my_c=0;					//zegar wirtualny

int DokRequestTime;				//zmienna przechowuje czas wirtualny ustawienia DokRequestSender na wartość 1
int DokResponseTime;				//zmienna przechowuje czas wirtualny ustawienia wartości !=-1 zmiennej DokResponseSender
int UnlockDokTime;				//zmienna przechowuje czas wirtualny ustawienia UnlockSender na wartość 1

int PositionLastWithTechnican = 0;		//zmienna przechowuje miejsce w kolejce ostatniego okrętu dala ktorego wystarcza mechaników
int MyPosition = 0;				//zmienna przechowuje pozycję proceu w kolejce

struct queue *head = NULL;                      //wskaźnik wskazujący na głowę listy stojących w kolejce okrętów
struct queue *previous, *current = NULL;        //wskaźnik wykorzystywany podczas przeglądania kolejki okrętów

pthread_mutex_t zamek = PTHREAD_MUTEX_INITIALIZER; //zamek, który chroni przed jednoczesnym przeglądanem listy przez dwa wątki

MPI_Request request;
MPI_Status status;

int size;
int thread_status;

int DokRequestSender = 0;                       //zmienna, która informuje wątek komunikacyjny o potrzebie wysłania wiadomości z tagiem DokRequest
int DokResponseSender = -1;                     //zmienna, która informuje wątek komunikacyjny o potrzebie wysłania wiadomości z tagiem DokResponse
int UnlockDokSender = 0;                        //zmienna, która informuje wątek komunikacyjny o potrzebie wysłania wiadomości z tagiem UnlockDok

int responses = 0;				//licznik służący do przechowywania liczby odebranych DokResponsów
int pom = 1;					//zmienna sterująca pętlą na wątku komunikacyjnym
int flag = 0;					//zmienna informująca czy w ramach danego nasłuchiwania pojawiła się wiadomość w kanale
int temp = 0;					//zmienna informująca czy zostało aktywowane nasłuchiwanie waidomości
pthread_t tid2;					//zmienna przechowująca id wątku komunikacyjnego

struct packet
{
    int nadawca_id;				//id nadawcy
    int c;    					//zegar wirtualny nadawcy
    int m;					//liczba wymaganych przez okręt mechaników
};

struct queue
{
    int nadawca_id;
    int c;
    int m;
    struct queue * next;
};

// PROCEDURY OD ZARZĄDZANIA LISTĄ
int max (int a, int b)
{
    if(a > b)
        return a;
    else
        return b;
}
void add_with_sort(struct packet packet)
{
    pthread_mutex_lock(&zamek);
    if(head == NULL)
    {
        current = malloc(sizeof(struct queue));
        current->nadawca_id = packet.nadawca_id;
        current->c = packet.c;
        current->m = packet.m;
        current->next = NULL;
        head = current;
    }
    else
    {
        previous = head;
        current = head->next;
        int czy = 1;

        struct queue *new = malloc(sizeof(struct queue));
        new->nadawca_id = packet.nadawca_id;
        new->c = packet.c;
        new->m = packet.m;

        // GDY JEST JEDEN ELEMENT NA LIŚCIE
        if(current == NULL)
        {
            if(packet.c < previous->c || ( packet.c == previous->c && packet.nadawca_id < previous->nadawca_id))
            {
                new->next = head;
                head = new;
            }
            else
            {
                new->next = NULL;
                previous->next = new;
            }
            czy = 0;
        }
        else if(packet.c < previous->c || ( packet.c == previous->c && packet.nadawca_id < previous->nadawca_id))
        {
            czy = 0;
            new->next = head;
            head = new;
        }

        while( current != NULL && czy)
        {
            if(   (  packet.c > previous->c || ( packet.c == previous->c && packet.nadawca_id > previous->nadawca_id) ) &&  (packet.c < current->c || ( packet.c == current->c && packet.nadawca_id < current->nadawca_id)) )
            {
                // za head'a
                if(previous == head)
                {
                    new->next = head->next;
                    head->next = new;
                }
                else
                {
                    new->next = current;
                    previous->next = new;
                }
                czy = 0;
            }
            previous = previous->next;
            current = current->next;
        }

        if(current == NULL && czy)
        {
            new->next = NULL;
            previous->next = new;
        }
    }
    pthread_mutex_unlock(&zamek);
}

void delete(int nadawca_id)
{
    pthread_mutex_lock(&zamek);
    current = head;
    if (current->nadawca_id == nadawca_id)
        head = head->next;
    while(current != NULL)
    {
        if( current->next != NULL && (current->next)->nadawca_id == nadawca_id )
        {
	    current->next = (current->next)->next;
            pthread_mutex_unlock(&zamek);
	    return;
        }
	current = current->next;
    }
    pthread_mutex_unlock(&zamek);
    return;
}

int IndexOf()
{
    int index = 0;
    pthread_mutex_lock(&zamek);
    current = head;
    while(current != NULL)
    {
        if(current->nadawca_id == tid)
        {
            pthread_mutex_unlock(&zamek);
            return index;
        }
        index ++;
        current = current->next;
    }
    pthread_mutex_unlock(&zamek);
    return index;
}

int IndexOfLastWithTechnican()
{
    int index = 0;
    int suma = 0;
    pthread_mutex_lock(&zamek);
    current = head;
    while(current != NULL)
    {
	suma += current->m;
        if(suma == M || current->next == NULL)
        {
	    pthread_mutex_unlock(&zamek);
	    return index;
	}
	else if (suma > M)
	{
	    pthread_mutex_unlock(&zamek);
	    return index-1;
	}
        index ++;
        current = current->next;
    }
    pthread_mutex_unlock(&zamek);
    return index;
}

void show()
{
    pthread_mutex_lock(&zamek);
    current=head;
    while(current != NULL)
    {
        printf("%i  %li %i %i %i\n", tid, my_c, current->nadawca_id, current->c, current->m);
        current = current->next;
    }
    pthread_mutex_unlock(&zamek);
}

// PROCEDURY OD ZARZĄDZANIA OKRĘTAMI
void DokRequest()
{
    my_c++;
    DokRequestTime = my_c;
    struct packet packet = {.nadawca_id = tid, .c = DokRequestTime, .m = m};
    add_with_sort(packet);
    
    DokRequestSender = 1;
}

void DokRequestResponse(struct packet packet)
{
    add_with_sort(packet);
    
    DokResponseTime = my_c;
    DokResponseSender = packet.nadawca_id;
}

void InDok()
{
    my_c++;
    printf("%i  %li InDok %i\n", tid, my_c, IndexOf());
    show();
}

void InRepair()
{
    my_c++;
    printf("%i  %li InRepair %i\n", tid, my_c,m);
    usleep(rand()%2000);
}

void Unlock()
{
    my_c++;
    delete(tid);
    printf("%i  %li InUnlock\n", tid, my_c);
    
    UnlockDokTime = my_c;
    UnlockDokSender = 1;
}

void *answer ()
{
    while(pom)
    {
        if(DokRequestSender)
        {
            int i=0;
            for(i=0; i<N; i++)
            {
                if(i != tid)
                {
                    struct packet DokRequestPacket = {.nadawca_id = tid, .c = DokRequestTime, .m = m};
                    printf("%i  %i SendDokRequest to %i\n", DokRequestPacket.nadawca_id, DokRequestPacket.c, i);
                    MPI_Send( (void*)&DokRequestPacket, sizeof(struct packet), MPI_BYTE, i, DokRequestTAG, MPI_COMM_WORLD);
                }
            }
            DokRequestSender = 0;
        }

        if(DokResponseSender != -1)
        {
            struct packet DokResponsePacket = {.nadawca_id = tid, .c = DokResponseTime, .m = m};
            printf("%i  %i SendDokRequestResponse to %i\n", DokResponsePacket.nadawca_id, DokResponsePacket.c, DokResponseSender);
            MPI_Send( (void*)&DokResponsePacket, sizeof(struct packet), MPI_BYTE, DokResponseSender, DokResponseTAG, MPI_COMM_WORLD);

            DokResponseSender = -1;
        }

        if(UnlockDokSender)
        {
            int i=0;
            for(i=0; i<N; i++)
                if(i != tid)
                {
                    struct packet UnlockPacket = {.nadawca_id = tid, .c = UnlockDokTime, .m = m};
                    MPI_Send( (void*)&UnlockPacket, sizeof(struct packet), MPI_BYTE, i, UnlockDokTAG, MPI_COMM_WORLD);
                    printf("%i  %i SendUnlock to %i\n", UnlockPacket.nadawca_id, UnlockPacket.c, i);
                }

            UnlockDokSender = 0;
        }
        struct packet packet;
        if(!temp)
        {
            MPI_Irecv((void*)&packet, sizeof(struct packet), MPI_BYTE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
            temp = 1;
        }
        else
        {
            MPI_Test(&request, &flag, &status);
            if(!flag)
            {}
            else
            {
                flag = 0;
                temp = 0;
                //printf("PROCES : %i otrzymal od: %i  tag: %i\n", tid, status.MPI_SOURCE, status.MPI_TAG);

                my_c = max(my_c, packet.c)+1;

                if(status.MPI_TAG == DokRequestTAG)
                {
		     DokRequestResponse(packet);
		     PositionLastWithTechnican = IndexOfLastWithTechnican();
		     MyPosition = IndexOf();
		}
                else if(status.MPI_TAG == DokResponseTAG)
                    responses++;

                else if(status.MPI_TAG == UnlockDokTAG)
                {
                    show();
		    printf("%i  %li ReceiveUnlock %i\n", tid, my_c, packet.nadawca_id);
                    delete(packet.nadawca_id);
		    PositionLastWithTechnican = IndexOfLastWithTechnican();
		    MyPosition = IndexOf();
		    printf("Show #2\n");
		    show();
                }
            }
        }
    }
}

void Init(int argc, char **argv)
{
    N = atoi(argv[1]);
    M = atoi(argv[2]);
    K = atoi(argv[3]);

    MPI_Init_thread(&argc, &argv,  MPI_THREAD_SERIALIZED, &thread_status);// MPI_THREAD_SERIALIZED

    MPI_Comm_size( MPI_COMM_WORLD, &size );
    MPI_Comm_rank( MPI_COMM_WORLD, &tid );
    pthread_create(&tid2, NULL, &answer, NULL);
    srand(time(NULL) + tid);
    m = 1 + (int) rand() % (M/2);
}

int main(int argc, char **argv)
{
    if(argc < 4)
        printf("Podałeś za mało argumentów\nWymagane N M K\n");
    else
    {
	Init(argc, argv);
        //sleep aby wszystkie w jednej chwili nie zaczynały ubiegania się o sekcję krytyczną
       
	/*struct packet packet = {.nadawca_id = 0, .c = 0, .m = 0};
	struct packet packet2 = {.nadawca_id = 0, .c = 1, .m = 2};
	add_with_sort(packet);
	add_with_sort(packet2);
	show();
	delete(0);
	printf("DŻOLO\n");
	show();*/
	
        while(1){
	    responses = 0;
	    MyPosition = IndexOf();
	    PositionLastWithTechnican = IndexOfLastWithTechnican();         
	    sleep(rand() % 5);
            DokRequest();

            //czekaj dopóki wszyscy nie odpowiedzą
            //ODBIÓR
            while(responses < N-1){}
            show();

            //czekaj dopóki nie ma dla ciebie wolnego doku
            //ODBIÓR
            while(IndexOf() >= K) {}

            InDok();

            //czekaj dopóki nie ma dla ciebie wystarczającej liczby techników
            //ODBIÓR
            while(PositionLastWithTechnican < MyPosition) {}

            InRepair();

            Unlock();
	    usleep(2000);
        }
        while(UnlockDokSender != 0) {}
        pom = 0;
        pthread_join(tid2,NULL);
        MPI_Finalize();
    }
    return 0;
}
