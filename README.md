# MPI_War
[Study Project]

 W odległej przyszłości wybucha wojna federacji terrańskiej z puchatymi misiami, których słitaśny wygląd kompletnie nie pasuje do ich mrocznej i sadystycznej natury. Wojna ma charakter podjazdowy, federacja wysyła pojedyncze okręty do walki, po których wracają zniszczone w różnym stopniu.

N okrętów co pewien czas kończy walkę i wraca do bazy, gdzie ubiega się o jeden z K doków. Dodatkowo, dowództwo okrętów ubiega się o mechaników. Liczba mechaników wymaganych do naprawy jest zależna od zniszczeń. Łącznie jest M mechaników, i są oni nierozróżnialni. 

1. ssh -l inf1172XX polluks.cs.put.poznan.pl
2. Your lab password
3. ssh lab-os-1
4. git clone https://github.com/beszczur/MPI_War OR git pull origin master
5. cd MPI_War
6. mpicc war.c -o war
7. mpirun -machinefile machinefile -np 10 war 10 4 1 > plik
8. sort -k2 -n plik > plik1
9. grep InDok plik1 > plik2
10. vi plik2
11. Have a lot of fun :)
