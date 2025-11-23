#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define BUF_SIZE 256
#define TRAIN_DATA 0
#define TEST_DATA 1

// definiowanie typow
#define WEKTOR_TRAIN 2
#define WEKTOR_TEST 3

char buffer[BUF_SIZE];

const char *filename_train = "train_100linijek.txt";
const char *filename_test = "test100_linijek.txt";

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
void print_danych(const void *ptr, uint8_t typ, size_t rozmiar) // const bo tylko odczytujemy dane
{
    Wektor *dane = (Wektor *)ptr;
    printf("Dane %s:\n", typ == WEKTOR_TRAIN ? "treningowe" : "testowe");
    for (size_t i = 0; i < rozmiar; i++)
        printf("Dane [%zu]: x: %.1lf, y:  %.1lf, z: %.1lf, type: %c\n", i + 1, dane[i].x, dane[i].y, dane[i].z, dane[i].type);
}


// funkcja porównująca do qsort
int compar(const void *a, const void *b)
{
    const double *d1 = (double *)a;
    const double *d2 = (double *)b;

    if (*d1 > *d2)
        return 1;
    if (*d1 < *d2)
        return -1;

    return 0;
}
// obliczanie odleglosci euklidesowej + obliczanie procentu trafien w zbiorze
double ratio_distance(const Wektor *train_data, const Wektor *test_data, size_t rozmiar_test_data) // rozmiar test data napewno jest wiekszy od test data
{                                                                                                  // nie chcemy modyfikowac struktur dlatego const
    int hits = 0, misses = 0;
    Dystans *distance = malloc(rozmiar_test_data * sizeof(Dystans)); // alokujemy tablice struktur dystans
    if (distance == NULL)
    {
        printf("Blad alokacji pamieci!\n");
        exit(1);
    }
    for (size_t i = 0; i < rozmiar_test_data; i++)
    {
        for (size_t j = 0; j < rozmiar_test_data; j++)
        {
            double dx = train_data[j].x - test_data[i].x;
            double dy = train_data[j].y - test_data[i].y;
            double dz = train_data[j].z - test_data[i].z;

            distance[j].odleglosc = sqrt(dx * dx + dy * dy + dz * dz); // obliczanie odległości jednego punktu testowego od każdego punktu treningowego
            distance[j].type = train_data[j].type;                     // zapisujemy typ kazdego obliczonego wektora treningowego
        }
        qsort(distance, rozmiar_test_data, sizeof(Dystans), compar); // sortowanie rosnące
        // najblizszy sasiad w takim razie to bedzie tablica z indeksem 0
        if (distance[0].type == test_data[i].type)
            hits++;
        else
            misses++;
    }
    double hits_to_misses_ratio = (double)hits / (hits + misses);
    return hits_to_misses_ratio; // zwracamy procent trafien w naszym zbiorze
}



int main(int argc, char *argv[])
{
    Memory mem_train, mem_test;
    Wektor *train_data, *test_data;

    train_data = wczyt(filename_train, &mem_train);
    test_data = wczyt(filename_test, &mem_test);
    size_t rozmiar[2] = {mem_train.rozmiar, mem_test.rozmiar};


    printf("Dane treningowe mają %zu elementów.\n", rozmiar[TRAIN_DATA]);
    printf("Dane testowe mają %zu elementów.\n", rozmiar[TEST_DATA]);
    printf("TRAIN: %p\nTEST: %p\nCapacity:%zu test: %zu", mem_train.wektor, mem_test.wektor, mem_train.capacity, mem_test.capacity);
    printf("Dane treningowe:\n");
    print_danych(train_data, WEKTOR_TRAIN, rozmiar[TRAIN_DATA]);
    printf("Dane testowe:\n");
    print_danych(test_data, WEKTOR_TEST, rozmiar[TEST_DATA]);

    printf("Hits to misses ratio: %.3lf", ratio_distance(train_data, test_data, rozmiar[TEST_DATA]));

    free(train_data);
    free(test_data);

    return 0;
}