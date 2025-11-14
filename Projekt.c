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

uint32_t licznik_linii(const char *filename)
{
    FILE *pf = fopen(filename, "r"); // read only mode
    if (pf == NULL)
    {
        printf("Error opening file\n");
        return 0;
    }
    uint32_t licznik = 0;
    while (fgets(buffer, BUF_SIZE, pf) != NULL)
    {
        licznik++;
    }
    fclose(pf);
    return licznik;
}

Wektor *memory_allocation(uint32_t linie)
{
    Wektor *dane = (Wektor *)malloc(linie * sizeof(Wektor));
    if (dane == NULL)
    {
        printf("Blad alokacji pamieci!\n");
        exit(1);
    }
    printf("Pamiec pomyslnie zaalokowana dla %u elementow\n", linie);
    printf("Wskaznik na pamiec o adresie %p\n", dane);
    return dane; // zwracamy wskaźnik do zaalokowanej pamieci
}


void wczytywanie_danych(const char *filename, Wektor *dane, uint32_t linie)
{
    FILE *pf = fopen(filename, "r");
    if (pf == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }
    if (fgets(buffer, BUF_SIZE, pf) == NULL)
    {
        printf("Blad odczytu pierwszej linii z pliku %s\n", filename);
        exit(1);
    }

    for (uint32_t i = 0; i < linie - 1; i++)
    {
        fgets(buffer, BUF_SIZE, pf);
        if (sscanf(buffer, "%lf,%lf,%lf,%c", &dane[i].x, &dane[i].y, &dane[i].z, &dane[i].type) != 4)
        {
            printf("Wczytano mniej niz 4 zmienne w linii %u\n", i + 1);
            exit(2);
        }
    }
    fclose(pf);
}

void print_danych(const Wektor *dane, uint32_t linie, bool iftrain_data)
{
    printf("Dane %s:\n", iftrain_data ? "treningowe" : "testowe");
    for (uint32_t i = 0; i < linie - 1; i++)
    {
        printf("Dane %s [%d]: x: %lf, y:  %lf, z: %lf, type: %c\n", iftrain_data ? "treningowe" : "testowe", i + 1, dane[i].x, dane[i].y, dane[i].z, dane[i].type);
    }
}

//obliczanie odleglosci euklidesowej
double* distance(const Wektor *train_data, const Wektor *test_data, uint32_t linie_test_data){
    double *distance = (double *)malloc(linie_test_data * sizeof(double));
    if(distance == NULL){
        printf("Blad alokacji pamieci!\n");
        exit(1);
    }
    for(uint32_t i = 0; i < linie_test_data - 1; i++){
        distance[i] = sqrt(pow((train_data[i].x - test_data[i].x), 2) + pow((train_data[i].y - test_data[i].y), 2) + pow((train_data[i].z - test_data[i].z), 2));
    }
    return distance; //zwracamy wskaznik ktory wskazuje na zaalokowana pamiec
}

int main(int argc, char *argv[])
{

    uint32_t linie[2] = {licznik_linii(filename_train), licznik_linii(filename_test)};
    Wektor *train_data, *test_data;

    train_data = memory_allocation(linie[TRAIN_DATA]);
    test_data = memory_allocation(linie[TEST_DATA]);

    printf("Liczba linii w pliku treningowym: %u\n", linie[TRAIN_DATA]);
    printf("Liczba linii w pliku testowym: %u\n", linie[TEST_DATA]);

    wczytywanie_danych(filename_train, train_data, linie[TRAIN_DATA]);
    wczytywanie_danych(filename_test, test_data, linie[TEST_DATA]);

    print_danych(train_data, linie[0], true);
    print_danych(test_data, linie[1], false);
    
    double *dystans = distance(train_data, test_data, linie[TEST_DATA]);
    printf("Odlegość euklidesowa dla wektorów TEST_DATA\n");
    for(uint32_t i = 0; i < linie[TEST_DATA] - 1; i++){
        printf("Odleglość dla elementu [%u] to: %lf\n", i+1, dystans[i]);
    }

    free(train_data);
    free(test_data);
    return 0;
}