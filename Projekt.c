#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BUF_SIZE 256

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
    return dane; // zwracamy wskaxnik do zaalokowanej pamieci
}

void wczytywanie_danych(const char *filename, Wektor *dane, uint32_t linie)
{
    FILE *pf = fopen(filename, "r");
    if (pf == NULL)
    {
        printf("Error opening file\n");
        exit(1);
    }
    if(fgets(buffer, BUF_SIZE, pf) == NULL){
        printf("Blad odczytu pierwszej linii z pliku %s\n", filename);
        exit(1);
    }

    for (uint32_t i = 0; i < linie-1; i++)
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

void print_danych(const Wektor* dane, uint32_t linie, bool iftrain){
    printf("Dane %s\n", iftrain ? "treningowe" : "testowe");
    for(uint32_t i = 0; i<linie-1; i++){
        printf("Dane %s [%d]: x: %lf, y:  %lf, z: %lf, type: %c\n", iftrain ? "treningowe" : "testowe",i+1, dane[i].x, dane[i].y, dane[i].z, dane[i].type);
    }
}

int main(int argc, char *argv[])
{

    uint32_t linie[2] = {licznik_linii(filename_train), licznik_linii(filename_test)};
    Wektor *train_data, *test_data;
    
    train_data = memory_allocation(linie[0]);
    test_data = memory_allocation(linie[1]);
    
    printf("Liczba linii w pliku treningowym: %u\n", linie[0]);
    printf("Liczba linii w pliku testowym: %u\n", linie[1]);
    
    wczytywanie_danych(filename_train, train_data, linie[0]); 
    wczytywanie_danych(filename_test, test_data, linie[1]);
    
    // printf("Dane treningowe:\n");
    // for (int i = 0; i < linie[0]-1; i++) //minus 1 bo pierwsza linie pomijamy
    //     printf("Dane treningowe[%d]: x: %lf, y:  %lf, z: %lf, type: %c\n",i+1, train_data[i].x, train_data[i].y, train_data[i].z, train_data[i].type);

    
    // printf("Dane testowe:\n");
    // for(int i = 0; i < linie[1]-1; i++)
    //     printf("Dane testowe[%d]: x: %lf, y:  %lf, z: %lf, type: %c\n",i+1, test_data[i].x, test_data[i].y, test_data[i].z, test_data[i].type);
    print_danych(train_data, linie[0], true);
    print_danych(test_data, linie[1], false);

    free(train_data);
    free(test_data);
    return 0;
}