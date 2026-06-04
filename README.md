# Compression et décompression à la Huffman

Implémentation complète en **C (C23 / `-std=c2x`)** d'un compresseur **et** d'un décompresseur fondés sur les arbres de Huffman, réalisée dans le cadre du TP n°6 d'**Algorithmique 3 — structures de données arborescentes** (Licence Informatique 2ᵉ année, Université de Rouen Normandie, 2025-2026).

L'objectif : produire un format binaire compact dans lequel les caractères les plus fréquents du fichier source reçoivent les codes les plus courts, et inversement.

**État du projet : compresseur et décompresseur fonctionnels.** Le round-trip (compression puis décompression) restitue l'original à l'octet près sur tous les cas testés, y compris un texte de 3 Mo et des fichiers binaires arbitraires. Le format produit est compatible avec l'exécutable de référence de l'UE (vérifié dans les deux sens).

---

## Sommaire

- [Principe](#principe)
- [Résultats](#résultats)
- [Architecture](#architecture)
- [Format du fichier compressé](#format-du-fichier-compressé)
- [Compilation](#compilation)
- [Utilisation](#utilisation)
- [Tests](#tests)
- [Ce que ce projet m'a appris](#ce-que-ce-projet-ma-appris)
- [Aspects sécurité](#aspects-sécurité)
- [Limites connues](#limites-connues)

---

## Principe

L'algorithme de Huffman attribue à chaque caractère un code binaire de longueur variable, choisi de façon que :

- aucun code ne soit préfixe d'un autre (préfixité  permet un décodage sans ambiguïté) ;
- la longueur moyenne des codes soit minimale par rapport aux fréquences des caractères.

Concrètement, on construit un **arbre binaire** dont les feuilles portent les caractères et dont chaque arête gauche/droite représente un bit `0`/`1`. Le code d'un caractère est le chemin de la racine jusqu'à sa feuille.

---

## Résultats

Mesures réelles obtenues avec `make test` :

| Fichier | Taille source | Taille compressée | Ratio |
|---|---:|---:|---:|
| `empty.txt` (vide) | 0 o | 2 o | — |
| `one.txt` (1 caractère) | 1 o | 3 o | — |
| `rep.txt` (10 000 × `A`) | 10 000 o | 1 253 o | 13 % |
| `lesmiserables.txt` (texte) | 3 088 841 o | 1 701 162 o | 55 % |
| `hcompress` (binaire) | — | — | round-trip OK |

Le ratio de ~55 % sur du texte ASCII correspond à ce qu'on attend d'un Huffman par octet (sans modélisation d'ordre supérieur).

---

## Architecture

```
Huffman/
├── huffman/
│   ├── hcompress/         # Bibliothèque de compression
│   │   ├── hcompress.h    #   Interface
│   │   └── hcompress.c    #   Implémentation
│   └── unhcompress/       # Bibliothèque de décompression
│       ├── unhcompress.h  #   Interface
│       └── unhcompress.c  #   Implémentation
│
├── hcompress_test/        # Exécutable de test (compression)
│   ├── main.c             #   CLI : SRC [DEST]
│   └── makefile
│
├── unhcompress_test/      # Exécutable de test (décompression)
│   ├── main.c             #   CLI : SRC [DEST]
│   └── makefile
│
├── tests/
│   ├── test.sh            # Suite de tests (round-trip + compat + ratios)
│   ├── lesmiserables.txt  # Jeu de test (~3 Mo)
│   ├── empty.txt          # Cas limite : fichier vide
│   ├── one.txt            # Cas limite : 1 caractère
│   ├── rep.txt            # Cas limite : caractère répété
│   └── reference/         # (optionnel, non versionné) binaires de l'UE
│
├── makefile               # Cibles all / test / clean / dist
├── algo3_huffman.pdf      # Sujet du TP
├── SECURITE.md            # Write-up : robustesse face aux entrées hostiles
├── LICENSE                # MIT
├── .gitignore
└── README.md
```

Les modules `huffman/hcompress/` et `huffman/unhcompress/` sont conçus comme des bibliothèques réutilisables. Chaque sous-dossier `*_test/` produit un exécutable autonome qui les lie à un `main` en ligne de commande.

---

## Format du fichier compressé

Le fichier compressé est un **flux de bits continu** structuré comme suit :

```
┌─────────────────────┬───────────────────────────────┬─────────────┬──────────┐
│ arbre (Łukasiewicz) │ codes des caractères du texte │ code CASPER │ padding  │
└─────────────────────┴───────────────────────────────┴─────────────┴──────────┘
```

1. **L'arbre est sérialisé en tête de fichier**, en notation de Łukasiewicz (préfixe) :
   - `0` → nœud interne, suivi des représentations récursives des fils gauche puis droit ;
   - `1` → feuille, suivi de la valeur du caractère sur `CHAR_BIT + 1` = **9 bits** (8 ne suffisent pas : il faut distinguer les 256 valeurs d'octet *plus* CASPER).
2. **Les codes des caractères** du texte source, émis bit à bit.
3. **Le code de CASPER**, marqueur de fin de message (voir ci-dessous).
4. **Le padding** : le dernier octet est complété par des zéros.

Placer l'arbre **en tête** permet au décodeur de le reconstruire d'abord, puis de décoder le flux dans la foulée, en une seule passe.

### Pourquoi un caractère fantôme (CASPER) ?

Comme le fichier compressé peut se terminer en plein milieu d'un octet (padding), il faut un moyen de signaler **la vraie fin** du message. `CASPER` est une 257ᵉ feuille de poids 1, qui obtient son propre code Huffman et qui est émise une seule fois, juste après le dernier caractère du source. Au décodage, dès qu'on rencontre ce code, on s'arrête — les bits de padding restants sont ignorés.

---

## Compilation

Le projet utilise `make` et **gcc en mode C23** avec un jeu d'avertissements stricts :

```makefile
CFLAGS = -std=c2x -Wall -Wconversion -Werror -Wextra -Wpedantic \
         -Wwrite-strings -O0 -g
```

`-Werror` transforme tout avertissement en erreur : **le projet compile sans le moindre warning** sous ce jeu de flags.

```bash
# Tout compiler depuis la racine
make

# Ou individuellement
cd hcompress_test && make      # produit hcompress
cd unhcompress_test && make    # produit unhcompress

# Nettoyage
make clean

# Archive de rendu
make dist                      # produit Huffman.tar.gz
```

---

## Utilisation

```bash
# Compresser
./hcompress_test/hcompress source.txt compressed.bin

# Décompresser
./unhcompress_test/unhcompress compressed.bin restored.txt

# Vérifier que le round-trip est exact
diff source.txt restored.txt   # ne doit rien afficher

# Aide
./hcompress_test/hcompress --help
```

Les deux programmes écrivent sur la sortie standard si aucun fichier de destination n'est fourni.

### Restriction connue

`compress_stream` refuse `stdin` comme entrée : l'algorithme nécessite **deux passes** sur le fichier (une pour les fréquences, une pour l'encodage), ce qui implique un `fseek` impossible sur un flux non-rembobinable.

---

## Tests

La suite de tests `tests/test.sh` (lançable via `make test`) vérifie :

1. **Round-trip** : pour chaque fichier, `hcompress` puis `unhcompress` doit redonner l'original à l'octet près (`cmp`). Couvre les cas limites (vide, 1 caractère, caractère répété), un texte de 3 Mo, et le binaire `hcompress` lui-même (qui contient les 256 valeurs d'octet possibles).
2. **Taux de compression** affiché pour chaque fichier.
3. **Compatibilité avec la référence** (optionnelle) : si les binaires de l'UE sont placés dans `tests/reference/`, le script vérifie dans les deux sens que les formats sont interopérables.

```bash
make test
```

> Les binaires de référence fournis par l'UE ne sont pas versionnés (ils ne sont pas de mon code et sont spécifiques à une architecture). Pour activer les tests de compatibilité, copie-les dans `tests/reference/`.

---

## Ce que ce projet m'a appris

- **Manipulation de bits en C.** Émettre et lire un flux de bits qui ne s'aligne pas sur les octets (accumulation dans un buffer, padding du dernier octet) est l'une des vraies difficultés du projet.
- **Sérialisation d'une structure arborescente.** La notation de Łukasiewicz permet d'écrire un arbre en un flux linéaire et de le reconstruire par une simple récursion.
- **Conception d'un format binaire auto-suffisant.** Le fichier compressé contient tout ce qu'il faut pour se décoder lui-même (l'arbre + le marqueur de fin CASPER).
- **Gestion mémoire manuelle rigoureuse.** L'arbre tient dans un tableau statique de 513 entrées ; seuls les codes par caractère sont alloués dynamiquement et libérés (`free_codes`).
- **Programmation défensive.** Le décodeur lit un fichier potentiellement non fiable : voir `SECURITE.md`.
- **Discipline de compilation.** Faire passer `-Wconversion -Werror -Wextra -Wpedantic` oblige à traiter chaque conversion implicite et chaque cas limite, plutôt que de les ignorer.

---

## Aspects sécurité

Un décompresseur lit un fichier **fourni par un tiers** et reconstruit des structures en mémoire à partir de son contenu : c'est une surface d'attaque classique (parsing d'entrée non fiable). Le fichier [`SECURITE.md`](SECURITE.md) documente une démarche problème → risque → correctif :

- bornage de la reconstruction de l'arbre pour éviter tout débordement de tableau ou récursion non maîtrisée ;
- détection d'un EOF prématuré (fichier tronqué) ;
- comportement face à un fichier vide, aléatoire, ou malformé ;
- réflexion sur les *decompression bombs*.

---

## Limites connues

- **Huffman par octet uniquement** : pas de modélisation d'ordre supérieur (digrammes, etc.), donc le ratio reste limité (~55 % sur du texte) comparé à des algorithmes modernes (LZ, arithmétique).
- **Deux passes obligatoires** : impossible de compresser depuis `stdin` (voir restriction ci-dessus).
- **Arbre limité à 257 feuilles** (256 octets + CASPER) : c'est suffisant pour des fichiers d'octets, mais le code ne gère pas d'alphabet plus large.
- **Pas de vérification d'intégrité** : aucun checksum n'est stocké, donc une corruption silencieuse du fichier compressé n'est pas détectée en tant que telle (elle produit une sortie erronée ou une erreur de décodage).

---

## Référence

Huffman, David A. *A Method for the Construction of Minimum-Redundancy Codes.* Proceedings of the IRE, 1952.

## Licence

Distribué sous licence **MIT**. Voir le fichier [`LICENSE`](LICENSE).
