# Aspects sécurité — robustesse du décompresseur

Un décompresseur est un **parseur d'entrée non fiable** : il lit un fichier produit par un tiers (potentiellement malveillant) et reconstruit des structures en mémoire à partir de son contenu. C'est une surface d'attaque classique. Ce document recense les risques identifiés sur `unhcompress`, suivant une démarche **problème → risque → correctif**, avec les tests qui les mettent en évidence.

Toutes les commandes ci-dessous se lancent depuis la racine du projet, après `make`.

---

## 1. Arbre dégénéré → boucle infinie (déni de service / disque plein)

**C'est la vulnérabilité la plus sérieuse trouvée sur ce projet, et elle est corrigée.**

### Problème

Le décodage des données part de la racine de l'arbre et descend à gauche/droite selon chaque bit lu, jusqu'à atteindre une feuille. Si l'arbre reconstruit est réduit à **une seule feuille de données** (non-CASPER), la racine est cette feuille : on n'a alors **aucun nœud interne à traverser**, donc **aucun bit n'est lu** — et on réémet le même caractère indéfiniment, sans jamais progresser vers la fin du flux ni atteindre le marqueur CASPER.

### Risque

Un fichier malformé de **2 octets seulement** suffit à déclencher une boucle infinie qui écrit sans fin sur le disque. C'est une forme extrême de *decompression bomb* : amplification quasi infinie (2 octets → sortie illimitée), entraînant un **déni de service** (CPU à 100 %, processus jamais terminé) et un **remplissage du disque**.

Démonstration (sur la version vulnérable, l'octet `0x90 0x40` code « feuille → caractère ‘A’ ») :

```
$ printf '\x90\x40' > bomb.bin       # 2 octets
$ ./unhcompress_test/unhcompress bomb.bin out.txt
# ne se termine jamais ; out.txt grossit indéfiniment
# (mesuré : ~450 Mo écrits en 3 secondes avant interruption)
```

### Correctif

Un arbre Huffman légitime produit par `hcompress` a **toujours** soit une racine interne (au moins un caractère de données + la feuille CASPER, donc ≥ 2 feuilles), soit une unique feuille **CASPER** dans le cas d'un fichier source vide. Une racine réduite à une feuille de **données** est donc nécessairement malformée et est désormais rejetée explicitement, avant tout décodage :

```c
if (table_ascii[iroot].mark == LEAF) {     /* feuille de donnees, pas CASPER */
  fprintf(stderr, "malformed Huffman tree: lone data leaf\n");
  return -1;
}
```

Pourquoi ce seul test suffit à éliminer toute boucle infinie : la seule façon de boucler sans consommer de bit est de ne traverser aucun nœud interne, c'est-à-dire d'avoir une racine-feuille. Dès que la racine est un nœud, chaque tour de boucle lit au moins un bit et progresse donc vers l'EOF, qui est détecté et arrête le décodage.

Après correctif :

```
$ printf '\x90\x40' > bomb.bin
$ ./unhcompress_test/unhcompress bomb.bin out.txt
malformed Huffman tree: lone data leaf       # arrêt immédiat, code de sortie 1
```

Le cas légitime du fichier vide (racine = feuille CASPER) reste accepté et restitue bien 0 octet.

---

## 2. Arbre tronqué ou jamais terminé

### Problème

La reconstruction de l'arbre (`read_tree_rec`) est récursive : chaque nœud interne attend deux sous-arbres. Un fichier qui n'annonce que des nœuds internes (`0` à répétition) décrit un arbre qui n'est jamais complet.

### Risque

Deux dangers : un **débordement du tableau** de nœuds (écriture hors limites), et une **récursion non bornée** (débordement de pile).

### Correctif (déjà présent dans la conception)

Le tableau a une taille fixe `SIZE = (UCHAR_MAX + 1) * 2 + 1 = 513`, qui est le nombre maximal de nœuds d'un arbre Huffman sur 257 symboles. La reconstruction est gardée par :

```c
if (next_index >= SIZE) {
  return -1;        /* trop de noeuds : entree malformee */
}
```

Comme chaque appel récursif consomme une case du tableau, ce garde borne **à la fois** la taille du tableau et la profondeur de récursion. Vérification :

```
$ head -c 1000 /dev/zero > allzeros.bin   # que des bits 0 -> nœuds internes
$ ./unhcompress_test/unhcompress allzeros.bin out.txt
malformed or truncated Huffman tree         # rejeté proprement, code 1
```

---

## 3. EOF prématuré dans les données

### Problème

Après l'arbre vient le flux de codes. Un fichier peut s'arrêter en plein milieu d'un code, avant le marqueur de fin CASPER.

### Risque

Lecture au-delà de la fin du fichier, ou attente infinie d'un marqueur qui n'arrive jamais.

### Correctif (déjà présent)

`read_bit` signale l'EOF en renvoyant `-1`, valeur propagée à chaque niveau :

```c
int bit = read_bit(br);
if (bit < 0) {
  fprintf(stderr, "unexpected EOF in data\n");
  return -1;
}
```

Vérification :

```
$ printf '\xff' > truncated.bin   # annonce une feuille, mais valeur incomplète
$ ./unhcompress_test/unhcompress truncated.bin out.txt
unexpected EOF in data                      # rejeté, code 1
```

---

## 4. Entrée totalement aléatoire

### Problème

Un fichier de contenu quelconque (ni produit par `hcompress`, ni structuré).

### Comportement attendu

Le décompresseur ne doit ni planter (pas de segfault), ni boucler, mais terminer avec un code d'erreur.

### Vérification

Avant le correctif du point 1, certaines entrées aléatoires reconstruisaient par hasard un arbre à racine-feuille et bouclaient (≈ 2 cas sur 3). Après correctif, sur 5 tirages de 256 octets aléatoires, les 5 se terminent proprement avec un code de sortie 1, sans boucle ni crash :

```
$ for i in $(seq 5); do head -c 256 /dev/urandom > r.bin; \
    timeout 5 ./unhcompress_test/unhcompress r.bin out.txt; echo "exit=$?"; done
exit=1
exit=1
exit=1
exit=1
exit=1
```

---

## 5. Propagation des erreurs jusqu'à l'appelant

### Problème

Initialement, `uncompress_stream` renvoyait `void` : impossible pour le `main` de savoir si la décompression avait réussi.

### Risque

Un script appelant ne peut pas distinguer un succès d'un échec, et pourrait traiter une sortie corrompue comme valide.

### Correctif

`uncompress_stream` renvoie désormais `int` (`0` = succès, `-1` = erreur), et `main` propage ce code via `EXIT_SUCCESS` / `EXIT_FAILURE`. Tout fichier malformé produit donc un code de sortie non nul exploitable en script.

---

## Pistes d'approfondissement

- **Fuzzing** : passer `unhcompress` sous `afl++` ou `libFuzzer` pour explorer automatiquement l'espace des entrées malformées et détecter d'éventuels cas non couverts.
- **Analyse mémoire** : exécuter le round-trip et les cas malformés sous `valgrind --leak-check=full` et compiler avec `-fsanitize=address,undefined` pour traquer fuites et comportements indéfinis.
- **Limite de taille de sortie** : imposer un plafond (ou un ratio maximal entrée/sortie) pour se prémunir contre d'autres formes de *decompression bombs*.
- **Contrôle d'intégrité** : stocker un checksum (CRC32) en tête de fichier pour détecter une corruption avant de produire une sortie erronée.
