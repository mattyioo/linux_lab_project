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
#define DOUBLE 2
#define WEKTOR 3

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
    return mem->wektor; //zwracamy wskaznik na zaalokowana pamiec typu Wektor *
}

// funkcja pomocnicza
void print_danych(const void *ptr, uint8_t typ, size_t rozmiar) // const bo tylko odczytujemy dane
{
    if (typ == WEKTOR)
    {
        Wektor *dane = (Wektor *)ptr;
        for (size_t i = 0; i < rozmiar; i++)
            printf("Dane [%zu]: x: %.1lf, y:  %.1lf, z: %.1lf, type: %c\n", i + 1, dane[i].x, dane[i].y, dane[i].z, dane[i].type);
    }
    else if (typ == DOUBLE)
    {
        double *dystans = (double *)ptr;
        printf("Dystanse:\n");
        for (size_t i = 0; i < rozmiar; i++)
            printf("Odleglosc od elemntu[%zu] to : %.3lf\n", i + 1, dystans[i]);
    }
}
// obliczanie odleglosci euklidesowej
double *distance(const Wektor *train_data, const Wektor *test_data, size_t rozmiar_test_data) // rozmiar test data napewno jest wiekszy od test data
{                                                                                             // nie chcemy modyfikowac struktur dlatego const
    double *distance = (double *)malloc(rozmiar_test_data * sizeof(double));
    if (distance == NULL)
    {
        printf("Blad alokacji pamieci!\n");
        exit(1);
    }
    for (size_t i = 0; i < rozmiar_test_data; i++)
    {
        double dx = train_data[i].x - test_data[i].x;
        double dy = train_data[i].z - test_data[i].y;
        double dz = train_data[i].z - test_data[i].z;
        distance[i] = sqrt(dx*dx + dy*dy + dz*dz);
    }
    return distance; // zwracamy wskaznik ktory wskazuje na zaalokowana pamiec
}

int main(int argc, char *argv[])
{
    Memory mem_train, mem_test;
    Wektor *train_data, *test_data;

    train_data = wczyt(filename_train, &mem_train);
    test_data = wczyt(filename_test, &mem_test);
    size_t rozmiar[2] = {mem_train.rozmiar, mem_test.rozmiar};
    double *dystans = distance(train_data, test_data, rozmiar[TEST_DATA]);

    printf("Dane treningowe mają %zu elementów.\n", rozmiar[TRAIN_DATA]);
    printf("Dane testowe mają %zu elementów.\n", rozmiar[TEST_DATA]);
    printf("TRAIN: %p\nTEST: %p\nCapacity:%zu test: %zu", mem_train.wektor, mem_test.wektor, mem_train.capacity, mem_test.capacity);
    printf("Dane testowe:\n");
    print_danych(train_data, WEKTOR, rozmiar[TRAIN_DATA]);
    printf("Dane testowe:\n");
    print_danych(test_data, WEKTOR, rozmiar[TEST_DATA]);
    print_danych(dystans, DOUBLE, rozmiar[TEST_DATA]);

    free(train_data);
    free(test_data);

    return 0;
}