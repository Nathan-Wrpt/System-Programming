#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/stat.h>
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

void concatener(char *result, int size, char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    int n = vsnprintf(result, size, format, ap);
    if (n < 0 || n >= size)
        raler(0, "snprintf %s", result);

    va_end(ap);

    return;
}

// Fonction qui remplace les caractères interdits dans les noms de répertoires
// par des _ (Pas utile pour passer les tests, mais évite des problèmes)
void sanitize_filename(char *filename)
{
    for (int i = 0; filename[i] != '\0'; i++) {
        if (filename[i] == '/' || filename[i] == '\\' || filename[i] == ':' ||
            filename[i] == '*' || filename[i] == '?' || filename[i] == '"' ||
            filename[i] == '<' || filename[i] == '>' || filename[i] == '|') {
            filename[i] = '_';
        }
    }
}

void cacher(const char *filename)
{
    // On ouvre le fichier pour le lire
    int fd;
    CHK(fd = open(filename, O_RDONLY));

    // Buffer qui servira à stocker les noms de répertoires
    char buf[NAME_MAX + 1];

    ssize_t n;

    // Chemin du répertoire caché (<filename>.d)
    char path[PATH_MAX];
    concatener(path, sizeof(path), "%s.d", filename);

    // On crée le répertoire caché initial
    CHK(mkdir(path, 0777));

    // Compteur pour les répertoires créés au sein d'un sous répertoire de
    // premier niveau
    int i = 0;

    // Variable qui contient la valeur du sous-répertoire de premier niveau
    // courant
    int cur = 0;

    // string qui contiendra le chemin du dernier répertoire créé
    char lastpathused[PATH_MAX];

    // On lit le fichier et on crée un répertoire pour chaque 255 caractères lus
    while ((n = read(fd, buf, NAME_MAX)) > 0) {
        buf[n] = '\0';

        // On remplace les caractères interdits par des _
        sanitize_filename(buf);

        // On crée le sous-répertoire de premier niveau si i == 0
        if (i == 0) {
            // Création du chemin du sous-répertoire de premier niveau
            char pathfirstlevel[PATH_MAX];
            concatener(pathfirstlevel, PATH_MAX, "%s/%d", path, cur);

            // On crée le sous-répertoire de premier niveau
            CHK(mkdir(pathfirstlevel, 0777));

            // On met ce chemin dans lastpathused pour pouvoir le concaténer
            // avec le nom du répertoire suivant
            concatener(lastpathused, PATH_MAX, "%s", pathfirstlevel);
        }

        // On concatène le dernier chemin avec le buffer pour obtenir le chemin
        // du répertoire à créer
        char curpath[PATH_MAX];
        concatener(curpath, PATH_MAX, "%s/%s", lastpathused, buf);

        // On crée le répertoire
        CHK(mkdir(curpath, 0777));

        // On met à jour lastpathused
        concatener(lastpathused, PATH_MAX, "%s", curpath);

        // On met à jour le compteur de répertoires
        i++;

        // Si on a créé 3 répertoires, on incrémente le sous-répertoire de
        // premier niveau courant et réinitialise le compteur
        if (i == 3) {
            cur++;
            i = 0;
        }
    }

    CHK(close(fd));
}

// Fonction auxiliaire de extraire qui itère sur tous les sous-répertoires d'un
// sous-répertoire de premier niveau donné et écrit leur nom dans le fichier de
// sortie
void extrairepart(char *dirname, char *output)
{
    DIR *dir;

    struct dirent *entry;

    // Création du chemin qui servira à rappeler récursivement la fonction sur
    // dirname/entry->d_name
    char path[PATH_MAX];

    // On ouvre le répertoire et check si il est bien ouvert
    dir = opendir(dirname);

    if (dir == NULL) {
        raler(1, "opendir %s", dirname);
    }
    // On parcourt le répertoire jusqu'à trouver un autre répertoire
    while ((entry = readdir(dir)) != NULL) {

        // On ignore les répertoires . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // On crée le chemin complet du nouveau potentiel répertoire
        concatener(path, sizeof(path), "%s/%s", dirname, entry->d_name);

        // On vérifie si le "fichier" est un répertoire
        struct stat st;
        CHK(stat(path, &st));
        if (S_ISDIR(st.st_mode)) {

            // On écrit le nom du répertoire dans le fichier de sortie (et le
            // crée s'il n'existe pas)
            int fd;
            CHK(fd = open(output, O_WRONLY | O_CREAT | O_APPEND, 0666));
            CHK(write(fd, entry->d_name, strlen(entry->d_name)));
            CHK(close(fd));

            // On rappelle la fonction sur le nouveau répertoire
            extrairepart(path, output);
            break;
        } else {
            // Si le "fichier" n'est pas un répertoire, on ne fait rien, on
            // passe au prochain fichier
            continue;
        }
    }
    CHK(closedir(dir));
}

void extraire(char *dirname, char *output)
{
    int count = 0;
    char path[PATH_MAX];

    // On crée le fichier de sortie et on le remet à zéro s'il existe déjà
    int fd;
    CHK(fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0666));
    CHK(close(fd));

    // Boucle jusqu'à ce que opendir échoue (donc quand il n'y a plus de
    // sous-répertoire de premier niveau à traiter)
    while (1) {
        // Construction du chemin du répertoire courant
        concatener(path, sizeof(path), "%s/%d", dirname, count);

        // Vérifie si le répertoire existe
        DIR *dir = opendir(path);
        if (dir == NULL) {
            // Si opendir échoue, il n'y a plus de répertoire à traiter
            break;
        }
        CHK(closedir(dir));

        // On appelle extrairepart sur le sous-répertoire de premier niveau
        // courant
        extrairepart(path, output);
        count++;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4)
        raler(0, "usage: %s {-c filename | -x dirname output}", argv[0]);
    int bcacher = 0;
    int extra = 0;
    if (argc == 3) {
        if (strcmp(argv[1], "-c") == 0) {
            bcacher = 1;
        } else {
            raler(0, "usage: %s {-c filename | -x dirname output}", argv[0]);
        }
    }
    if (argc == 4) {
        if (strcmp(argv[1], "-x") == 0) {
            extra = 1;
        } else {
            raler(0, "usage: %s {-c filename | -x dirname output}", argv[0]);
        }
    }
    if (bcacher) {
        cacher(argv[2]);
    } else if (extra) {
        extraire(argv[2], argv[3]);
    }

    return 0;
}