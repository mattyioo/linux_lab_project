#define _POSIX_C_SOURCE 200112L // do uzywania barrier
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h> //funkcja poll
#include <pthread.h>
#include <unistd.h>    //STDIN_FILENO


#define BUF_SIZE 256
#define TRAIN_DATA 0
#define TEST_DATA 1
#define NUM_THREADS 5

char buffer[BUF_SIZE];

pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_barrier_t barrier;

const char *filename_train = "train.csv";
const char *filename_test = "test.csv";

typedef struct
{
    double x, y, z;
    char type;
} Wektor;

typedef struct
{
    Wektor *wektor;
    size_t rozmiar;
    size_t capacity;
} Memory;

typedef struct
{
    const Wektor *wektor_train;
    const Wektor *wektor_test;
    size_t size_test;
    size_t size_train;
    int hits;
    int misses;
    size_t processed;
    size_t total;
    bool is_finished;  // sprawdzamy czy watek sie zakonczyl
    bool stop_request; // sprawdzamy czy wcisnieto przycisk Z
    bool is_paused;    // sprawdzamy czy watek jest wstrzymany
    bool reset;        // sprawdzamy czy nastapil reset
} DaneWatku;

typedef struct
{
    int thread_id; // do rozpoznawiania watkow tzn. watek 0 watek 1 itp.
    DaneWatku *data;
} ThreadInfo;

Wektor *wczyt(const char *filename, Memory *mem)
{
    mem->wektor = NULL;
    mem->rozmiar = 0;
    mem->capacity = 0;
    FILE *pf = fopen(filename, "r");
    if (pf == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }
    if (fgets(buffer, BUF_SIZE, pf) == NULL) // ignorujemy nagłówek, bo nie zawiera zadnych danych
    {
        printf("Blad odczytu pierwszej linii z pliku %s\n", filename);
        fclose(pf);
        exit(1);
    }
    while (fgets(buffer, BUF_SIZE, pf) != NULL)
    {
        if (mem->rozmiar >= mem->capacity) // realloc tylko gdy zabrkanie juz miejsca na nowe dane, aby nie robic tego za kazdym razem gdy wczytany jest wiersz
        {
            if (mem->capacity == 0)
                mem->capacity = 256;
            else
                mem->capacity *= 2;
            Wektor *new_wektor = realloc(mem->wektor, mem->capacity * sizeof(Wektor)); // inicjalizaja tablicy struktur Wektor
            if (new_wektor == NULL)
            {
                printf("[realloc] Error!\n");
                free(mem->wektor);
                exit(1);
            }
            mem->wektor = new_wektor;
        }
        if (sscanf(buffer, "%lf,%lf,%lf,%c", &mem->wektor[mem->rozmiar].x, &mem->wektor[mem->rozmiar].y, &mem->wektor[mem->rozmiar].z, &mem->wektor[mem->rozmiar].type) != 4)
        {
            printf("[sscanf] Zaalokowano mniej niz 4 atrybuty!\n");
            exit(2);
        }
        mem->rozmiar++;
    }
    if (mem->rozmiar < mem->capacity)
    { // zmniejszamy pamiec ktora jest niewykorzystana tak aby idealnie pasowala do rozmiaru naszej tablicy
        Wektor *new_wektor = realloc(mem->wektor, mem->rozmiar * sizeof(Wektor));
        if (new_wektor != NULL)       // jesli realloc sie udal to niech wskazuje na nowa zaalokowana pamiec dobrana do rozmiaru tablicy
            mem->wektor = new_wektor; // jesli sie nie uda to po prostu bedzie nadwyzka pamieci w tablicy i mem->wektor bedzie na to wskazywal
    }
    fclose(pf);
    return mem->wektor; // zwracamy wskaznik na zaalokowana pamiec typu Wektor *
}

// watek do przeprowadznia obliczen
void *calc_thread(void *arg)
{
    ThreadInfo *info = (ThreadInfo *)arg;
    DaneWatku *dane_watku = info->data;

    dane_watku->is_finished = false;
    int local_hits;   // lalal
    int local_misses; // kazdy watek posiada wlasny stos a zmienne lokalne sa na stosie wiec nie trzeba tutaj stosowac tablicy
    size_t local_processed;

    while (1)
    {
        local_hits = 0;
        local_misses = 0; // kazdy watek zeruje swoja zmienna lokalna
        local_processed = 0;
        int ret = pthread_barrier_wait(&barrier);

        // Tylko jeden wątek czyści dane globalne
        if (ret == PTHREAD_BARRIER_SERIAL_THREAD)
        {
            pthread_mutex_lock(&mutex);
            dane_watku->hits = 0;
            dane_watku->misses = 0;
            dane_watku->processed = 0;
            dane_watku->reset = false; // Potwierdzenie wykonania resetu
            pthread_mutex_unlock(&mutex);
        }

        // inne watki czekaja az skonczy sie czyszczenie
        pthread_barrier_wait(&barrier);
        for (size_t i = info->thread_id; i < dane_watku->size_test; i += NUM_THREADS) // watki skacza po zbiorze testowym
        {
            pthread_mutex_lock(&mutex);
            while (dane_watku->is_paused)         // jesli false to komenda pthread_cond_wait jest pomijana i watek kontynuuje dalej obliczenia
                pthread_cond_wait(&cond, &mutex); // uspienie watku w oczekiwaniu na sygnal i zwolnienie mutexa
            pthread_mutex_unlock(&mutex);         // po wybudzeniu program wykonuje sie od linijki pthread_cont_wait i nastepnie wraca na poczatek while i sprawdza stan zmiennej is_paused

            pthread_mutex_lock(&mutex);
            if (dane_watku->stop_request) // sprawdzamy czy mamy zakonczyc program
            {
                pthread_mutex_unlock(&mutex);
                goto end;
            }
            else if (dane_watku->reset)
            { // sprawdzamy czy mamy zrestowac program
                pthread_mutex_unlock(&mutex);
                goto handle_reset;
            }
            pthread_mutex_unlock(&mutex);

            double min_dist = __DBL_MAX__; // najwieksza mozliwa wartosc jaka moze przechowac typ double
            char type = ' ';
            for (size_t j = 0; j < dane_watku->size_train; j++)
            {
                double dx = dane_watku->wektor_train[j].x - dane_watku->wektor_test[i].x;
                double dy = dane_watku->wektor_train[j].y - dane_watku->wektor_test[i].y;
                double dz = dane_watku->wektor_train[j].z - dane_watku->wektor_test[i].z;

                double distance = dx * dx + dy * dy + dz * dz;
                if (distance < min_dist)
                {
                    min_dist = distance; // o wiele szybszy algorytm bez qsorta
                    type = dane_watku->wektor_train[j].type;
                }
            }

            pthread_mutex_lock(&mutex); // blokujemy zeby zwiekszyc dane bo watek p_thread moze akurat wtedy chciec odczytac
            if (type == dane_watku->wektor_test[i].type)
                local_hits++;
            else
                local_misses++;
            local_processed++;
            pthread_mutex_unlock(&mutex);
            if (local_processed % 20 == 0) // aaktualizujemy co 20
            {
                pthread_mutex_lock(&mutex);
                dane_watku->hits += local_hits;
                dane_watku->misses += local_misses;
                dane_watku->processed += local_processed;
                pthread_mutex_unlock(&mutex);
                local_hits = 0;
                local_misses = 0;
                local_processed = 0;
            }
        }
        // Aktualizujemy resztki statystyk, które nie załapały się na modulo 19
        pthread_mutex_lock(&mutex);
        dane_watku->hits += local_hits;
        dane_watku->misses += local_misses;
        dane_watku->processed += local_processed;
        pthread_mutex_unlock(&mutex);

        break; //break musi byc nad handle reset bo inaczej po skonczeniu obliczen petla while(1) zaczynalaby sie od nowa

    handle_reset:
        continue; // zacznamy petle while od nowa, czyli oblicznia rowniez zaczna sie od nowa
    }
end:
    int ret = pthread_barrier_wait(&barrier); // bariera po to zeby wszystkie konczyly w tym samym momencie
    if (ret == PTHREAD_BARRIER_SERIAL_THREAD) //tylko jeden watek wypisuje dane i ustawia flage
    {
        dane_watku->is_finished = true; // zmiana stanu obliczen na finished
        printf("\n--- OSTATECZNE DANE ---\n");
        printf("Postep:      %zu / %zu [%.1f%%]\n", dane_watku->processed, dane_watku->total, 100 * ((double)dane_watku->processed / dane_watku->total));
        printf("Trafienia:   %d\n", dane_watku->hits);
        printf("Pudla:       %d\n", dane_watku->misses);
        printf("Dokladnosc:  %.3f\n", (double)dane_watku->hits / (dane_watku->hits + dane_watku->misses));
        printf("-----------------------\n");
        printf("Stan obliczen: Zakonczony\n");
    }
    printf("[Work thread] Koncze dzialanie, ID: %d\n", info->thread_id);
    return NULL; // zeby nie bylo warninga
}

// watek obslugujacy wyswietlanie rezultatow
void *pause_thread(void *arg)
{
    DaneWatku *dane = (DaneWatku *)arg;
    struct pollfd pfd = {
        .fd = STDIN_FILENO, // monitorujemy STDIN, deskryptorem tego jest STDIN_FILENO
        .events = POLLIN    // POLLIN = dane do odczytu, monitorujemy czy sa dane do odczytu
    };

    while (1)
    {
        pthread_mutex_lock(&mutex);
        bool finished = dane->is_finished;
        pthread_mutex_unlock(&mutex);

        if (finished)
            break;
        // ret zwraca ile deskryptorow mialo pole revents ustalone na niezerowe (co oznacza ze nastapil error lub zdarzenie np. POLLIN), jesli zero to znaczy ze nic sie nie stalo, -1 oznacza error
        // drugi argument to 1 bo poll ma nasluchiwac tylko jednego gniazda (STDIN)
        int ret = poll(&pfd, 1, 100); // funkjca nieblokujaca czeka na dane przez 100ms jesli nie ma to petla wykonuje sie od nowa
        if (ret == 1 && (pfd.revents & POLLIN))
        { // revents zawiera flagi, jesli jest flaga POLLIN to znaczy ze jest cos do odczytania
            int c = getchar();
            if (c == '\n')
                continue;
            if (c == 'p' || c == 'P')
            {
                pthread_mutex_lock(&mutex);
                unsigned long h = dane->hits;
                unsigned long m = dane->misses;
                size_t state = dane->processed;
                size_t total = dane->total;
                pthread_mutex_unlock(&mutex);
                double percent = ((double)state / total) * 100;
                double ratio = (double)h / (h + m);
                printf("\n--- STATUS OBLICZEN ---\n");
                printf("Postep:      %zu / %zu [%.3lf]\n", state, total, percent);
                printf("Trafienia:   %lu\n", h);
                printf("Pudla:       %lu\n", m);
                printf("Dokladnosc:  %.3f\n", ratio);
                printf("-----------------------\n");
                if (dane->is_paused)
                    printf("Stan obliczen: Wstrzymany\n");
                else
                    printf("Stan obliczen: Uruchomiony\n");
            }
            else if (c == 'z' || c == 'Z')
            {
                printf("[Pause thread] Wcisnieto klawisz 'Z'\n");
                pthread_mutex_lock(&mutex);
                dane->stop_request = true;
                pthread_mutex_unlock(&mutex);
                break;
            }
            else if (c == 'w' || c == 'W')
            {
                pthread_mutex_lock(&mutex);
                dane->is_paused = !dane->is_paused;
                if (!dane->is_paused) // sprawdzamy czy watek nie jest wstrzymany
                {
                    pthread_cond_broadcast(&cond); // wybudzenie wszystkich watkow
                    printf("Obliczenia wznowione.\n");
                }
                else
                    printf("Obliczenia wstrzymane.\n");
                pthread_mutex_unlock(&mutex);
            }
            else if (c == 'r' || c == 'R')
            {
                pthread_mutex_lock(&mutex);
                dane->reset = true;
                if (dane->is_paused)
                { // jesli watek jest wstrzmany to zasygnalizuj mu zeby sie obudzil
                    dane->is_paused = false;
                    pthread_cond_broadcast(&cond);
                }
                pthread_mutex_unlock(&mutex);
                printf("Zresetowano obliczenia!\n");
            }
        }
    }
    printf("[Pause thread] Koncze dzialanie.\n");
    return NULL; // zeby nie bylo warninga
}

int main(int argc, char *argv[])
{
    Memory mem_train, mem_test;
    Wektor *train_data, *test_data;
    DaneWatku dane_watku;
    pthread_t work_thread[NUM_THREADS], p_thread;
    char wybor_uzytkownika;

    clock_t start,end;
    start = clock();

    train_data = wczyt(filename_train, &mem_train);
    test_data = wczyt(filename_test, &mem_test);
    size_t rozmiar[2] = {mem_train.rozmiar, mem_test.rozmiar};
    dane_watku.size_test = rozmiar[TEST_DATA];
    dane_watku.size_train = rozmiar[TRAIN_DATA];
    dane_watku.total = (double)rozmiar[TEST_DATA];
    dane_watku.wektor_test = test_data;
    dane_watku.wektor_train = train_data;
    dane_watku.stop_request = false;
    dane_watku.is_paused = false;
    dane_watku.reset = false;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    pthread_barrier_init(&barrier, NULL, NUM_THREADS);

    ThreadInfo thread_info[NUM_THREADS];

    printf("***********************************\n");
    printf("Witaj w progamie klasfyfikatora\n");
    printf("***********************************\n");
    printf("[K]Rozpocznij klasyfikacje:\n");
    printf("[P]Sprawdz stan obliczen:\n");
    printf("Wybor uzytkownika: ");
    while (1)
    {
        scanf("%c", &wybor_uzytkownika);
        getchar();
        if (wybor_uzytkownika == 'k' || wybor_uzytkownika == 'K')
            break;
        if (wybor_uzytkownika == 'p' || wybor_uzytkownika == 'P')
            printf("Stan obliczen: Bezczynny\n");
        printf("\n[K]Rozpocznij klasyfikacje:\n");
        printf("[P]Sprawdz stan obliczen:\n");
        printf("Wybor uzytkownika: ");
    }

    printf("***Dodatkowe opcje***\n");
    printf("[P]Wyswielt rezultaty\n");
    printf("[W]Wstrzymaj/wznow dzialanie\n");
    printf("[R]Reset. Zacznij oblczanie od poczatku\n");
    printf("[Z]Zakoncz analize\n");
    for (int i = 0; i < NUM_THREADS; i++)
    {
        thread_info[i].thread_id = i;
        thread_info[i].data = &dane_watku;
        if (pthread_create(&work_thread[i], NULL, calc_thread, (void *)&thread_info[i]) != 0)
        {
            printf("Error creating thread\n");
            return 1;
        }
    }
    if (pthread_create(&p_thread, NULL, pause_thread, (void *)&dane_watku) != 0)
    {
        printf("Error creating thread\n");
        return 1;
    }
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(work_thread[i], NULL);
    pthread_join(p_thread, NULL);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    pthread_barrier_destroy(&barrier);

    free(train_data);
    free(test_data);
    end = clock();
    double time = ((double)(end - start))/NUM_THREADS; //dzielimy przez liczbe watkow aby wynik byl prawidlowy bo inaczej funnkcja clock() zsumowala by czas ile dzialal jeden watek i dodala do wszystkich
    double time_taken = time/CLOCKS_PER_SEC;
    printf("[main]Czas wykonania programu wynosi: %lf\n", time_taken);
    printf("[main]Koncze dzialanie\n");
    return 0;
}