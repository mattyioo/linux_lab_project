#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define BUF_SIZE 256
#define TRAIN_DATA 0
#define TEST_DATA 1

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
    return mem->wektor;
}

void print_danych(const Wektor *dane, uint32_t linie, bool iftrain_data) // const bo tylko odczytujemy dane
{
    printf("Dane %s:\n", iftrain_data ? "treningowe" : "testowe");
    for (uint32_t i = 0; i < linie - 1; i++)
    {
        printf("Dane %s [%d]: x: %lf, y:  %lf, z: %lf, type: %c\n", iftrain_data ? "treningowe" : "testowe", i + 1, dane[i].x, dane[i].y, dane[i].z, dane[i].type);
    }
}

// obliczanie odleglosci euklidesowej
double *distance(const Wektor *train_data, const Wektor *test_data, uint32_t linie_test_data)
{ // nie chcemy modyfikowac struktur dlatego const
    double *distance = (double *)malloc(linie_test_data * sizeof(double));
    if (distance == NULL)
    {
        printf("Blad alokacji pamieci!\n");
        exit(1);
    }
    for (uint32_t i = 0; i < linie_test_data - 1; i++)
    {
        distance[i] = sqrt(pow((train_data[i].x - test_data[i].x), 2) + pow((train_data[i].y - test_data[i].y), 2) + pow((train_data[i].z - test_data[i].z), 2));
    }
    return distance; // zwracamy wskaznik ktory wskazuje na zaalokowana pamiec
}

int main(int argc, char *argv[])
{
    Memory mem_train, mem_test;
    Wektor *train_data, *test_data;
    train_data = wczyt(filename_train, &mem_train);
    test_data = wczyt(filename_test, &mem_test);
    uint32_t linie[2] = {mem_train.rozmiar, mem_test.rozmiar};
    printf("Dane treningowe mają %zu elementów.\n", linie[TRAIN_DATA]);
    printf("Dane testowe mają %zu elementów.\n", linie[TEST_DATA]);

    free(train_data);
    free(test_data);

    return 0;
}