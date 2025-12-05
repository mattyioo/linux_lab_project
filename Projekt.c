#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h> //funkcja poll
#include <pthread.h>
#include <unistd.h>    //STDIN_FILENO
#include <stdatomic.h> //dla operacji atomicznych

#define BUF_SIZE 256
#define TRAIN_DATA 0
#define TEST_DATA 1

// definiowanie typow
#define WEKTOR_TRAIN 2
#define WEKTOR_TEST 3

#define NUM_THREADS 5

char buffer[BUF_SIZE];

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
    double odleglosc;
    char type;
} Dystans;

typedef struct
{
    const Wektor *wektor_train;
    const Wektor *wektor_test;
    size_t size_test;
    size_t size_train;
    unsigned long hits;
    unsigned long misses;
    size_t processed;
    size_t total;
    size_t num_of_threads;
    size_t start_index;
    pthread_mutex_t mutex;
    bool is_finished;  // sprawdzamy czy watek sie zakonczyl
    bool stop_request; // sprawdzamy czy wcisnieto przycisk Z
    bool is_paused;    // sprawdzamy czy watek jest wstrzymany
    bool reset;        // sprawdzamy czy nastapil reset
    pthread_cond_t cond;
} DaneWatku;

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

// funkcja pomocnicza
// void print_danych(const void *ptr, uint8_t typ, size_t rozmiar) // const bo tylko odczytujemy dane
// {
//     Wektor *dane = (Wektor *)ptr;
//     printf("Dane %s:\n", typ == WEKTOR_TRAIN ? "treningowe" : "testowe");
//     for (size_t i = 0; i < rozmiar; i++)
//         printf("Dane [%zu]: x: %.1lf, y:  %.1lf, z: %.1lf, type: %c\n", i + 1, dane[i].x, dane[i].y, dane[i].z, dane[i].type);
// }

// funkcja porównująca do qsort (sortowanie rosnące)
int compar(const void *a, const void *b)
{
    const Dystans *d1 = (Dystans *)a;
    const Dystans *d2 = (Dystans *)b;

    if (d1->odleglosc > d2->odleglosc)
        return 1;
    if (d1->odleglosc < d2->odleglosc)
        return -1;

    return 0;
}
// obliczanie odleglosci euklidesowej + obliczanie procentu trafien w zbiorze
void ratio_distance(const Wektor *train_data, const Wektor *test_data, DaneWatku *dane_watku, size_t rozmiar_test_data, size_t rozmiar_train_data) // rozmiar test data napewno jest wiekszy od test data
{                                                                                                                                                  // nie chcemy modyfikowac struktur dlatego const
    dane_watku->is_finished = false;
    Dystans *distance = (Dystans *)malloc(rozmiar_train_data * sizeof(Dystans)); // alokujemy tablice struktur dystans
    if (distance == NULL)
    {
        printf("Blad alokacji pamieci!\n");
        exit(1);
    }
    while (1)
    {
        dane_watku->hits = 0;
        dane_watku->misses = 0;
        dane_watku->processed = 0;
        for (size_t i = 0; i < rozmiar_test_data; i++)
        {
            pthread_mutex_lock(&dane_watku->mutex);
            while (dane_watku->is_paused)                                 // jesli false to komenda pthread_cond_wait jest pomijana i watek kontynuuje dalej obliczenia
                pthread_cond_wait(&dane_watku->cond, &dane_watku->mutex); // uspienie watku w oczekiwaniu na sygnal i zwolnienie mutexa
            pthread_mutex_unlock(&dane_watku->mutex);                     // po wybudzeniu program wykonuje sie od linijki pthread_cont_wait i nastepnie wraca na poczatek while i sprawdza stan zmiennej is_paused

            // if(atomic_load(&dane_watku->stop_request)) //pobierz i sprawdz aktualna wartosc z pamieci
            // break; //wyjdz z petli i zakoncz dzialanie watku
            pthread_mutex_lock(&dane_watku->mutex);
            if (dane_watku->stop_request) // sprawdzamy czy mamy zakonczyc program
            {
                pthread_mutex_unlock(&dane_watku->mutex);
                break;
            }
            else if (dane_watku->reset)
            { // sprawdzamy czy mamy zrestowac program
                pthread_mutex_unlock(&dane_watku->mutex);
                break;
            }
            pthread_mutex_unlock(&dane_watku->mutex);
        
            for (size_t j = dane_watku->start_index; j < rozmiar_train_data; j+=dane_watku->num_of_threads)
            {
                double dx = train_data[j].x - test_data[i].x;
                double dy = train_data[j].y - test_data[i].y;
                double dz = train_data[j].z - test_data[i].z;

                distance[j].odleglosc = dx * dx + dy * dy + dz * dz; // obliczanie odległości jednego punktu testowego od każdego punktu treningowego
                distance[j].type = train_data[j].type;                     // zapisujemy typ kazdego obliczonego wektora treningowego
            }
            qsort(distance, rozmiar_test_data, sizeof(Dystans), compar); // sortowanie rosnące
            // najblizszy sasiad w takim razie to bedzie tablica z indeksem 0
            pthread_mutex_lock(&dane_watku->mutex); // blokujemy zeby zwiekszyc dane bo watek p_thread moze akurat wtedy chciec odczytac
            if (distance[0].type == test_data[i].type)
                dane_watku->hits++;
            else
                dane_watku->misses++;
            dane_watku->processed++;
            pthread_mutex_unlock(&dane_watku->mutex);
        }
        pthread_mutex_lock(&dane_watku->mutex);
        if (dane_watku->reset)
        {
            dane_watku->reset = false;
            pthread_mutex_unlock(&dane_watku->mutex);
            continue; // zacznamy petle while od nowa, czyli oblicznia rowniez zaczna sie od nowa
        }
        pthread_mutex_unlock(&dane_watku->mutex);
        break;
    }
    free(distance);
    pthread_mutex_lock(&dane_watku->mutex);
    dane_watku->is_finished = true; // zmiana stanu obliczen na finished
    pthread_mutex_unlock(&dane_watku->mutex);
    if (dane_watku->is_finished)
    {
        printf("\n--- OSTATECZNE DANE ---\n");
        printf("Postep:      %zu / %zu [%.1f%%]\n", dane_watku->processed, dane_watku->total, 100 * ((double)dane_watku->processed / dane_watku->total));
        printf("Trafienia:   %lu\n", dane_watku->hits);
        printf("Pudla:       %lu\n", dane_watku->misses);
        printf("Dokladnosc:  %.3f\n", (double)dane_watku->hits / (dane_watku->hits + dane_watku->misses));
        printf("-----------------------\n");
        printf("Stan obliczen: Zakonczony\n");
    }
}

// watek do przeprowadznia obliczen
void *calc_thread(void *arg)
{
    DaneWatku *dane_thread = (DaneWatku *)arg;
    ratio_distance(dane_thread->wektor_train, dane_thread->wektor_test, dane_thread, dane_thread->size_test, dane_thread->size_train);
    printf("[Work thread] Koncze dzialanie\n");
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
        pthread_mutex_lock(&dane->mutex);
        bool finished = dane->is_finished;
        pthread_mutex_unlock(&dane->mutex);

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
                pthread_mutex_lock(&dane->mutex);
                unsigned long h = dane->hits;
                unsigned long m = dane->misses;
                size_t state = dane->processed;
                size_t total = dane->total;
                pthread_mutex_unlock(&dane->mutex);
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
                pthread_mutex_lock(&dane->mutex);
                dane->stop_request = true;
                pthread_mutex_unlock(&dane->mutex);
                // atomic_store(&dane->stop_request, true); //zapisz taka wartosc do pamieci
                break;
            }
            else if (c == 'w' || c == 'W')
            {
                pthread_mutex_lock(&dane->mutex);
                dane->is_paused = !dane->is_paused;
                if (!dane->is_paused) // sprawdzamy czy watek nie jest wstrzymany
                {
                    pthread_cond_signal(&dane->cond); // wybudzenie watku drugiego jesli watek nie jest wstrzymany
                    printf("Obliczenia wznowione.\n");
                }
                else
                    printf("Obliczenia wstrzymane.\n");
                pthread_mutex_unlock(&dane->mutex);
            }
            else if (c == 'r' || c == 'R')
            {
                pthread_mutex_lock(&dane->mutex);
                dane->reset = true;
                if (dane->is_paused)
                { // jesli watek jest wstrzmany to zasygnalizuj mu zeby sie obudzil
                    dane->is_paused = false;
                    pthread_cond_signal(&dane->cond);
                }
                pthread_mutex_unlock(&dane->mutex);
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

    train_data = wczyt(filename_train, &mem_train);
    test_data = wczyt(filename_test, &mem_test);
    size_t rozmiar[2] = {mem_train.rozmiar, mem_test.rozmiar};
    dane_watku.size_test = rozmiar[TEST_DATA];
    dane_watku.size_train = rozmiar[TRAIN_DATA];
    dane_watku.total = (double)rozmiar[TEST_DATA];
    dane_watku.wektor_test = test_data;
    dane_watku.wektor_train = train_data;
    dane_watku.num_of_threads = NUM_THREADS;
    // atomic_init(&dane_watku.stop_request, false);
    dane_watku.stop_request = false;
    dane_watku.is_paused = false;
    dane_watku.reset = false;
    pthread_mutex_init(&dane_watku.mutex, NULL);
    pthread_cond_init(&dane_watku.cond, NULL);

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
    for(int i=0; i<5; i++){
    dane_watku.start_index = i;
    if (pthread_create(&work_thread[i], NULL, calc_thread, (void *)&dane_watku) != 0)
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
    for(int i=0; i<5; i++)
        pthread_join(work_thread[i], NULL);
    pthread_join(p_thread, NULL);

    pthread_mutex_destroy(&dane_watku.mutex);
    pthread_cond_destroy(&dane_watku.cond);

    free(train_data);
    free(test_data);
    printf("[main]Koncze dzialanie\n");
    return 0;
}