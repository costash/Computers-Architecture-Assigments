#
# Author: Constantin Șerban-Rădoi 333CA
# Tema 2 ASC
# Aprilie 2013
#
** Fișiere:
	- main.c - Fișier sursă ce conține funcția main și implementarea temei
	- mmio.h - Fișier header pentru citire matrice Matrix-Market format
	- mmio.c - Fișier sursă pentru citire matrice Matrix-Market format
	- Makefile - makefile care primește ca variabile modulul încărcat de
		scriptul din care a fost apelat și parametrii care trebuie dați la
		rularea programului (make run)
	- run.sh - script pentru submis joburile pe cozi
	- exec_script.sh - fișierul executat pe noduri
	- *.mtx - Fișiere de test ce conțin matrice în format Matrix-Market
		Matrice folosite de pe matrix-market:
			bcsstk01.mtx  bcsstk17.mtx  bcsstk18.mtx  bcsstk25.mtx
			e40r0000.mtx  s3rmt3m3.mtx
	- plots/*.png - Fișiere ce conțin plot-uri realizate cu gnuplot
	- plots/*.txt - Fișiere ce conțin datele pentru crearea plot-urilor în
		format gnuplot-friendly
	- README.txt - Acest fișier

** Implementare:
	Varianta brută (de mână) parcurge efectiv toată matricea cu 2 for-uri și
calculează produsul. Acest lucru este neoptim datorită faptului că se fac prea
multe înmulțiri inutile.
	Varianta optimă (de mână), parcurge doar jumătatea superioară a matricei,
și se parcurge folosind *doar* pointeri, în loc de indecsi. Am parcurs și
vectorii în aceeași manieră, pentru a evita overhead-ul indexării. De asemenea,
se calculează o singură dată produsul alpha * x[j], în loc să se calculeze de
SIZE ori. Acumulatorul în care se adună A[i][j] este salvat ca registru, pentru
a fi mai rapid.
	Varianta BLAS doar apelează funcția cblas_dsymv(), cu parametri specifici.

** Testare:
	Pentru a testa corectitudinea rezultatelor am luat drept referință valorile
întoarse de funcția cblas_dsymv. Compararea am făcut-o cu precizie de până la
9 cifre semnificative, deoarece varianta brută acumulează prea multe erori din
adunări și înmulțiri pe double.

	Am folosit mai multe fișiere de test pentru matrice, de diferite dimensiuni.
Primele teste folosesc matrice cu dimensiuni de până la 17000 de pe Matrix
Market. Acestea sunt matrice rare, cu relativ puține valori. Un alt doilea set
de teste cuprinde matrice dense, generate cu rand(), cu valori de la 15000 până
la 40000.

** Comparație implementare Brută, neoptimizată versus implementare optimizată,
ambele fără flag-uri de optimizare la compilator:
	În medie, pe toate platformele, varianta brută este de aproximativ 2 ori mai
lentă decât cea optimizată. După cum se observă din grafice, la dimensiuni mari,
aceste diferențe se accentuează.
	Pe Quad respectiv Opteron, pentru matrice densă de 40000 random bruta
neoptimizată obține în jur de 19 secunde, iar pe Nehalem în jur de 13 secunde.
Varianta optimizată (fără flag-uri) obține pe Nehalem și Opteron timpi în jur de
5.5-6 secunde, iar pe Quad 11 secunde.

** Comparație între implementarea Brută, fără flag-uri de optimizare și varianta
optimizată, cu flag-uri de optimizare la compilator:
	În medie, pe toate platformele, varianta brută, fără flag-uri este cu până
la 10 ori mai lentă decât cea optimizată cu flaguri de optimizare la compilator.
		La dimensiuni mari (40k) se observă diferențe precum:
			19s versus 2.2s pe Opteron și Quad
			13.5s versus 1.5s pe Nehalem

** Comparație între implementarea optimizată, și varianta din BLAS, ambele cu
flag-uri de optimizare la compilator:
	Timpi obținuți pe aceeași matrice de 40k:
	Nehalem:
		940ms versus 1426ms
	Opteron:
		2411ms versus 2528ms
	Quad:
		2482ms versus 2524ms

** Interpretări
	Este evident că pe cozile Quad, respectiv Opteron timpii de rulare vor fi
ceva mai mari, datorită arhitecturilor ceva mai vechi decât pe Nehalem, care
beneficiază de frecvențe mai mari, cache mai mult, etc.
	Tot din grafice, se poate observa că varianta brută, la care se folosesc
flag-uri de optimizare la compilator, se apropie destul de mult ca performanțe
de varianta optimizată de mână, tot cu flag-uri de compilare.
	Optimizata cu toate flag-urile activate are rezultate foarte foarte
apropiate de cele obținute cu BLAS, pe Quad și Opteron, iar pe Nehalem se
apropie până la 80-90% din performanțele BLAS.

	De asemeni, se poate observa și dependența quadratică de timp a
implementărilor, cu constante mai mici pentru implementările BLAS respectiv
cele optimizate.