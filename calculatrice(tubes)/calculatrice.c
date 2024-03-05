#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHK(op)                                                                \
    do {                                                                       \
        if ((op) == -1)                                                        \
            raler(1, #op);                                                     \
    } while (0)

noreturn void raler(int syserr, const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    if (syserr == 1)
        perror("");

    exit(EXIT_FAILURE);
}

// Fonction qui fait ce qui doit être fait dans chaque fils,
// sachant qu'ils font tous les 4 la même chose mais avec
// un opérateur et un tube différent
void sonprocess(int *pipe, int *resultpipe, char operator)
{
    int operand1, operand2;
    int r;
    CHK(close(resultpipe[0])); // On ne lit pas dans ce tube
    CHK(close(pipe[1]));       // On n'écrit pas dans ce tube

    // On lit la première opérande depuis le tube
    while ((r = read(pipe[0], &operand2, sizeof(int))) > 0) {
        // On lit la deuxième opérande depuis le tube
        r = read(pipe[0], &operand1, sizeof(int));
        if (r == -1)
            raler(1, "read operand 1");
        int result;
        switch (operator) {
        case '+':
            result = operand1 + operand2;
            break;
        case '-':
            result = operand1 - operand2;
            break;
        case '/':
            if (operand2 == 0)
                raler(0, "Division par zéro");
            result = operand1 / operand2;
            break;
        case '*':
            result = operand1 * operand2;
            break;
        default:
            raler(0, "Opérateur inconnu");
        }
        // On écrit le résultat dans le tube des résultats
        write(resultpipe[1], &result, sizeof(int));
    }
    if (r == -1)
        raler(1, "read operand 2");
    CHK(close(pipe[0]));
}

// Fonction qui écrit les opérandes dans le tube correspondant
void wrOperandsInPipe(int *pipe, int operand1, int operand2)
{
    CHK(write(pipe[1], &operand2, sizeof(int)));
    CHK(write(pipe[1], &operand1, sizeof(int)));
}

// Fonction qui wait n processus fils et vérifie qu'ils se sont bien terminés
void waitson(int n)
{
    int status;
    for (int i = 0; i < n; i++) {
        CHK(wait(&status));
        // On check si le fils s'est terminé normalement
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            raler(1, "child pb");
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc % 2 != 0)
        raler(0, "usage: %s nb [op. nb1 op. nb2 ...]", argv[0]);

    // Note : J'ai choisi de donner un nom pour chaque tube plutôt que de faire
    // un tableau de 5 tubes dans un souci de lisibilité et de compréhension du
    // code, bien que cela rajoute des lignes (~80) et empêche de faire une
    // boucle pour la création des tubes et leur fermeture, la création des fils
    // etc.

    // Création du pipe pour récupérer les résultats :
    int respipe[2];
    CHK(pipe(respipe));

    // Tube & processus addition
    int addpipe[2];
    CHK(pipe(addpipe));
    switch (fork()) {
    case -1:
        raler(1, "fork");
    case 0:
        sleep(2);
        sonprocess(addpipe, respipe, '+');
        exit(EXIT_SUCCESS);
    default:
        break;
    }

    // Tube & processus soustraction
    int subpipe[2];
    CHK(pipe(subpipe));

    switch (fork()) {
    case -1:
        raler(1, "fork");
    case 0:
        sleep(2);
        sonprocess(subpipe, respipe, '-');
        exit(EXIT_SUCCESS);
    default:
        break;
    }

    // Tube & processus division
    int divpipe[2];
    CHK(pipe(divpipe));

    switch (fork()) {
    case -1:
        raler(1, "fork");
    case 0:
        sleep(2);
        sonprocess(divpipe, respipe, '/');
        exit(EXIT_SUCCESS);
    default:
        break;
    }

    // Tube & processus multiplication
    int mulpipe[2];
    CHK(pipe(mulpipe));

    switch (fork()) {
    case -1:
        raler(1, "fork");
    case 0:
        sleep(2);
        sonprocess(mulpipe, respipe, '*');
        exit(EXIT_SUCCESS);
    default:
        break;
    }

    // On va maintenant lire les opérateurs et écrire les opérandes dans les
    // tubes correspondants

    // Position de l'argument actuellement traité :
    int current_arg = 1;

    // On met le premier nombre dans le tube des résultats pour avoir seulement
    // à lire l'opérateur et le nombre suivant (dans un but de récursivité dans
    // les itérations futures)
    int first_number = atoi(argv[current_arg++]);
    CHK(write(respipe[1], &first_number, sizeof(int)));

    while (argv[current_arg] != NULL) {

        // On lit le nombre suivant (opérande 2):
        int next_number = atoi(argv[current_arg + 1]);

        // On lit le nombre qui est dans le tube résultat (opérande 1)
        int cur_number;
        CHK(read(respipe[0], &cur_number, sizeof(int)));

        // On écrit les opérandes dans le tube correspondant :
        switch (argv[current_arg][0]) {
        case '+':
            wrOperandsInPipe(addpipe, cur_number, next_number);
            break;
        case '-':
            wrOperandsInPipe(subpipe, cur_number, next_number);
            break;
        case '/':
            wrOperandsInPipe(divpipe, cur_number, next_number);
            break;
        case '*':
            wrOperandsInPipe(mulpipe, cur_number, next_number);
            break;
        default:
            raler(0, "Opérateur inconnu");
        }
        current_arg += 2;
    }

    // On ferme les côtés d'écriture et de lecture (car on a fini de traiter
    // argv)
    CHK(close(addpipe[1]));
    CHK(close(subpipe[1]));
    CHK(close(divpipe[1]));
    CHK(close(mulpipe[1]));
    CHK(close(respipe[1]));

    CHK(close(addpipe[0]));
    CHK(close(subpipe[0]));
    CHK(close(divpipe[0]));
    CHK(close(mulpipe[0]));
    // On ne ferme pas respipe[0] car on va encore
    // lire dedans le resultat final

    // On attend que les 4 processus fils terminent
    waitson(4);

    // On print le résultat final
    int result;
    CHK(read(respipe[0], &result, sizeof(int)));
    printf("%d\n", result);

    // On ferme enfin le tube de lecture
    CHK(close(respipe[0]));

    return 0;
}
